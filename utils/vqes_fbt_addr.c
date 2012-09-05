/**-----------------------------------------------------------------
 * @brief
 * VAM Cache Manager.  Feedback target address utility functions
 *
 * @file
 * vqe_fbt_addr.c
 *
 * June 2007, Anne McCormick
 * July 2008, Donghai Ma (moved out of CM(DP))
 *
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#define _GNU_SOURCE
#include <features.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <asm/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>

#include "vqes_fbt_addr.h"
#include "vam_util.h"
#include "log/vqes_fbt_addr_syslog_def.h"

/**
 * @defgroup VQE FBT Utilities
 * @{*/

/**@brief
 * Maximum netlink message payload, in bytes
 */
#define VQE_NL_MSG_MAX_PAYLOAD            1024

/**@brief
 * Name string of loopback interface. All FBT addresses are associated 
 * with loopback interface 
 */
#define VQE_LOOPBACK_IFNAME               "lo"

/**@brief
 * Label to use when adding addresses. This is necessary so that the flush
 * command can remove only addresses that were added by VQES
 */
#define VQE_LOOPBACK_LABEL                "lo:vqes"

/**@brief
 * Prefix length of netlink addresses
 */
#define VQE_NL_ADDR_PREFIXLEN             32

/**@brief
 * Minimum netlink socket send/recv timeout
 */
#define VQE_NL_MSG_SNDRCV_MIN_TMO_MSECS   2
#define VQE_NL_MSG_SNDRCV_MIN_TMO      (MSECS(VQE_NL_MSG_SNDRCV_MIN_TMO_MSECS))

/**@brief
 * Maximum netlink request time allowed
 */
#define VQE_NL_MSG_MAX_REQ_TIME_MSECS     1000
#define VQE_NL_MSG_MAX_REQ_TIME        (MSECS(VQE_NL_MSG_MAX_REQ_TIME_MSECS))

/**@brief
 * Return values from system() while checking FBT indicate presence/absence
 * of a fbt in the given scope. 
 * @note
 * Running bash commands via the system() call is a little funky. Boolean 
 * values from TRUE and FALSE correspond to 0 and 256 respectively.
 */
#define VQE_ADDR_ABSENT  0
#define VQE_ADDR_PRESENT 256


#define VQE_SYS_CMD_LEN                   256


/**
 * vqes_fbt_ret_t
 * @brief
 * Return codes for the internal VQE FBT utility functions
 */
typedef enum vqes_fbt_ret_ {
    VQE_FBT_OK = 0,
    VQE_FBT_RETRY,
    VQE_FBT_FAILED,
} vqes_fbt_ret_t;


/**
 * vqe_nl_req_t
 * @brief
 * Netlink interface address request structure
 */
typedef struct vqe_nl_req_ {
    struct nlmsghdr nl_hdr;            /* netlink header */
    struct ifaddrmsg ifaddr_msg;       /* interface address header */
    char data[VQE_NL_MSG_MAX_PAYLOAD];  /* payload data */
} vqe_nl_req_t;

/**
 * vqe_nl_rsp_t
 * @brief
 * Netlink interface address response structure
 * @note
 * Depending on the type of request, the response may contain either an
 * ifaddrmsg or an nlmsgerr. For an add or delete address request, the 
 * response always contains an nlmsgerr where the error field indicates
 * success or failure (error of zero means success). For a status request,
 * a successful response will contain an ifaddrmsg with status info, while
 * a failure will contain an nlmsgerr indicating the error.
 */
typedef struct vqe_nl_rsp_t {
    struct nlmsghdr nl_hdr;            /* netlink header */
    union {
        struct ifaddrmsg ifaddr_msg;   /* interface address header */
        struct nlmsgerr nack_hdr;      /* NACK header (error) */
    } status;
    char data[VQE_NL_MSG_MAX_PAYLOAD];  /* payload data */
} vqe_nl_rsp_t;

/**
 * vqe_check_nl_response_criteria_t
 * @brief
 * A callback function used to check a netlink response against different
 * criteria, depending on the type of request that was made.
 * @return
 * - VQE_FBT_OK for a successful response
 * - VQE_FBT_RETRY to retry the request
 * - VQE_FBT_FAILED for a failed response (no retry)
 */
typedef vqes_fbt_ret_t (*vqe_check_nl_response_criteria_t)(vqe_nl_req_t * reg,
                                                          vqe_nl_rsp_t * rsp);

/* Global data */
static int nl_sock = -1;
static uint32_t nl_msg_seq = 0;
static pthread_mutex_t nl_mutex;

/* VQE FBT counters */
static vqes_fbt_cnts_t fa_counters;
#define VQES_FA_CNT(name)                       \
    (fa_counters.name++)
 
