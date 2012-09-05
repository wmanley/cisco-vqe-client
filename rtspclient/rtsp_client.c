/*
 *------------------------------------------------------------------
 * RTSP Client
 *
 * July 2006, Dong Hsu
 *
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */


#include <string.h>
#include <stdlib.h>
#include <utils/strl.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>    
#include <netdb.h>    
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <vam_time.h>

#include "rtsp_client.h"
#ifndef __VQEC__
#define __VQEC__
#endif
#include <vqec_debug.h>

static rtsp_client_t client;
static rtsp_ret_e ConnectServer(const char *host, int port);
static rtsp_ret_e SendRequest (const char *abs_path,
                               const char *accept_media_type);
static rtsp_ret_e RecvResponse (abs_time_t abort_time);
static int GetResponseLength (char *response);
static rtsp_ret_e ValidateResponse (char *response);
static int ReadLine (char* from_str, int first, int max, char* to_str);

/* Time allowed to establish TCP connection */
#define RTSP_CLIENT_TCP_CONNECT_TIMEOUT_SECS 5
/*
 * Timeout for reading received pkt(s) on TCP connection.
 *
 * Once the RTSP request has been sent, the RTSP client tries to read
 * the packets arriving from the server as the response.  The read calls
 * are blocking, so this defines the maximum amount of time to block upon
 * a read or else a failure is reported due to the inactivity on the 
 * connection.
 */
#define RTSP_CLIENT_RESPONSE_FRAG_RECV_TIMEOUT_SECS 5

/* sa_ignore DISABLE_RETURNS sscanf */
/* sa_ignore DISABLE_RETURNS fcntl */


/*
 * The following function is modified based on the code from the book of 
 * "Unix Network Programming", (Second Edition) Volume 1, by W. Richard 
 * Stevens. We would like to acknowledge Richard Stevens for contributing 
 * the code in this function.
 *
 */
static int connect_nonb (int sockfd, 
                         const struct sockaddr *saptr, 
                         socklen_t salen, 
                         int nsec)
{
    int		        flags, n, error;
    socklen_t		len;
    fd_set		rset, wset;
    struct timeval	tval;

    flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    error = 0;
    if ((n = connect(sockfd, (struct sockaddr *) saptr, salen)) < 0) {
        if (errno != EINPROGRESS) {
            return (-1);
        }
    }

    /* Do whatever we want while the connect is taking place. */
    
    if (n == 0) {
        goto done;	/* connect completed immediately */
    }

    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);
    wset = rset;
    tval.tv_sec = nsec;
    tval.tv_usec = 0;

    if ((n = select(sockfd+1, &rset, &wset, NULL,
                    nsec ? &tval : NULL)) == 0) {
        close(sockfd);		/* timeout */
        errno = ETIMEDOUT;
        return (-1);
    }

    if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset)) {
        len = sizeof(error);
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
            return (-1);			/* Solaris pending error */
        }
    } else {
        VQEC_LOG_ERROR("connect_nonb:: sockfd not set\n");
    }
    
done:
    fcntl(sockfd, F_SETFL, flags);      /* restore file status flags */

    if (error) {
        close(sockfd);		/* just in case */
        errno = error;
        return (-1);
    }

    return (0);
}



/* Function:    rtsp_client_init
 * Description: Initialize the RTSP client
 * Parameters:  name - RTSP server name
 *              port - RTSP server port
 * Returns:     Success or failed
 */
rtsp_ret_e rtsp_client_init (const char *name, int port)
{
    rtsp_ret_e ret = RTSP_FAILURE;

    if (name != NULL) {
        strncpy(client.serverHost, name, MAX_NAME_LENGTH);
        client.serverPort = port;

        ret = ConnectServer(name, port);
        client.cSeq = 0;
        client.response.code = 0;
        client.response.server = NULL;
        client.response.data_buffer = NULL;
        client.response.body = NULL;
    }
    else {
        VQEC_LOG_ERROR("rtsp_client_init:: RTSP server name "
                        "or IP address must be specified\n");
    }

    return ret;
}


/* Function:    rtsp_client_close
 * Description: Close the RTSP client
 * Parameters:  N/A
 * Returns:     Success or failed
 */
