/*
 *------------------------------------------------------------------
 * RTSP Client
 *
 * July 2006, Dong Hsu
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef _RTSP_CLIENT_H_
#define _RTSP_CLIENT_H_

#include "../cfg/cfg_channel.h"
#include "utils/vam_types.h"

#define MAX_NAME_LENGTH 100
#define MAX_BUFFER_LENGTH 4096
#define MAX_LINE_SIZE 1024
#define SOCKET_ERROR -1
#define MSG_200_OK 200
#define MSG_404_NOT_FOUND 404
#define MSG_415_UNSUPPORTED_MEDIA_TYPE 415
#define MEDIA_TYPE_APP_PLAIN_TEXT "application/plain-text"
#define MEDIA_TYPE_APP_SDP        "application/sdp"

/*! \enum rtsp_ret_e
    \brief Return value from rtsp_client api calls.
*/
typedef enum {
    RTSP_SUCCESS,        /**< success */
    RTSP_FAILURE,        /**< generic failure */
    RTSP_SOCKET_ERR,     /**< socket error */
    RTSP_CONNECT_ERR,    /**< failed to connect to the server */
    RTSP_SEND_ERR,       /**< DESCRIBE request failed */
    RTSP_RESP_ERR,       /**< error processing the response to DESCRIBE request */
    RTSP_INVALID_RESP,   /**< invalid RTSP response */
    RTSP_MALLOC_ERR,     /**< memory allocation failed */
    RTSP_INTERNAL_ERR    /**< internal error */
} rtsp_ret_e;


/*! \struct rtsp_response_t
    \brief RTSP response message
*/
typedef struct rtsp_response_ {
    int         code;
    char        *server;
    char        *data_buffer;
    char        *body;
} rtsp_response_t;


/*! \struct rtsp_client_t
    \brief RTSP client structure
*/
typedef struct rtsp_client_ {
    char                serverHost[MAX_NAME_LENGTH]; // DNS name of RTSP server
    int                 serverPort;
    int                 socket;

    uint32_t            cSeq;

    rtsp_response_t     response;
} rtsp_client_t;


/*! \fn rtsp_ret_e rtsp_client_init(const char *server_name, int port)
    \brief Initialize the rtsp client.

    \param server_name The DNS name of RTSP server.
*/
extern rtsp_ret_e rtsp_client_init(const char *server_name, int port);


/*! \fn rtsp_ret_e rtsp_client_close()
    \brief Clean up the rtsp client.
*/
extern rtsp_ret_e rtsp_client_close(void);


/*! \fn rtsp_ret_e rtsp_send_request(const char *abs_path,
                                     const char *accept_media_type,
                                     abs_time_t abort_time)
    \brief Send out a DESCRIBE request based on absoulte path.
*/
extern rtsp_ret_e rtsp_send_request(const char *abs_path,
                                    const char *accept_media_type,
                                    abs_time_t abort_time);

/*! \fn int rtsp_get_response_code(void)
    \brief Get the response code
*/
extern int rtsp_get_response_code(void);

/*
 * Gets the "Server" field string from the response, if present.
 *
 * @param[out] char *  "Server" string contents, or
 *                     NULL if no server field was present
 */
char *rtsp_get_response_server(void);

/*! \fn int rtsp_get_response_body(void)
    \brief Get the response body data
*/
extern char *rtsp_get_response_body(void);

#endif /* _RTSP_CLIENT_H_ */
