/*
 *------------------------------------------------------------------
 * Video Quality Reporter
 *
 * Jan 9, 2007, Carol Iturralde
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * cei: todo:
 *   - change addr/port to sockaddr_in
 *      o print in dotted decimal (rather than hex)
 *------------------------------------------------------------------
 */

/* System Includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

/* Application Includes */
#include <utils/vam_debug.h>
#include <utils/vam_types.h>
#include "vqr.h"

/* 
 * VQR Globals
 */
static vqr_globals_t vqr_globals;

static boolean vqr_initialized = FALSE;

/*
 * Forward Declarations
 */
vqr_status_e vqr_open_new_socket (in_addr_t vqm_ip_addr, uint16_t  vqm_port);


/* 
 * Function:    vqr_init
 *
 * Description: Initialize the Video Quality Reporter (VQR) component
 *
 *              Initializes VQR global data structures, and opens a UDP socket 
 *              to the Video Quality Monitoring (VQM) application.
 *
 * Called:      Once, when VQE-S Control Plane process is started.
 *
 * Parameters:
 *
 *    o vqm_ip_addr - IP Address where VQM application resides (can be reached)
 *    o vqm_port    - UDP port number where VQM application can be reached
 *
 *    o vqm_ip_addr_configured - TRUE if vqm_ip_addr was configured
 *    o vqm_port_configured    - TRUE if vqm_port was configured
 *
 *    Both parameters are assumed to be in network byte order.
 *    If neither parameter was configured, then the VQR facility is simply not
 *    desired (don't open the socket, don't send reports).  If both are configured,
 *    the facility is desired and an attempt is made to start it up.  If
 *    one is configured w/o the other, or either is invalid, this is an error.
 *
 * Returns:
 *
 *    o Success - If input arguments are valid, and UDP socket to this
 *                destination (vqm_ip_addr, vqm_port) can be opened.
 *                If user did not configre vqm information.
 *    o Failure - If user incorrectly configured VQM information (either
 *                - specified only one of the parameters (ip addr or port)
 *                - supplied invalid data for either ip addr or port
 */