rtsp_ret_e rtsp_client_close (void)
{
    if (client.socket) {
        close(client.socket);
    }

    if (client.response.server) {
        free(client.response.server);
        client.response.server = NULL;
    }

    if (client.response.data_buffer) {
        free(client.response.data_buffer);
        client.response.data_buffer = NULL;
    }

    if (client.response.body) {
        free(client.response.body);
        client.response.body = NULL;
    }

    return RTSP_SUCCESS;
}


/* Function:    ConnectServer
 * Description: Open the socket connection to RTSP server
 * Parameters:  host - RTSP server name
 *              port - RTSP server port
 * Returns:     Success or failed
 */
#define ERRBUF_LEN 80
rtsp_ret_e ConnectServer (const char *host, int port)
{
    int socket_type = SOCK_STREAM;
    struct sockaddr_in server;
    struct hostent *hp;
    char errbuf[ERRBUF_LEN];

    memset(&server, 0, sizeof(server));

    if (isalpha(host[0])) {   /* server address is a name */
        hp = gethostbyname(host);
        if (hp == NULL ) {
            VQEC_LOG_ERROR("ConnectServer:: Host %s not found!\n", host);
            return RTSP_FAILURE;
        }

        memcpy(&(server.sin_addr), hp->h_addr, hp->h_length);
        server.sin_family = hp->h_addrtype;
    }
    else  { /* Simply just use it */
        if (inet_aton(host, &(server.sin_addr)) == 0) {
            VQEC_LOG_ERROR("ConnectServer:: Invalid address %s is used!\n",
                           host);
            return RTSP_FAILURE;
        }
        server.sin_family = AF_INET;
    }

    server.sin_port = htons(port);

    client.socket = socket(AF_INET, socket_type, 0); /* Open the socket */
    if (client.socket < 0) {
        VQEC_LOG_ERROR("ConnectServer:: Could not open the socket!\n");
        return RTSP_SOCKET_ERR;
    }

    if (connect_nonb(client.socket,
                     (struct sockaddr*)&server, 
                     sizeof(server), 
                     RTSP_CLIENT_TCP_CONNECT_TIMEOUT_SECS)
        == SOCKET_ERROR) {
        VQEC_LOG_ERROR("ConnectServer:: Could not connect to the server "
                       "%s! (connect error: %s)\n", host,
                       strerror_r(errno, errbuf, ERRBUF_LEN));
        return RTSP_CONNECT_ERR;
    }

    return RTSP_SUCCESS;
}


/* Function:    rtsp_send_request
 * Description: Send out an DESCRIBE request and process the response
 * Parameters:  abs_path              - absolute ptah for rtsp url
 *              accept_media_type     - media types to include in RTSP header
 *              abort_time            - time after which reqyest should be
 *                                      aborted
 * Returns:     Success or failed
 */
rtsp_ret_e rtsp_send_request (const char *abs_path,
                              const char *accept_media_type,
                              abs_time_t abort_time)
{
    rtsp_ret_e ret;

    if (!abs_path) {
        return RTSP_SEND_ERR;
    }
    ret = SendRequest(abs_path, accept_media_type);
    if (ret != RTSP_SUCCESS) {
        VQEC_LOG_ERROR("rtsp_send_request:: Failed sending DESCRIBE "
                        "request to %s!\n",
                        abs_path);
        return RTSP_SEND_ERR;
    }
    else {
        ret = RecvResponse(abort_time);
        if (ret != RTSP_SUCCESS) {
            VQEC_LOG_ERROR("rtsp_send_request:: Failed receiving "
                            "the response!\n");
            return RTSP_RESP_ERR;
        }
        else {
            ret = ValidateResponse(client.response.data_buffer);
        }
    }

    return ret;
}

#define RETURN_LEN 3
#define SEQ_LENGTH 20

/* Function:    SendRequest
 * Description: Send out the DESCRIBE message
 * Parameters:  abs_path - absolute path for rtsp url
 *              accept_media_type - accepted media for "Accept" field
 *                                  (may be NULL) 
 * Returns:     Success or failed
 */