/**
 * vqe_nl_sock_set_tmo
 * @brief
 * @param[out] cnts - 
 * @return
 *
 */        
void 
vqes_fbt_get_cnts (vqes_fbt_cnts_t *cnts)
{
    if (!cnts)
        return;

    /* Grab global lock */
    if (pthread_mutex_lock(&nl_mutex) != 0) {
        ASSERT_FATAL(0, CP_INTERNAL_FORCED_EXIT, 
                     "acquiring netlink mutex");
    }

    memcpy(cnts, &fa_counters, sizeof(vqes_fbt_cnts_t));

    /* Release global lock */
    if (pthread_mutex_unlock(&nl_mutex) != 0) {
        ASSERT_FATAL(0, CP_INTERNAL_FORCED_EXIT, "releasing netlink mutex");
    }    
}


const char *
vqes_fbt_addr_scope_to_str (int scope)
{
    switch (scope) {
    case RT_SCOPE_UNIVERSE:
        return "global";
    case RT_SCOPE_HOST:
        return "host";
    case RT_SCOPE_SITE:
        return "site";
    case RT_SCOPE_LINK:
        return "link";
    case RT_SCOPE_NOWHERE:
        return "nowhere";
    default:
        return "undefined scope";
    }
}

const char *
vqes_fbt_addr_status_to_str (vqes_fbt_status_t status)
{
    switch (status) {
    case VQE_FBT_ADDR_MISSING:
        return "address not present";
    case VQE_FBT_ADDR_GLOBAL:
        return "address present (global scope)";
    case VQE_FBT_ADDR_HOST:
        return "address present (local scope)";
    default:
        return "undefined";
    }
}



/**
 * vqe_nl_sock_set_tmo
 * @brief
 * Sets either the SO_SNDTIMEO or SO_RCVTIMEO setting for the global 
 * netlink socket. If socket operation times out before data is 
 * sent/received, errno  will be set to EAGAIN or EWOULDBLOCK.
 * @param[in] sock_tmo_opt - Either SO_SNDTIMEO or SO_RCVTIMEO
 * @param[in] tmo - Desired timeout
 * @return
 * - TRUE if tmo was set
 * - FALSE otherwise
 * @note Assumes global netlink lock is held
 */
static boolean
vqe_nl_sock_set_tmo (int sock_tmo_opt, rel_time_t tmo) 
{
    boolean retval = TRUE;
    struct timeval tm = TIME_GET_R(timeval, tmo);

    if ((sock_tmo_opt != SO_SNDTIMEO) && (sock_tmo_opt != SO_RCVTIMEO)) 
        return FALSE;

    if (setsockopt(nl_sock, SOL_SOCKET, sock_tmo_opt, &tm, sizeof(tm)) < 0) {
        switch (sock_tmo_opt) {
        case SO_SNDTIMEO:
            syslog_print(VQE_FBT_GEN_ERROR, 
                         "setting send timeout for netlink socket");
            VQES_FA_CNT(fbt_nl_send_tmo_reset_failed);
            break;
            
        case SO_RCVTIMEO:
            syslog_print(VQE_FBT_GEN_ERROR, 
                         "setting receive timeout for netlink socket");
            VQES_FA_CNT(fbt_nl_recv_tmo_reset_failed);
            break;
        }
        retval = FALSE;
    }
    return retval;
}

/**
 * vqes_fbt_open_nl_sock
 * @brief
 * This function will open the global netlink socket, set send/recv timeout
 * values, and bind the socket
 * @return
 * - 0 on success
 * - -1 otherwise
 */