vqr_status_e vqr_init (in_addr_t vqm_ip_addr,
                       uint16_t  vqm_port,
                       boolean   vqm_ip_addr_configured,
                       boolean   vqm_port_configured)
{

    vqr_status_e status;

    if (vqr_initialized) {
        vam_debug_error("video_quality_reporter:: VQR initialization called, "
                        "twice, should never happen");
        return (VQR_FAILURE);
    }
    vqr_initialized = TRUE;

    /* 
     * Initialize VQR Globals 
     */
    vqr_globals.state                = VQR_STATE_NO_CONFIG;
    vqr_globals.vqm_ip_addr_config   = vqm_ip_addr_configured;
    vqr_globals.vqm_port_config      = vqm_port_configured;
    vqr_globals.socket_open          = FALSE;
    vqr_globals.send_buf_p           = NULL;
    vqr_globals.num_reports_sent     = 0;
    vqr_globals.num_reports_dropped  = 0;
    vqr_globals.drops_pending_export = 0;

    // vqr_globals.sock_to_vqm.sin_family = AF_INET;
    if (vqr_globals.vqm_ip_addr_config) {
        vqr_globals.vqm_ip_addr = vqm_ip_addr;
    }
    if (vqr_globals.vqm_port_config) {
        vqr_globals.vqm_port = vqm_port;
    }

    /* 
     * Determine whether VQR is desired (if any VQM info supplied)
     */
    if ((vqm_ip_addr_configured == FALSE) && (vqm_port_configured == FALSE)) {
        vam_debug_trace("video_quality_reporter:: VQR not started, no VQM info "
                        "configured");
        return (VQR_SUCCESS);
    }

    /* 
     * Ensure both vqm parameters were configured
     */
    if ((vqm_ip_addr_configured == FALSE) || (vqm_port_configured == FALSE)) {
        vam_debug_error("video_quality_reporter:: VQR initialization failed, "
                        "VQM %s missing",
                        (vqm_port_configured == FALSE)? "port" : "IP address");
        
        goto init_failed;
    }

    /*
     * Ensure both vqm parameters valid (cei: add better IP Addr validation)
     */
    if ((vqm_ip_addr == INADDR_NONE) || (vqm_port == 0)) {
        vam_debug_error("video_quality_reporter:: VQR initialization failed, "
                        "VQM IP address or port invalid, "
                        "addr = 0x%x, port = %d\n", ntohl(vqm_ip_addr), 
                        ntohs(vqm_port));
        
        goto init_failed;
    }


    /*
     * Allocate a single global buffer for sending to UDP socket.
     *
     *    Each report is likely pretty small:
     *
     *        o RR: 32 bytes
     *        o SR: 52 bytes
     *    
     *    But we export entire compound packet, which may include other
     *          packets such as SDES, NACK, NACK PLI, ... 
     *
     *        o RR: 32 bytes (+ SDES=~32 bytes => total: ~64 bytes)
     *        o SR: 52 bytes (+ SDES=~32 bytes => total: ~84 bytes)
     * If we :
     *   o assume each compound packet is less than 100 bytes, and
     *   o want to avoid fragmentation (stay under 1400 bytes, after
     *      lower layer headers prepended), and
     *   o want to allow for some amount of (cei: left off here)
     */
    vqr_globals.send_buf_p = (uint8_t*) malloc(VQR_BUF_SIZE);
    if (vqr_globals.send_buf_p == NULL) {
        goto init_failed;
    }

    
    /* 
     * Open UDP Socket to VQM Application
     *
     * note: vqr_globals socket fields updated inside function call below)
     *
     * cei: do I need to register for async. events on this socket?
     */
    status = vqr_open_new_socket(vqm_ip_addr, vqm_port);

    if (status != VQR_SUCCESS) {
        vam_debug_error("video_quality_reporter:: VQR initialization failed, "
                        "Socket could not be opened");
        goto init_failed;
    }


    /* Return Success! */
    vam_debug_trace("video_quality_reporter:: VQR Init Succeeded,connected "
                    "to IP Address: 0x%x, Port: %d", ntohl(vqm_ip_addr), 
                    ntohs(vqm_port));
    vqr_globals.state = VQR_STATE_UP;
    return (VQR_SUCCESS);

 init_failed:

    if (vqr_globals.send_buf_p) {
        free(vqr_globals.send_buf_p);
        vqr_globals.send_buf_p = NULL;
    }
    vqr_globals.state   = VQR_STATE_INIT_FAILED;    
    return (VQR_FAILURE);            

}

/* 
 * Function:    vqr_open_new_socket
 *
 * Description: Open UDP socket to VQM Application
 *
 * Called:      Once at process start-up, when VQE-S Control Plane process is
 *              firt started.  In this case, parameters come from vam.conf file.
 *
 *              May be called additional times if, after start-up, if the user
 *              modifies the VQM information (IP address and/or UDP port
 *              number).  In this case, parameters come from the Configuration
 *              Manager, and if a socket is currently open, it is closed and
 *              the new one is opened.
 *
 * Parameters:
 *
 *    o vqm_ip_addr - IP Address where VQM application resides (can be reached)
 *    o vqm_port    - UDP port number where VQM application can be reached
 *
 *    Caller is assumed to have validated input parameters.  Both parameters
 *    are assumed to be already stored in network byte order.
 *
 * Returns:
 *
 *    o Success - If a UDP socket to this destination was successfully opened.
 *    o Failure - Otherwise
 *
 * Side Effects:  On success, updates vqr_globals.socket to point to newly
 *                opened socket.
 *
 */