rtsp_ret_e SendRequest (const char *abs_path,
                        const char *accept_media_type)
{ 
    rtsp_ret_e ret = RTSP_SUCCESS;
    int len, cur = 0;
    char nl[RETURN_LEN] = "\r\n";
    char reqString[MAX_BUFFER_LENGTH];
    char *send_p;
    char seqNum[SEQ_LENGTH];

    /* Construct the DESCRIBE message */
    snprintf(reqString, MAX_BUFFER_LENGTH, "DESCRIBE ");
    len = strlcat(reqString, "rtsp://", MAX_BUFFER_LENGTH);
    len = strlcat(reqString, client.serverHost, MAX_BUFFER_LENGTH);
    len = strlcat(reqString, "/", MAX_BUFFER_LENGTH);
    len = strlcat(reqString, abs_path, MAX_BUFFER_LENGTH);
    len = strlcat(reqString, " RTSP/1.0", MAX_BUFFER_LENGTH);
    len = strlcat(reqString, nl, MAX_BUFFER_LENGTH);
    snprintf(seqNum, SEQ_LENGTH, "%d", ++client.cSeq);
    len = strlcat(reqString, "CSeq: ", MAX_BUFFER_LENGTH);
    len = strlcat(reqString, seqNum, MAX_BUFFER_LENGTH);
    len = strlcat(reqString, nl, MAX_BUFFER_LENGTH);
    if (accept_media_type) {
        len = strlcat(reqString, "Accept: ", MAX_BUFFER_LENGTH);
        len = strlcat(reqString, accept_media_type, MAX_BUFFER_LENGTH);
        len = strlcat(reqString, nl, MAX_BUFFER_LENGTH);
    }
    len = strlcat(reqString, nl, MAX_BUFFER_LENGTH);
    len = strlcat(reqString, "\0", MAX_BUFFER_LENGTH);
    if (len >= MAX_BUFFER_LENGTH) {
        return RTSP_INTERNAL_ERR;
    }

    send_p = reqString;

    /* Clear the response data in client */
    client.response.code = 0;
    if (client.response.data_buffer) {
        free(client.response.data_buffer);
        client.response.data_buffer = NULL;
    }

    if (client.response.body) {
        free(client.response.body);
        client.response.body = NULL;
    }

    VQEC_DEBUG(VQEC_DEBUG_UPDATER,
               "SendRequest:: Sending request:\n\n%s\n", reqString);

    while (len > 0) {
        if ((cur = send(client.socket, send_p, len-cur, 0)) == SOCKET_ERROR) {
            VQEC_LOG_ERROR("SendRequest:: Socket error occured in "
                            "sending the request\n");
            ret = RTSP_SOCKET_ERR;
            break;
        }
        else {
            send_p += cur; 
            len -= cur;
        }
    }
  
    return ret;
}


/* Function:    RecvResponse
 * Description: Receive the response to the DESCRIBE request
 * Parameters:  abort_time  - time after which receive should be aborted
 * Returns:     Success or failed
 */