static int
vqes_fbt_open_nl_sock (void)
{
    struct sockaddr_nl nl_addr;
    int ret = 0;

    /* Grab global lock */
    if (pthread_mutex_lock(&nl_mutex) != 0) {
        ASSERT_FATAL(0, CP_INTERNAL_FORCED_EXIT, 
                     "acquiring netlink mutex");
    }
    
    if (nl_sock > 0) {
        /* Socket already exists, success */
        goto done;
    }

    /* Create global netlink socket */
    nl_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
    if (nl_sock == -1) {
        syslog_print(VQE_FBT_GEN_ERROR, "opening netlink socket");
        ret = -1;
        goto done;
    }

    /* Set send/recv timeout values */
    if (! vqe_nl_sock_set_tmo(SO_SNDTIMEO, VQE_NL_MSG_SNDRCV_MIN_TMO)) {
        ret = -1;
        goto done;
    }
    if (! vqe_nl_sock_set_tmo(SO_RCVTIMEO, VQE_NL_MSG_SNDRCV_MIN_TMO)) {
        ret = -1;
        goto done;
    }

    /* Bind netlink socket */
    memset(&nl_addr, 0, sizeof(nl_addr));
    nl_addr.nl_family = AF_NETLINK;
    nl_addr.nl_pid = getpid();  /* must be unique id */
    nl_addr.nl_groups = 0;      /* unicast */

    if (bind(nl_sock, (struct sockaddr *) &nl_addr, sizeof(nl_addr)) < 0) {
        syslog_print(VQE_FBT_GEN_ERROR, "binding netlink socket");
        ret = -1;
        goto done;
    }

done:
    /* If error encountered, clean up socket */
    if (ret != 0) {
        if (nl_sock != -1) {
            if (close(nl_sock) == -1) {
                syslog_print(VQE_FBT_GEN_ERROR,
                             "closing netlink socket");
            }
            nl_sock = -1;
        }
    }
    
    /* Release global lock */
    if (pthread_mutex_unlock(&nl_mutex) != 0) {
        ASSERT_FATAL(0, CP_INTERNAL_FORCED_EXIT, "releasing netlink mutex");
    }
    
    return ret;
}

/**
 * vqes_fbt_close_nl_sock
 * @brief
 * This function will close the global netlink socket
 * @return
 * - 0 on success
 * - -1 otherwise
 */
static int
vqes_fbt_close_nl_sock (void)
{
    int ret = 0;

    /* Grab global lock */
    if (pthread_mutex_lock(&nl_mutex) != 0) {
        ASSERT_FATAL(0, CP_INTERNAL_FORCED_EXIT, "acquiring netlink mutex");
    }

    /* Close global socket */
    if (nl_sock != -1) {
        if (close(nl_sock) == -1) {
            syslog_print(VQE_FBT_GEN_ERROR,
                         "closing netlink socket");
            ret = -1;
        }
        nl_sock = -1;
    }

    /* Release global lock */
    if (pthread_mutex_unlock(&nl_mutex) != 0) {
        ASSERT_FATAL(0, CP_INTERNAL_FORCED_EXIT, "releasing netlink mutex");
    }

    return ret;
}

/**
 * vqes_fbt_nl_init
 * @brief
 * This function initializes the FBT netlink request mechanism
 * @return
 * - 0 on success
 * - -1 otherwise
 */
int
vqes_fbt_nl_init (void)
{
    pthread_mutexattr_t attr;
    int ret = 0;
    
    memset(&fa_counters, 0, sizeof(vqes_fbt_cnts_t));

    if (pthread_mutexattr_init(&attr) != 0) {
        ASSERT_FATAL(0, CP_INTERNAL_FORCED_EXIT,
                     "initializing netlink mutex attribute");
    }
    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
        ASSERT_FATAL(0, CP_INTERNAL_FORCED_EXIT,
                     "setting netlink mutex attribute");
    }

    /* Create global lock */
    if (pthread_mutex_init(&nl_mutex, &attr) != 0) {
        ASSERT_FATAL(0, CP_INTERNAL_FORCED_EXIT, "creating netlink mutex");
    }

    /* Open netlink socket */
    ret = vqes_fbt_open_nl_sock();
    if (ret != 0) {
        if (pthread_mutex_destroy(&nl_mutex) != 0) {
            ASSERT_FATAL(0, CP_INTERNAL_FORCED_EXIT, 
                         "destroying netlink mutex");
        }
    }
    return ret;
}

/**
 * vqes_fbt_nl_deinit
 * @brief
 * This function de-initializes the FBT netlink request mechanism
 * @return
 * - 0 on success
 * - -1 otherwise
 */
int
vqes_fbt_nl_deinit (void)
{
    int ret;

    /* Close netlink socket */
    ret = vqes_fbt_close_nl_sock();

    /* Destroy global lock */
    if (pthread_mutex_destroy(&nl_mutex) != 0) {
        ASSERT_FATAL(0, CP_INTERNAL_FORCED_EXIT, "destroying netlink mutex");
    }

    return ret;
}

/**
 * vqe_ifaddr_req_fill_nlmsghdr
 * @brief
 * Utility function to fill in netlink header
 */
static inline void
vqe_ifaddr_req_fill_nlmsghdr (struct nlmsghdr * hdr, uint16_t req_type, 
                              uint16_t flags)
{
    hdr->nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    hdr->nlmsg_flags = flags;
    hdr->nlmsg_type = req_type;
    hdr->nlmsg_pid = getpid();
}

/**
 * vqe_ifaddr_req_fill_ifaddrmsg
 * @brief
 * Utility function to fill in interface address message
 */