vqr_status_e vqr_open_new_socket (in_addr_t vqm_ip_addr,
                                  uint16_t  vqm_port)
{
    int32_t            new_socket;
    int                on = 1;
    struct sockaddr_in dst_addr;


    /* 
     * Open new UDP Socket to VQM Application
     */

    if ((new_socket = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
        return (VQR_FAILURE); /* caller prints failure debug msg */
    }

    if (setsockopt(new_socket,
                   SOL_SOCKET,       /* manipulating option at socket level */
                   SO_REUSEADDR,     /* allow re-use of local addrs in bind */
                   &on,              /* enable this socket option           */
                   sizeof(on)) == -1) {

        close(new_socket);
        return (VQR_FAILURE); /* caller prints failure debug msg */
    }
    
    if (fcntl(new_socket, F_SETFL, O_NONBLOCK) == -1) {
        close(new_socket);
        return (VQR_FAILURE); /* caller prints failure debug msg */
    }

    dst_addr.sin_family      = AF_INET;
    dst_addr.sin_addr.s_addr = vqm_ip_addr;
    dst_addr.sin_port        = vqm_port;

    if (connect(new_socket, 
                (struct sockaddr *) &dst_addr, sizeof(dst_addr)) == -1) {
        close(new_socket);
        return (VQR_FAILURE); /* caller prints failure debug msg */
    }

    vqr_globals.socket_open = TRUE;
    vqr_globals.socket      = new_socket;
    return (VQR_SUCCESS);
}


/*
 * vqr_report_init
 *
 * Set up the header for a VQR export message (of type VQR_RTCP_REPORT)
 */
void vqr_report_init (vqr_hdr_t *hdr,
                      vqr_hdr_subtype_e subtype,
                      in_addr_t chan_addr,
                      uint16_t  chan_port,
                      vqr_hdr_role_e sndr_role,
                      vqr_hdr_role_e rcvr_role,
                      ntp64_t orig_send_time,
                      rtp_envelope_t *orig_addrs)
{
    uint16_t params = 0;

    params = vqr_hdr_set_version(params);
    params = vqr_hdr_set_type(params, VQR_RTCP_REPORT);
    params = vqr_hdr_set_subtype(params, subtype);
    hdr->params = htons(params);
    hdr->len = htons((uint16_t)(sizeof(vqr_hdr_t)/sizeof(uint32_t) - 1));
    hdr->chan_addr = chan_addr;
    hdr->chan_port = chan_port;
    hdr->sndr_role = sndr_role;
    hdr->rcvr_role = rcvr_role;
    hdr->ntp_upper = htonl(orig_send_time.upper);
    hdr->ntp_lower = htonl(orig_send_time.lower);
    hdr->src_addr = orig_addrs->src_addr;
    hdr->src_port = orig_addrs->src_port;
    hdr->dst_addr = orig_addrs->dst_addr;
    hdr->dst_port = orig_addrs->dst_port;
}


/* 
 * Function:    vqr_rr_export
 *
 * Description: Export an RTCP Receiver Report (RR)
 *
 *              Called whenever RTCP 
 *
 *                 o Receives  an RTCP RR from a STB
 *                 o Generates an RTCP RR (for VQE-S)
 *                 o Generates an RTCP SR (for VQE-S)
 *
 *              If the VQR is UP (it has been configured to send Reports,
 *              and is ready to do so), immediately sends this Report out 
 *              over the UDP socket to the VQM Application.
 *
 *              If the VQR is not UP, just returns (does nothing).
 *
 *
 * Parameters:
 *
 *    o vqm_ip_addr - IP Address where VQM application resides (can be reached)
 *    o vqm_port    - UDP port number where VQM application can be reached
 *
 *    Caller is assumed to have validated input parameters.  Both parameters
 *    are assumed to be already stored in network byte order.
 *
 * Returns:
 *
 *    o Success - If a UDP socket to this destination was successfully opened.
 *    o Failure - Otherwise
 *
 * Side Effects:  On success, updates vqr_globals.socket to point to newly
 *                opened socket.
 *
 */
void vqr_export (vqr_hdr_t *hdr_p, uint8_t *report_p, uint16_t report_len)
{
    
    vqr_base_header_t        *vqr_base_p;
    vqr_missed_reports_t     *vqr_missed_p;
    vqr_rtcp_report_header_t *vqr_report_p;
    uint8_t                  *export_p;
    uint16_t                 tot_len = 0;
    
    if (vqr_globals.state != VQR_STATE_UP) {
        /* note: we don't count reports dropped when we're down */
        return;
    }

    goto alternate_export;

    vqr_base_p = (vqr_base_header_t*) vqr_globals.send_buf_p;

    if (vqr_globals.drops_pending_export) {
        /* add a missed reports record first */
        vqr_base_p->type   = VQR_BASE_TYPE_MISSED_REPORTS;
        vqr_base_p->flags  = 0;
        vqr_base_p->length = (sizeof(vqr_base_header_t) + 
                              sizeof(vqr_missed_reports_t));
        vqr_missed_p = (vqr_missed_reports_t*)vqr_base_p->payload[0];
        vqr_missed_p->num_reports_missing = vqr_globals.drops_pending_export;
        
        tot_len = vqr_base_p->length;
        vqr_base_p = (vqr_base_header_t*)
            (vqr_globals.send_buf_p += vqr_base_p->length);
    }

    /* add RTCP Report now */
    vqr_base_p->type   = VQR_BASE_TYPE_RTCP_REPORT;
    vqr_base_p->flags  = 0;
    vqr_base_p->length = (sizeof(vqr_base_header_t) + 
                          sizeof(vqr_rtcp_report_header_t) +
                          report_len);
    vqr_report_p = (vqr_rtcp_report_header_t*)vqr_base_p->payload[0];
    vqr_report_p->version  = 1;
    vqr_report_p->src_addr = hdr_p->src_addr;

    tot_len += vqr_base_p->length;
    if ((tot_len) > VQR_BUF_SIZE) {
        vam_debug_error("video_quality_reporter:: VQR send exceeded buffer "
                        "size, should never happen (size = %d", tot_len);
        goto vqr_export_fail;
    }

    bcopy(report_p, (void*)vqr_report_p->payload[0], report_len);

    if (send(vqr_globals.socket, 
             vqr_globals.send_buf_p, tot_len, 0) == -1) {
        /* locally-detected error occurred*/
        /* cei: per-error-type counter increment? e.g. ENOBUFS, ENETUNREACH,...*/
        goto vqr_export_fail;
    }

alternate_export:
    export_p = vqr_globals.send_buf_p;
    memcpy(export_p, hdr_p, sizeof(vqr_hdr_t));
    tot_len += sizeof(vqr_hdr_t);
    memcpy(export_p + tot_len, report_p, report_len);
    tot_len += report_len;
    if (send(vqr_globals.socket, 
             vqr_globals.send_buf_p, tot_len, 0) == -1) {
        goto vqr_export_fail;
    }

    /* 
     * Export succeeded 
     */
    vqr_globals.num_reports_sent++;
    vqr_globals.drops_pending_export = 0;
    /* cei: temporary debug */
    vam_debug_trace("video_quality_reporter:: VQR Send Succeeded, "
                    "sent %d reports so far", vqr_globals.num_reports_sent);
    return;

 vqr_export_fail:
    vqr_globals.num_reports_dropped++;
    vqr_globals.drops_pending_export++;
    /* cei: temporary debug */
    vam_debug_trace("video_quality_reporter:: VQR Send Failed, "
                    "dropped %d reports so far", vqr_globals.num_reports_dropped);
    return;
}


/* Function:    vqr_shutdown
 *
 * Description: Shutdown the Video Quality Reporter (VQR) component
 *
 *              Closes UDP socket, ...
 *
 * Parameters:  None
 *
 * Returns:     Success or failure
 */
vqr_status_e vqr_shutdown ()
{
    return (VQR_SUCCESS);
}