rtsp_ret_e RecvResponse (abs_time_t abort_time)
{
    int len;
    char resp[MAX_BUFFER_LENGTH];
    int total_length;
    int acc_len;
    int req_len = MAX_BUFFER_LENGTH;
    int rv;
    struct timeval timeout;

    /*
     * The socket is blocking and consequently the recv syscall will block until
     * a TCP fragment is received or a timeout is reached. If the timeout
     * is reached the method will immediately free the response data buffer
     * and return declaring failure. The timeout value is arbitrarily chosen. 
     * The socket will return with EAGAIN when there is a timeout. 
     *
     * It is also possible that there are multiple TCP fragments and while no
     * timeouts are seen, an update continues to be active. Thus the total
     * time for receiving a complete response has been bounded as well.
     */ 
    timeout.tv_sec = RTSP_CLIENT_RESPONSE_FRAG_RECV_TIMEOUT_SECS;
    timeout.tv_usec = 0;
    rv = setsockopt(client.socket, SOL_SOCKET, 
                    SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (rv) {
        VQEC_LOG_ERROR("RecvResponse:: Error in setting socket option \n");
        return RTSP_SOCKET_ERR;
    }
    
    bzero(resp, MAX_BUFFER_LENGTH);
    /* Peek the message first to figure out the content length */
    if ((len = recv(client.socket, resp, MAX_BUFFER_LENGTH, MSG_PEEK))
        != SOCKET_ERROR) {
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "RecvResponse:: Received the first byte ...\n");
        total_length = GetResponseLength(resp);
        req_len += total_length;

        /* Check the size of the request */
        if (req_len > MAX_BUFFER_LENGTH * MAX_CHANNELS) {
            VQEC_LOG_ERROR("RecvResponse:: Ask too much memory "
                            "in receiving the response\n");
            return RTSP_MALLOC_ERR;
        }

        /* Allocate the memory for response data */
        client.response.data_buffer = malloc(sizeof(char) * req_len);
        if (client.response.data_buffer == NULL) {
            VQEC_LOG_ERROR("RecvResponse:: Memory allocation error occured "
                            "in receiving the response\n");
            return RTSP_MALLOC_ERR;
        }

        /* Reset the memory */
        memset(client.response.data_buffer, 0, req_len);

        bzero(resp, MAX_BUFFER_LENGTH);
        acc_len = 0;
        
        while ((len = recv(client.socket, resp, MAX_BUFFER_LENGTH, 0))) {

            if (len <= 0) {

                /*
                 * EAGAIN is nominally returned when there is a timeout or the
                 * socket has been closed - abort reception in that case.
                 */
                if (errno == ECONNRESET || errno == EAGAIN) {
                    VQEC_LOG_ERROR("RecvResponse:: Socket error occured "
                                   "in receiving the response (ECONNRESET / EAGAIN)\n");
                    free(client.response.data_buffer);
                    client.response.data_buffer = NULL;        
                    return RTSP_SOCKET_ERR;
                }
            }
            else {
                strncat(client.response.data_buffer, resp, len);
                acc_len += len;
                if (acc_len > total_length) {
                    break;
                }
                VQEC_DEBUG(VQEC_DEBUG_UPDATER, 
                           "Received fragment %d/%d/%d\n", 
                           len, acc_len, total_length);

                bzero(resp, MAX_BUFFER_LENGTH);
            }
            
            if (TIME_CMP_A(gt, get_sys_time(), abort_time)) {
                VQEC_LOG_ERROR("RecvResponse:: maximum timeout exceeded\n");
                free(client.response.data_buffer);
                client.response.data_buffer = NULL;
                return RTSP_SOCKET_ERR;                
            }
        }

        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "RecvResponse:: Received %d bytes ...\n", acc_len);

        return RTSP_SUCCESS;
    }
    else {
        VQEC_LOG_ERROR("RecvResponse:: Socket error occured in receiving "
                        "the response\n");
        return RTSP_SOCKET_ERR;
    }	
}

#define STRING_LEN 30

/* Function:    GetResponseLength
 * Description: Get the content length of response message
 * Parameters:  response - response message
 * Returns:     Success or failed
 */
int GetResponseLength (char* response)
{
    int cur, len;
    int resp_len;
    char buffer[MAX_LINE_SIZE];
    char *resp;
    char string_buffer[STRING_LEN];
    int response_code = 0;
    int cSeq;
    int length = 0;

    /* Figure out the status of response first */
    cur = 0;
    resp_len = strlen(response);
    resp = malloc(sizeof(char) * resp_len);
    if (resp == NULL) {
        VQEC_LOG_ERROR("GetResponseLength:: Failed to allocate memory\n");
        return 0;
    }

    strncpy(resp, response, resp_len);
    if ((len = ReadLine(resp, cur, resp_len, buffer)) != -1) {
        if (strncasecmp(buffer, "RTSP/1.0", 8) == 0) {
            /* Extract the response code */
            sscanf(buffer, "%20s %d",
                   string_buffer, &response_code);
        }
        else {
            VQEC_LOG_ERROR("GetResponseLength:: Invalid RTSP response:"
                            "\n\n%s\n",
                            response);
            free(resp);
            return 0;
        }
    }

    /* Process the response header */
    if (response_code == MSG_200_OK) {
        cur = len; 
        while ((len = ReadLine(resp, cur, resp_len, buffer)) != -1) {
            /* Check the cSeq number */
            if (strncasecmp(buffer, "CSeq:", 5) == 0) {
                /* Extract the c-sequence number */
                sscanf(buffer, "%20s %d",
                       string_buffer, &cSeq);
            
                if (cSeq != client.cSeq) {
                    VQEC_LOG_ERROR("GetResponseLength:: Mismatch CSeq %d "
                                    "in response\n",
                                    cSeq);
                    free(resp);
                    return 0;
                }

                cur += len;
            }
            else {
                /* Check for content type */
                if (strncasecmp(buffer, "Content-Type:", 13) == 0) {
                    /* Extract the content type */
                    char type[STRING_LEN];
                    sscanf(buffer, "%20s %30s",
                           string_buffer, type);

                    if ((strcmp(type, MEDIA_TYPE_APP_SDP) != 0) &&
                        (strcmp(type, MEDIA_TYPE_APP_PLAIN_TEXT) != 0)) {
                        VQEC_LOG_ERROR("GetResponseLength:: Unknown "
                                        "content type %s\n",
                                        type);
                        free(resp);
                        return 0;
                    }

                    cur += len;
                }
                else {
                    /* Check for content length */
                    if (strncasecmp(buffer, "Content-Length:", 15) == 0) {
                        /* Extract the content type */
                        sscanf(buffer, "%20s %d",
                               string_buffer, &length);
                        
                        cur += len;
                    }
                    else {
                        /* If we hit the empty line, this is */
                        /* beginning of response body */
                        if (strlen(buffer) != 0) {
                            cur += len;
                        }
                        else {
                            free(resp);

                            /* Found the beginning of body */
                            /* We return the header length */
                            /* plus the content length */
                            return cur+length;
                        }
                    }
                }
            }
        }
    }
    else {
        VQEC_DEBUG(VQEC_DEBUG_UPDATER,
                   "GetResponseLength:: Not 200 OK response:\n\n%s\n",
                   response);

        free(resp);
        return 0;
    }

    free(resp);
    return 0;
}