static inline void
vqe_ifaddr_req_fill_ifaddrmsg (struct ifaddrmsg * msg, int scope)
{
    msg->ifa_family = AF_INET;
    msg->ifa_scope = scope;
    msg->ifa_prefixlen = VQE_NL_ADDR_PREFIXLEN;
    msg->ifa_index = (int) if_nametoindex(VQE_LOOPBACK_IFNAME);
}

/**
 * vqe_ifaddr_req_add_addr_attr
 * @brief
 * Utility function to add rtnetlink IFA_LOCAL attribute
 */
static inline uint32_t
vqe_ifaddr_req_add_addr_attr (struct rtattr * attr, struct in_addr addr)
{
    uint32_t addr_len = sizeof(addr.s_addr);
    attr->rta_type = IFA_LOCAL;
    attr->rta_len = RTA_LENGTH(addr_len);
    memcpy(RTA_DATA(attr), &addr.s_addr, addr_len);
    return RTA_LENGTH(addr_len);
}

/**
 * vqe_ifaddr_req_add_label_attr
 * @brief
 * Utility function to add rtnetlink IFA_LABEL attribute
 */
static inline uint32_t
vqe_ifaddr_req_add_label_attr (struct rtattr * attr, char * label)
{
    uint32_t label_len = strlen(label);
    char * attr_str;
    attr->rta_type = IFA_LABEL;
    attr->rta_len = RTA_LENGTH(label_len + 1);
    memcpy(RTA_DATA(attr), label, label_len);
    attr_str = (char *) RTA_DATA(attr);
    attr_str[label_len] = '\0';
    return RTA_LENGTH(label_len + 1);
}

/**
 * vqe_nl_recv_rsp
 * @brief
 * This function will attempt to receive a given netlink response, based on
 * sequence number of interest. If the desired response is successfully
 * received, it will be passed to a specific callback function to check the
 * response against criteria appropriate for the type of request made.
 * @param[in] req - Netlink request to match on
 * @param[out] rsp - Received netlink response
 * @param[in] check_rsp - Callback function to check validitiy of response
 *                        based on type of request
 * @param[in] now - Current time
 * @param[in] quitting_time - Time at which we should give up on receive
 *                            attempts
 * @param[in/out] recv_tmo - Socket receive timeout value
 * @return
 * - VQE_FBT_OK for a successful response
 * - VQE_FBT_RETRY to retry the request
 * - VQE_FBT_FAILED for a failed response (no retry)
 * @note Assumes global netlink lock is held
 */
static vqes_fbt_ret_t
vqe_nl_recv_rsp (vqe_nl_req_t * req, 
                 vqe_nl_rsp_t * rsp, 
                 vqe_check_nl_response_criteria_t check_rsp,
                 abs_time_t now, 
                 abs_time_t quitting_time,
                 rel_time_t * recv_tmo)
{
    int bytes;
    abs_time_t curr_time = now;
    vqes_fbt_ret_t err = VQE_FBT_FAILED;
    rel_time_t tmo = *recv_tmo;

    while (TIME_CMP_A(lt, curr_time, quitting_time)) {

        memset(rsp, 0, sizeof(*rsp));
        bytes = recv(nl_sock, rsp, VQE_NL_MSG_MAX_PAYLOAD, 0);
        if (bytes <= 0) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                /* We timed out on the receive, bump up the tmo */
                tmo = TIME_LSHIFT_R(tmo, 1);
                /* sa_ignore {if fail, use current tmo} IGNORE_RETURN */
                vqe_nl_sock_set_tmo(SO_RCVTIMEO, tmo);

                VQES_FA_CNT(fbt_nl_recv_failed_tmo);
            }
            else {
                VQES_FA_CNT(fbt_nl_recv_failed_other);
            }                
            err = VQE_FBT_RETRY;
            break;
        }

        /* Check for valid netlink header */
        if (! NLMSG_OK(&rsp->nl_hdr, bytes)) {
            /* Netlink header is garbled. Retry */
            err = VQE_FBT_RETRY;
            VQES_FA_CNT(fbt_nl_recv_invalid_hdr);
            break;
        }

        /* Check if sequence number of response matches request */
        if (rsp->nl_hdr.nlmsg_seq == req->nl_hdr.nlmsg_seq) {
            /* We got the right response, now check it against our
             * criteria 
             */
            err = check_rsp(req, rsp);
            break;
        }
        else {
            /* If we got here, this is an old response either with
             * a sequence number less than the request sequence 
             * number, or greater than the request sequence number
             * (which can happen due to a wrap of the global 
             * sequence number). We need to fall through and receive 
             * again. Update current time.
             */
            curr_time = get_sys_time();
            
            if (rsp->nl_hdr.nlmsg_seq < req->nl_hdr.nlmsg_seq) {
                VQES_FA_CNT(fbt_nl_recv_lower_seq);
            }
            else {
                VQES_FA_CNT(fbt_nl_recv_higher_seq);
            }
        }
    }

    /* Pass back (possibly) altered receive timeout */
    *recv_tmo = tmo;

    return err;
}

/**
 * vqe_nl_process_req
 * @brief
 * This function sends down a netlink request and processes the response
 * @param[in] req - Netlink request
 * @param[out] rsp - Received netlink response
 * @param[in] check_rsp - Callback function to check validitiy of response
 *                        based on type of request
 * @return
 * - 0 for successful response
 * - -1 otherwise (either not successful, or status of request 
 *                 could not be determined)
 */
static int
vqe_nl_process_req (vqe_nl_req_t * req, 
                    vqe_nl_rsp_t * rsp,
                    vqe_check_nl_response_criteria_t check_rsp) 
{
    int bytes = 0;
    vqes_fbt_ret_t err = VQE_FBT_FAILED;
    int ret = -1;
    rel_time_t send_tmo, recv_tmo;
    abs_time_t start_time, curr_time, quitting_time;

    start_time = curr_time = get_sys_time();
    quitting_time = TIME_ADD_A_R(start_time, VQE_NL_MSG_MAX_REQ_TIME);
    send_tmo = recv_tmo = VQE_NL_MSG_SNDRCV_MIN_TMO;

    /* Grab global lock */
    if (pthread_mutex_lock(&nl_mutex) != 0) {
        ASSERT_FATAL(0, CP_INTERNAL_FORCED_EXIT, "acquiring netlink mutex");
    }

    /* Make sure global socket is set up */
    if (nl_sock <= 0) {
        goto done;
    }

    while (TIME_CMP_A(lt, curr_time, quitting_time)) {

        /* Set request sequence number */
        req->nl_hdr.nlmsg_seq = ++nl_msg_seq;

        /* Send netlink request */
        bytes = send(nl_sock, req, req->nl_hdr.nlmsg_len, MSG_NOSIGNAL);
        if (bytes != req->nl_hdr.nlmsg_len) {

            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                /* We timed out on the send, bump up the tmo and try again */
                curr_time = get_sys_time();
                if (TIME_CMP_A(lt, TIME_ADD_A_R(curr_time, 
                                                TIME_LSHIFT_R(send_tmo, 1)), 
                               quitting_time)) {
                    send_tmo = TIME_LSHIFT_R(send_tmo, 1);
                    /* sa_ignore {if fail, use current tmo} IGNORE_RETURN */
                    vqe_nl_sock_set_tmo(SO_SNDTIMEO, send_tmo);
                }
                VQES_FA_CNT(fbt_nl_send_failed_tmo);
                continue;
            }
            else {
                /* Send failed for some really unexpected reason, bail out */
                err = VQE_FBT_FAILED;
                VQES_FA_CNT(fbt_nl_send_failed_other);
                break;
            }
        }

        /* Successfully sent request, now look for response */
        err = vqe_nl_recv_rsp(req, rsp, check_rsp, curr_time, quitting_time, 
                             &recv_tmo);
        if (err != VQE_FBT_RETRY)
            break;

        /* Update current time */
        curr_time = get_sys_time();

        VQES_FA_CNT(fbt_nl_req_retries);
    }

    /* Count the occurrences of running out of retry attempts, which is a 
     * failure */
    if (err == VQE_FBT_RETRY) {
        VQES_FA_CNT(fbt_nl_req_no_more_retries);
    }

    /* Return success only when VQE_FBT_OK */
    if (err == VQE_FBT_OK) {
        ret = 0;
    }

    /* Reset socket timeouts to the minimum value */
    if (TIME_CMP_R(gt, send_tmo, VQE_NL_MSG_SNDRCV_MIN_TMO)) {
        /* sa_ignore {if fail, use current tmo} IGNORE_RETURN */
        vqe_nl_sock_set_tmo(SO_SNDTIMEO, VQE_NL_MSG_SNDRCV_MIN_TMO);
    }
    if (TIME_CMP_R(gt, recv_tmo, VQE_NL_MSG_SNDRCV_MIN_TMO)) {
        /* sa_ignore {if fail, use current tmo} IGNORE_RETURN */
        vqe_nl_sock_set_tmo(SO_RCVTIMEO, VQE_NL_MSG_SNDRCV_MIN_TMO);
    }

done:
    /* Release global lock */
    if (pthread_mutex_unlock(&nl_mutex) != 0) {
        ASSERT_FATAL(0, CP_INTERNAL_FORCED_EXIT, "releasing netlink mutex");
    }
    return ret;
}