/* Function:    ValidateResponse
 * Description: Validate the response message
 * Parameters:  response - response message
 * Returns:     Success or failed
 */
rtsp_ret_e ValidateResponse (char* response)
{
    int cur, len;
    int resp_len;
    char buffer[MAX_LINE_SIZE];
    char *resp;
    char string_buffer[STRING_LEN];
    int response_code = 0;
    int cSeq = 0;
    int length = 0;
    rtsp_ret_e err = RTSP_SUCCESS;

    VQEC_DEBUG(VQEC_DEBUG_UPDATER,
               "ValidateResponse:: Starting to validate "
               "the response ...\n");

    /* Figure out the status of response first */
    cur = 0;
    resp_len = strlen(response);
    resp = malloc(sizeof(char) * resp_len);
    if (resp == NULL) {
        VQEC_LOG_ERROR("ValidateResponse:: Failed to allocate memory\n");
        err = RTSP_MALLOC_ERR;
        goto done;
    }

    strncpy(resp, response, resp_len);
    if ((len = ReadLine(resp, cur, resp_len, buffer)) != -1) {
        if (strncasecmp(buffer, "RTSP/1.0", 8) == 0) {
            /* Extract the response code */
            sscanf(buffer, "%20s %d",
                   string_buffer, &response_code);

            client.response.code = response_code;
        }
        else {
            VQEC_LOG_ERROR("ValidateResponse:: Invalid RTSP response:"
                            "\n\n%s\n",
                            response);
            err = RTSP_INVALID_RESP;
            goto done;
        }
    }

    /* Process the response header */
    cur = len; 
    while ((len = ReadLine(resp, cur, resp_len, buffer)) != -1) {
        /* Check the cSeq number */
        if (strncasecmp(buffer, "CSeq:", 5) == 0) {
            /* Extract the c-sequence number */
            sscanf(buffer, "%20s %d",
                   string_buffer, &cSeq);

            if (cSeq != client.cSeq) {
                VQEC_LOG_ERROR("ValidateResponse:: Mismatch CSeq %d "
                                "in response\n",
                                cSeq);
                err = RTSP_INVALID_RESP;
                goto done;
            }

            cur += len;
        }
        else {
            /* 
             * Check for Server
             * We expect the format:  "Server: <version>"
             * Everything following "Server: " is considered to be the version
             */
            if (strncasecmp(buffer, "Server:", 7) == 0) {
                /* Allocate room for <version> plus a terminating NULL */
                client.response.server = malloc(sizeof(char) * (len - 6));
                memset(client.response.server, 0, sizeof(char) * (len - 6));
                if (!client.response.server) {
                    VQEC_LOG_ERROR("ValidateResponse:: Failed to allocate "
                                   "memory for server field\n");
                    err = RTSP_MALLOC_ERR;
                    goto done;
                }
                /* Copy the version, starting just after the whitespace */
                strncpy(client.response.server, buffer + 8, len - 8);
                
                cur += len;
            }
            else {
                /* Check for content type */
                if (strncasecmp(buffer, "Content-Type:", 13) == 0) {
                    /* Extract the content type */
                    char type[MAX_LINE_SIZE];
                    sscanf(buffer, "%20s %s",
                           string_buffer, type);
                    
                    if ((strcmp(type, MEDIA_TYPE_APP_SDP) != 0) &&
                        (strcmp(type, MEDIA_TYPE_APP_PLAIN_TEXT) != 0)) {
                        VQEC_LOG_ERROR("ValidateResponse:: Unknown "
                                       "content type %s\n",
                                       type);
                        err = RTSP_INVALID_RESP;
                        goto done;
                    }
                    
                    cur += len;
                }
                else {
                    /* Check for content length */
                    if (strncasecmp(buffer, "Content-Length:", 15) == 0) {
                        /* Extract the content type */
                        sscanf(buffer, "%20s %d",
                               string_buffer, &length);
                        
                        cur += len;
                    }
                    else {
                        /* If we hit the empty line, this is */
                        /* beginning of response body */
                        if (strlen(buffer) != 0) {
                            cur += len;
                        }
                        else {
                            /* Found the beginning of body */
                            break;
                        }
                    }
                }
            }
        }
    }

    /* Now, extract the body data */
    cur += len;
    if (response_code == MSG_200_OK && 
        cSeq == client.cSeq && length != 0) {
        if (length > MAX_BUFFER_LENGTH * MAX_CHANNELS) {
            VQEC_LOG_ERROR("ValidateResponse:: Ask to malloc too much "
                            "memory for response body\n");
            err = RTSP_MALLOC_ERR;
            goto done;
        }

        client.response.body = malloc(sizeof(char) * (length + 1));
        memset(client.response.body, 0, length+1);
        if (client.response.body == NULL) {
            VQEC_LOG_ERROR("ValidateResponse:: Failed to allocate "
                            "memory for response body\n");
            err = RTSP_MALLOC_ERR;
            goto done;
        }

        strncpy(client.response.body, &resp[cur], length);

        /* Make sure that it is null terminated */
        client.response.body[length-1] = '\n';
    }

    VQEC_DEBUG(VQEC_DEBUG_UPDATER,
               "ValidateResponse:: Done with validation "
               "of the response ...\n");
 done:
    if (resp) {
        free(resp);
    }
    if (err != RTSP_SUCCESS) {
        if (client.response.server) {
            free(client.response.server);
            client.response.server = NULL;
        }
    }
    return (err);
}