/**
 * vqes_fbt_check_add_response
 * @brief
 * This function checks the netlink response to an address add request
 * @param[in] req - Netlink add request
 * @param[in] rsp - Received netlink response
 * @return
 * - VQE_FBT_OK for a successful response
 * - VQE_FBT_RETRY to retry the request
 * - VQE_FBT_FAILED for a failed response (no retry)
 */
static vqes_fbt_ret_t 
vqes_fbt_check_add_response (vqe_nl_req_t * req, vqe_nl_rsp_t * rsp)
{
    vqes_fbt_ret_t err = VQE_FBT_OK;

    if (rsp->nl_hdr.nlmsg_type != NLMSG_ERROR) {
        /* Unexpected response type, not ACK/NACK */
        VQES_FA_CNT(fbt_nl_recv_invalid_add_msg);
        err = VQE_FBT_RETRY;
        goto done;
    }
    
    /* 
     * Check if copy of request in ACK/NACK matches original request 
     * (note that we could burrow deeper into the original request and
     * verify RT attributes as well, but checking sequence number should
     * be sufficient)
     */
    struct nlmsgerr * err_msg = &rsp->status.nack_hdr; 
    if ((err_msg->msg.nlmsg_type != RTM_NEWADDR) ||
        (err_msg->msg.nlmsg_seq != req->nl_hdr.nlmsg_seq)) {
        VQES_FA_CNT(fbt_nl_recv_invalid_add_msg);
        err = VQE_FBT_RETRY;
        goto done;
    }

    /* Check error reported. An error code of zero indicates success (ACK) */
    if (err_msg->error != 0) {
        switch (-err_msg->error) {
        case EEXIST:
            /* If address is already there, we consider it a successful add */
            VQES_FA_CNT(fbt_nl_dup_add);
            break;

        case EINVAL:
        case EPERM:
        case EBADF:
        case ENOTSOCK:
        case EOPNOTSUPP:
            /* Probably not worth retrying, call it a failure */
            err = VQE_FBT_FAILED;
            break;

        default:
            /* Anything else, retry */
            err = VQE_FBT_RETRY;
            break;
        }
    }

done:
    return err;
}

/**
 * vqes_fbt_addr_add
 * @brief
 * This function adds a feedback target address to the loopback interface
 * via netlink
 * @param[in] addr - FBT address to add
 * @param[in] scope - Desired scope
 * @return
 * - 0 for successful add
 * - -1 otherwise (either not successful, or status of request
 *                 could not be determined)
 */
int
vqes_fbt_addr_add (struct in_addr addr, int scope)
{
    vqe_nl_req_t req;
    vqe_nl_rsp_t rsp;
    int ret = 0;
    char tmp[MAX_IP_ADDR_LEN];
    const char *addr_str = inet_ntop(AF_INET, &addr, tmp, MAX_IP_ADDR_LEN);
    
    memset(&req, 0, sizeof(req));
    memset(&rsp, 0, sizeof(rsp));

    /* Set up netlink header */
    vqe_ifaddr_req_fill_nlmsghdr(&req.nl_hdr, RTM_NEWADDR, 
                                NLM_F_REQUEST | NLM_F_ACK);

    /* Fill in address request header */
    vqe_ifaddr_req_fill_ifaddrmsg(&req.ifaddr_msg, scope);

    /* Fill in RT address attribute */
    struct rtattr * rt_attr;
    uint32_t addr_len;
    rt_attr = (struct rtattr *)(((char *) &req) + 
                                NLMSG_ALIGN(req.nl_hdr.nlmsg_len));
    addr_len = vqe_ifaddr_req_add_addr_attr(rt_attr, addr);

    /* Fill in RT label attribute */
    uint32_t label_len;
    rt_attr = (struct rtattr *)(((char *) &req) + 
                                NLMSG_ALIGN(req.nl_hdr.nlmsg_len) +
                                addr_len);
    label_len = vqe_ifaddr_req_add_label_attr(rt_attr, VQE_LOOPBACK_LABEL);
    
    /* Set overall length of request */
    req.nl_hdr.nlmsg_len = NLMSG_ALIGN(req.nl_hdr.nlmsg_len) + 
        addr_len + label_len;
    
    /* Process netlink request */
    ret = vqe_nl_process_req(&req, &rsp, vqes_fbt_check_add_response);

    if (ret == 0) {
        syslog_print(VQE_FBT_REQ_INFO, 
                "added", 
                addr_str,
                vqes_fbt_addr_scope_to_str(scope));
        VQES_FA_CNT(fbt_nl_add);
    } else {
        syslog_print(VQE_FBT_REQ_ERROR, 
                "adding", 
                addr_str,
                vqes_fbt_addr_scope_to_str(scope));
        VQES_FA_CNT(fbt_nl_add_failed);
    }
    return ret;
}

/**
 * vqes_fbt_check_del_response
 * @brief
 * This function checks the netlink response to an address delete request
 * @param[in] req - Netlink delete request
 * @param[in] rsp - Received netlink response
 * @return
 * - VQE_FBT_OK for a successful response
 * - VQE_FBT_RETRY to retry the request
 * - VQE_FBT_FAILED for a failed response (no retry)
 */
static vqes_fbt_ret_t 
vqes_fbt_check_del_response (vqe_nl_req_t * req, vqe_nl_rsp_t * rsp)
{
    vqes_fbt_ret_t err = VQE_FBT_OK;

    if (rsp->nl_hdr.nlmsg_type != NLMSG_ERROR) {
        /* Unexpected response type, not ACK/NACK */
        VQES_FA_CNT(fbt_nl_recv_invalid_del_msg);
        err = VQE_FBT_RETRY;
        goto done;
    }
     
    /* 
     * Check if copy of request in ACK/NACK matches original request 
     * (note that we could burrow deeper into the original request and
     * verify RT attributes as well, but checking sequence number should
     * be sufficient)
     */
    struct nlmsgerr * err_msg = &rsp->status.nack_hdr; 
    if ((err_msg->msg.nlmsg_type != RTM_DELADDR) ||
        (err_msg->msg.nlmsg_seq != req->nl_hdr.nlmsg_seq)) {
        VQES_FA_CNT(fbt_nl_recv_invalid_del_msg);
        err = VQE_FBT_RETRY;
        goto done;
    }

    /* Check error reported. An error code of zero indicates success (ACK) */
    if (err_msg->error != 0) {
        switch (-err_msg->error) {
        case EADDRNOTAVAIL:
            /* If address was not there, we consider it a successful delete */
            VQES_FA_CNT(fbt_nl_dup_del);
            break;
            
        case EINVAL:
        case EPERM:
        case EBADF:
        case ENOTSOCK:
        case EOPNOTSUPP:
            /* Probably not worth retrying, call it a failure */
            err = VQE_FBT_FAILED;
            break;

        default:
            /* Anything else, retry */
            err = VQE_FBT_RETRY;
            break;
        }
    }

done:
    return err;
}

/**
 * vqes_fbt_addr_del
 * @brief
 * This function deletes a feedback target address from the loopback interface
 * via netlink
 * @param[in] addr - FBT address to delete
 * @return
 * - 0 for successful delete
 * - -1 otherwise (either not successful, or status of request
 *                 could not be determined)
 */
int
vqes_fbt_addr_del (struct in_addr addr)
{
    vqe_nl_req_t req;
    vqe_nl_rsp_t rsp;
    int ret = 0;
    char tmp[MAX_IP_ADDR_LEN];
    const char *addr_str = inet_ntop(AF_INET, &addr, tmp, MAX_IP_ADDR_LEN);

    memset(&req, 0, sizeof(req));
    memset(&rsp, 0, sizeof(rsp));

    /* Set up netlink header */
    vqe_ifaddr_req_fill_nlmsghdr(&req.nl_hdr, RTM_DELADDR,
                                 NLM_F_REQUEST | NLM_F_ACK);

    /* Fill in address request header */
    vqe_ifaddr_req_fill_ifaddrmsg(&req.ifaddr_msg, 0);

    /* Fill in RT address attribute */
    struct rtattr * rt_attr;
    uint32_t addr_len;
    rt_attr = (struct rtattr *)(((char *) &req) + 
                                NLMSG_ALIGN(req.nl_hdr.nlmsg_len));
    addr_len = vqe_ifaddr_req_add_addr_attr(rt_attr, addr);
    
    /* Set overall length of request */
    req.nl_hdr.nlmsg_len = NLMSG_ALIGN(req.nl_hdr.nlmsg_len) + addr_len;

    /* Process netlink request */
    ret = vqe_nl_process_req(&req, &rsp, vqes_fbt_check_del_response);

    if (ret == 0) {
        syslog_print(VQE_FBT_REQ_INFO, 
                "removed", 
                addr_str, "");
        VQES_FA_CNT(fbt_nl_del);
    } else {
        syslog_print(VQE_FBT_REQ_ERROR, 
                "removing", 
                addr_str, "");
        VQES_FA_CNT(fbt_nl_del_failed);
    }

    return ret;
}


/**
 * vqes_fbt_addr_get_status
 * @brief
 * This function gets the current status of a feedback target address on the 
 * loopback interface. Note that this function currently uses the ip command,
 * but in the future it would ideally go through netlink directly.
 * @param[in] addr - FBT address
 * @param[out] addr_status - Current status of FBT address
 * @return
 * - 0 for successful request
 * - -1 otherwise (either not successful, or status of address
 *                 could not be determined)
 */