/* Function:    rtsp_get_response_code
 * Description: return the current response code for DESCRIBE request
 * Parameters:  
 * Returns:     response code
 */
int rtsp_get_response_code ()
{
    return client.response.code;
}


/*
 * Gets the "Server" field string from the response, if present.
 *
 * @param[out] char *  "Server" string contents, or
 *                     NULL if no server field was present
 */
char *rtsp_get_response_server ()
{
    return client.response.server;
}

/* Function:    rtsp_get_response_body
 * Description: return the current response body for DESCRIBE request
 * Parameters:  
 * Returns:     response body data
 */
char *rtsp_get_response_body ()
{
    return client.response.body;
}


/* Function:    ReadLine
 * Description: Parse out each line from response message
 * Parameters:  
 * Returns:     Success or failed
 */
int ReadLine (char* from_str, int first, int max, char* to_str)
{
    int res = 0;
    int count = 0;

    // We support "\r\n" and "\n" line termination
    while ((first + count < max) && (count < MAX_LINE_SIZE)) {
        if (from_str[first+count] == '\n') { 
            to_str[count] = '\0';
            count+=1;
            res = count;
            break;
        }
	
	if (from_str[first+count] == '\r') { 
            to_str[count] = '\0';
            count+=2;
            res = count;
            break;
        }

        to_str[count] = from_str[first+count];
        count++;
    }

    if (res == 0) {
        if (first + count == max) {
            res = -1; // from_str is empty, read more from e.g. the socket
        }
        else {
            VQEC_LOG_ERROR("ReadLine:: Very long lines received "
                            "from the server!\n");
            res = -2;
        }
    }

    return res;
}