int
vqes_fbt_addr_get_status (struct in_addr addr, vqes_fbt_status_t * addr_status)
{
    char cmd[VQE_SYS_CMD_LEN];
    int op_result = 0;
    char tmp[MAX_IP_ADDR_LEN];
    const char * addr_str = inet_ntop(AF_INET, &addr, tmp, MAX_IP_ADDR_LEN);
    vqes_fbt_status_t status = VQE_FBT_ADDR_MISSING;
    int ret = 0;

    /* First try finding address with global scope */
    snprintf(cmd, VQE_SYS_CMD_LEN,
             "[ -z \"`/sbin/ip -f inet -o addr show dev lo to %s/32 "
             "scope %d 2>/dev/null`\" ]", addr_str, RT_SCOPE_UNIVERSE);
    op_result = system(cmd);
    
    switch (op_result) {
    case VQE_ADDR_ABSENT:
        /* Didn't find it with global scope, try host scope */
        snprintf(cmd, VQE_SYS_CMD_LEN,
                 "[ -z \"`/sbin/ip -f inet -o addr show dev lo to %s/32 "
                 "scope %d 2>/dev/null`\" ]", addr_str, RT_SCOPE_HOST);
        op_result = system(cmd);
        
        switch (op_result) {
        case VQE_ADDR_ABSENT:
            status = VQE_FBT_ADDR_MISSING;
            break;

        case VQE_ADDR_PRESENT:
            status = VQE_FBT_ADDR_HOST;
            break;

        default:
            ret = -1;
            break;
        }
        break;

    case VQE_ADDR_PRESENT:
        status = VQE_FBT_ADDR_GLOBAL;
        break;

    default:
        ret = -1;
        break;
    }
    *addr_status = status;

    if (ret == 0) {
        syslog_print(VQE_FBT_REQ_INFO, 
                "retrieved status", 
                addr_str, 
                vqes_fbt_addr_status_to_str(status));
        VQES_FA_CNT(fbt_nl_status);
    } else {
        syslog_print(VQE_FBT_REQ_ERROR, 
                "retrieving status", 
                addr_str, "");
        VQES_FA_CNT(fbt_nl_status_failed);
    }

    return ret;
}

/**
 * vqes_fbt_addr_flush_all
 * @brief
 * This function flushes all FBT addresses on the loopback interface that were
 * added by VQES (as noted by the 'lo:vqes' label associated with the addresses).
 * @return
 * - 0 for successful flush
 * - -1 otherwise (either not successful, or status of flush
 *                 could not be determined)
 */
int 
vqes_fbt_addr_flush_all (void)
{
    vqe_nl_req_t req;
    vqe_nl_rsp_t rsp;
    int ret = 0;

    memset(&req, 0, sizeof(req));
    memset(&rsp, 0, sizeof(rsp));

    /* Set up netlink header. Not sure why but if NLM_F_ACK is added to the
     * other flags, the batch deletion will no longer work.
     */
    vqe_ifaddr_req_fill_nlmsghdr(&req.nl_hdr, RTM_DELADDR,
                                 NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST);
    
    /* Fill in address request header */
    vqe_ifaddr_req_fill_ifaddrmsg(&req.ifaddr_msg, 0);

    /* Fill in IFA_LABEL attribute with label "lo:vqes" */
    struct rtattr * rt_attr;
    uint32_t label_len;
    rt_attr = (struct rtattr *)(((char *) &req) + 
                                NLMSG_ALIGN(req.nl_hdr.nlmsg_len));
    label_len = vqe_ifaddr_req_add_label_attr(rt_attr, VQE_LOOPBACK_LABEL);
    
    /* Set overall length of request */
    req.nl_hdr.nlmsg_len = NLMSG_ALIGN(req.nl_hdr.nlmsg_len) + label_len;

    /* Process netlink request */
    ret = vqe_nl_process_req(&req, &rsp, vqes_fbt_check_del_response);

    (ret == 0) ? VQES_FA_CNT(fbt_nl_flush) : VQES_FA_CNT(fbt_nl_flush_failed);

    if (ret == 0) {
        syslog_print(VQE_FBT_REQ_INFO, 
                "flushed", 
                "<ALL>", "");
        VQES_FA_CNT(fbt_nl_flush);
    } else {
        syslog_print(VQE_FBT_REQ_ERROR, 
                "flushing", 
                "<ALL>", "");
        VQES_FA_CNT(fbt_nl_flush_failed);
    }

    return ret;
}
