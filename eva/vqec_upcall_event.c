/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Upcall event handling in control-plane.
 *
 * Documents: 
 *
 *****************************************************************************/
#include "vqec_upcall_event.h"
#include "vqec_channel_private.h"
#include "vqec_debug.h"
#include <sys/un.h>
#include "vqec_lock_defs.h"
#include "vqec_recv_socket.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#include "test_vqec_utest_interposers.h"
#endif


/****************************************************************************
 * HELPFUL DEFINITIONS: 
 *
 *  -- Upcall: An unsolicited event from the data- to the control-plane.
 *  -- IRQ: An interrupt request (IRQ) is a message from the dataplane to
 *     the control-plane, sent via socket. The request tells the control-plane
 *     that an event is waiting in the dataplane which the control-plane 
 *     should read via IPC. IRQ messages are generated on a per channel basis.
 * -- Upcall sockets: There are 2 different upcall sockets: one for IRQ 
 *     requests from the dataplane-to-control plane, and one for receiving
 *     any packets that the dataplane has "punted" or "ejected" to the
 *     control-plane. For events on the IRQ socket, the control-plane must
 *     acknowledge the event via IPC and acquire the associated event
 *     data. Packets received on the "punt" or "eject" packet socket are
 *     delivered to the control-plane channel which determines their
 *     final destination.
 * -- The sockets are UNIX domain sockets, and hence an inode is 
 *     needed to support them. These inodes are created in the "/tmp"
 *     directory; the option may be made a compile-time configuration
 *     parameter if so needed.
 * -- Generation Identifier: A number which is assigned to a dataplane 
 *     when it is first created, and does not change after that point. This
 *     is used to ensure that quick channel identifier reuse does not cause
 *     a message for the channel that has deleted to be processed by
 *     the channel that is now active with the same id.
 * -- Message Generation Identifier: The sequence number of a message. 
 *     The sequence number space is per channel (not global across all 
 *     the channels).
 */


/* Upcall unix-domain sockets base directory. */
#ifndef VQEC_TMP_FILE_PATH
 #define VQEC_UPCALL_SOCKET_BASEDIR "/tmp/"
#else 
 #define VQEC_UPCALL_SOCKET_BASEDIR VQEC_TMP_FILE_PATH
#endif

/* 
 * Upcall socket for "IRQ" styled-events, i.e., the maximum socket queue-depth
 * is bounded, and drops are unexpected.
 */
#define VQEC_UPCALL_SOCKET_IRQEVENTS ".vqec_irqsk"
/* 
 * Upcall socket for packet ejects, and other unsolicited: the maximum socket
 * queue-depth is not bounded, and drops at enQ are possible.
 */
#define VQEC_UPCALL_SOCKET_RX_PAKEJECT ".vqec_paksk"

/**
 * Private upcall socket descriptor structure.
 */
struct vqec_upcall_sock_desc
{
    /**
     * Upcall socket.
     */
    struct sockaddr_un sock;
    /**
     * Upcall socket event.
     */
    vqec_event_t *ev;
    /**
     * A valid socket descriptor (to differentiate fd = 0).
     */
    boolean is_fd_valid;
    /**
     * Socket descriptor.
     */
    int32_t fd;
    /**
     * Socket message buffer (used only for packet sockets).
     */
    char *pak_buf;
    /**
     * Socket message buffer length.
     */
    uint32_t buf_len;
    /**
     * Upcall events received.
     */
    uint32_t upcall_events;
    /**
     * Upcall events with generation number mismatch.
     */
    uint32_t upcall_lost_events;
    /**
     * Error events, i.e., events that cannot be parsed.
     */
    uint32_t upcall_error_events;
    /**
     * Error while acknowledging / polling events.
     */
    uint32_t upcall_ack_errors;

};

static struct vqec_upcall_sock_desc s_upcall_irqsock, s_upcall_pak_ejectsock;

/**
 * Upcall interrupting device attributes: for internal use within the
 * control-plane channel module.
 */  
typedef
struct vqec_upcall_chan_dev_attrib_
{
    /**
     * channel's generation number from dataplane.
     */
    uint32_t chan_generation_num;    
    /**
     * device type from dataplane.
     */
    vqec_dp_upcall_device_t device;
    /**
     * device identifier from dataplane.
     */
    vqec_dp_streamid_t device_id;

} vqec_upcall_chan_dev_attrib_t;


/**---------------------------------------------------------------------------
 * Dataplane channel generation identifiers. These are assigned when the 
 * dpchan is created and remain constant throughout it's lifetime. 
 *---------------------------------------------------------------------------*/ 
static inline boolean 
vqec_upcall_chan_generation_id_verify (vqec_chan_t *chan,
                                       uint32_t dp_generation)
{
    vqec_chan_upcall_data_t *upc_data;

    upc_data = vqec_chan_upc_data_ptr(chan);
    if (!upc_data->dp_generation_id) {
        /* First update after creation. */
        upc_data->dp_generation_id = dp_generation; 
    }
    
    return (upc_data->dp_generation_id == dp_generation);
}

static inline uint32_t
vqec_upcall_chan_get_generation_id (vqec_chan_t *chan)
{
    vqec_chan_upcall_data_t *upc_data;

    upc_data = vqec_chan_upc_data_ptr(chan);
    return (upc_data->dp_generation_id);
}


static inline boolean
vqec_upcall_generation_num_le (uint32_t cur, uint32_t last)
{
    return ((last == cur) || ((cur - last) >> 31));
}


/**---------------------------------------------------------------------------
 * Update the upcall message sequence generation (sequence number). 
 * 
 * @param[in] chan Pointer to the channel object.
 * @param[in] cur_generation_num The current message generation number 
 * read from an incoming upcall event.
 * @param[out] boolean Returns true if the current message generation 
 * number is equal to the last message generation number plus one.
 *---------------------------------------------------------------------------*/ 
static boolean
vqec_upcall_chan_update_msg_generation_num (vqec_chan_t *chan,
                                            uint32_t cur_generation_num)
{
    boolean ret = TRUE;
    vqec_chan_upcall_data_t *upc_data;

    upc_data = vqec_chan_upc_data_ptr(chan);
    upc_data->upcall_events++;

    if (vqec_upcall_generation_num_le(cur_generation_num, 
                                      upc_data->upcall_last_generation_num)) {
        VQEC_DEBUG(VQEC_DEBUG_UPCALL, 
                   "Repeated or late upcall event on channel %s "
                   "(expected %d,  actual %d)\n",
                   vqec_chan_print_name(chan),
                   upc_data->upcall_last_generation_num+1, cur_generation_num);
        upc_data->upcall_repeat_events++;
        ret = FALSE;
        
    } else if ((upc_data->upcall_last_generation_num + 1) != cur_generation_num) {
        VQEC_DEBUG(VQEC_DEBUG_UPCALL, 
                   "Generation number mismatch on upcall for channel %s "
                   "(expected %d,  actual %d)\n",
                   vqec_chan_print_name(chan),
                   upc_data->upcall_last_generation_num + 1, cur_generation_num);
        upc_data->upcall_lost_events++; 
        ret = FALSE;

    } else {
        
        /* Update last generation number. */
        upc_data->upcall_last_generation_num = cur_generation_num;
    }
    
    return (ret);
}


/**---------------------------------------------------------------------------
 * Process the data for a single upcall event for a channel. The data has 
 * been retrieved via IPC. The data is delivered to it's target entity in
 * the channel based on the "device type" specified by the event.
 * 
 * @param[in] chan Pointer to the channel instance.
 * @param[in] attrib Attributes for the device that generated the event.
 * @param[in] irq_resp The event response data retrieved from the dataplane.
 *---------------------------------------------------------------------------*/ 
#define VQEC_DP_UPCALL_RTP_SRC_EVENTS                   \
    (VQEC_DP_UPCALL_MAKE_REASON(                        \
         VQEC_DP_UPCALL_REASON_RTP_SRC_ISACTIVE) |      \
     VQEC_DP_UPCALL_MAKE_REASON(                        \
         VQEC_DP_UPCALL_REASON_RTP_SRC_ISINACTIVE) |    \
     VQEC_DP_UPCALL_MAKE_REASON(                        \
         VQEC_DP_UPCALL_REASON_RTP_SRC_NEW) |           \
     VQEC_DP_UPCALL_MAKE_REASON(                        \
         VQEC_DP_UPCALL_REASON_RTP_SRC_CSRC_UPDATE))

static void
vqec_upcall_chan_event (vqec_chan_t *chan,
                        vqec_upcall_chan_dev_attrib_t *attrib,
                        vqec_dp_irq_ack_resp_t *irq_resp)
{
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    vqec_chan_upcall_data_t *upc_data;

    upc_data = vqec_chan_upc_data_ptr(chan);
    upc_data->upcall_ack_inputs++;

    VQEC_DEBUG(VQEC_DEBUG_UPCALL,
               " UPC: channel %s, dev: %d/%x, reason: %x\n",
               vqec_chan_print_name(chan),
               attrib->device,
               attrib->device_id,
               irq_resp->irq_reason_code);

    if (!irq_resp->irq_reason_code) {
        VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                   "UPC: channel %s rx upcall event with null reason code\n",
                   vqec_chan_print_name(chan));
        upc_data->upcall_ack_nulls++;
        return;
    }

    /* deliver to next-element based on interrupting device */
    switch (attrib->device) {
    case VQEC_DP_UPCALL_DEV_RTP_PRIMARY:
        if (vqec_chan_primary_session_dp_id(chan) != attrib->device_id ||
            !vqec_chan_get_primary_session(chan)) {
            snprintf(msg_buf, 
                     VQEC_LOGMSG_BUFSIZE,
                     "Upcall channel %s, primary device id mismatch %d/%d/%p",
                     vqec_chan_print_name(chan),
                     vqec_chan_primary_session_dp_id(chan),                     
                     attrib->device_id,
                     vqec_chan_get_primary_session(chan));
            syslog_print(VQEC_ERROR, msg_buf);
            upc_data->upcall_ack_errors++;
            return;
        }
        if (irq_resp->irq_reason_code & 
            VQEC_DP_UPCALL_RTP_SRC_EVENTS) {
            /* RTP source event for the primary session. */
            /* sa_ignore{value null-checked above} NO_NULL_CHK(2). */
            MCALL(vqec_chan_get_primary_session(chan), process_upcall_event,
                  &irq_resp->response.rtp_dev.src_table);
        }
        break;

    case VQEC_DP_UPCALL_DEV_RTP_REPAIR:
        if (vqec_chan_repair_session_dp_id(chan) != attrib->device_id ||
            !vqec_chan_get_repair_session(chan)) {
            snprintf(msg_buf, 
                     VQEC_LOGMSG_BUFSIZE,
                     "Upcall channel %s, repair device id mismatch %d/%d/%p",
                     vqec_chan_print_name(chan),
                     vqec_chan_repair_session_dp_id(chan),
                     attrib->device_id,
                     vqec_chan_get_repair_session(chan));
            syslog_print(VQEC_ERROR, msg_buf);
            upc_data->upcall_ack_errors++;
            return;
        }
        if (irq_resp->irq_reason_code & 
            VQEC_DP_UPCALL_RTP_SRC_EVENTS) {
            /* RTP source event for the repair session. */
            /* sa_ignore{value null-checked above} NO_NULL_CHK(2). */
            MCALL(vqec_chan_get_repair_session(chan), process_upcall_event,
                  &irq_resp->response.rtp_dev.src_table);
        }
        break;

    case VQEC_DP_UPCALL_DEV_DPCHAN: 
#if HAVE_FCC
        if (VQEC_DP_UPCALL_REASON_ISSET(irq_resp->irq_reason_code,
                                        VQEC_DP_UPCALL_REASON_CHAN_RCC_NCSI)) {
            /* NCSI event. */
            vqec_chan_upcall_ncsi_event(chan, 
                                        &irq_resp->response.chan_dev.ncsi_data);
        }
        if (VQEC_DP_UPCALL_REASON_ISSET(irq_resp->irq_reason_code,
                                        VQEC_DP_UPCALL_REASON_CHAN_RCC_ABORT)) {
            /* RCC abort event. */
            vqec_chan_upcall_abort_event(chan);
        }
        if (VQEC_DP_UPCALL_REASON_ISSET(irq_resp->irq_reason_code,
                                   VQEC_DP_UPCALL_REASON_CHAN_FAST_FILL_DONE)) {
            /* Fast fill done event. */
            vqec_chan_upcall_fast_fill_event(chan,
                                    &irq_resp->response.chan_dev.fast_fill_data);
        }
        if (VQEC_DP_UPCALL_REASON_ISSET(irq_resp->irq_reason_code,
                                   VQEC_DP_UPCALL_REASON_CHAN_BURST_DONE)) {
            /* Fast fill done event. */
            vqec_chan_upcall_burst_done_event(chan);
        }
        if (VQEC_DP_UPCALL_REASON_ISSET(irq_resp->irq_reason_code,
                                   VQEC_DP_UPCALL_REASON_CHAN_FEC_UPDATE)) {
            /* FEC update event. */
            vqec_chan_upcall_fec_update_event(chan,
                &irq_resp->response.chan_dev.fec_update_data);
        }

#endif /* HAVE_FCC */

        if (VQEC_DP_UPCALL_REASON_ISSET(
                irq_resp->irq_reason_code,
                VQEC_DP_UPCALL_REASON_CHAN_PRIM_INACTIVE)) {
            /* primary inactive event. used for vod session reconnect */
            vqec_chan_upcall_prim_inactive_event(chan);
        }

        break;

    default:
        snprintf(msg_buf, 
                 VQEC_LOGMSG_BUFSIZE,
                 "Upcall channel %s, unsupported device class %d",
                 vqec_chan_print_name(chan),
                 attrib->device);            
        syslog_print(VQEC_ERROR, msg_buf);
        upc_data->upcall_ack_errors++;
        break;
    }
}


/**---------------------------------------------------------------------------
 * Process the event data for all of a dataplane channel's device which
 * is retrieved via a poll IPC.
 * 
 * @param[in] chan Pointer to the channel instance.
 * @param[in] irq_poll The batch event response data retrieved from 
 * the dataplane.
 *---------------------------------------------------------------------------*/ 
static void
vqec_upcall_chan_event_batch (vqec_chan_t *chan, vqec_dp_irq_poll_t *irq_poll)
{
    uint32_t i;
    vqec_upcall_chan_dev_attrib_t attrib;

    memset(&attrib, 0, sizeof(attrib));
    attrib.chan_generation_num = irq_poll->chan_generation_num;

    for (i = 0; i < VQEC_DP_UPCALL_DEV_MAX; i++) {
        if (irq_poll->response[i].ack_error != VQEC_DP_ERR_OK &&
            irq_poll->response[i].ack_error != VQEC_DP_ERR_NOPENDINGIRQ) {
            /* error polling the device */
            syslog_print(VQEC_CHAN_ERROR,
                         vqec_chan_print_name(chan), 
                         "encountered error in poll of a IRQ device");
            VQEC_DEBUG(VQEC_DEBUG_UPCALL,
                       "channel %s, error in poll of IRQ device %d\n",
                       vqec_chan_print_name(chan), 
                       i+1);
            continue;
        }

        if (!irq_poll->response[i].irq_reason_code) {
            /* No events generated by this particular device. */
            continue;
        }

        /* deliver upcall event data to objects / entities in the channel. */
        attrib.device = i + 1;
        attrib.device_id = irq_poll->response[i].device_id;
        vqec_upcall_chan_event(chan, &attrib, &irq_poll->response[i]);
    } 
}


/**---------------------------------------------------------------------------
 * Acknowledge an IRQ, and retrieve the event data associated with it.
 *
 * @param[in] chid Identifier of the channel for which the IRQ
 * is acknowledged.
 * @param[in] device Type of the device for which the IRQ is being acknowledged.
 * @param[in] device_id Dataplane identifier of the device for
 * which the IRQ is acknowledged.
 * @param[out] resp Response data for the IRQ event.
 * @param[out] booelan Returns true if the ACK succeeds.
 *---------------------------------------------------------------------------*/ 
static boolean
vqec_upcall_chan_ack_irq (vqec_dp_chanid_t chid,
                          vqec_dp_upcall_device_t device,
                          vqec_dp_streamid_t device_id, 
                          vqec_dp_irq_ack_resp_t *resp)
{
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    boolean ret = FALSE;
    vqec_dp_error_t err;

    err = vqec_dp_chan_ack_upcall_irq(chid,
                                      device,
                                      device_id,
                                      resp);

    if (err == VQEC_DP_ERR_OK) {
        ret =TRUE;
    } else {
        snprintf(msg_buf, 
                 VQEC_LOGMSG_BUFSIZE,
                 "Upcall IRQ acknowledgment failure, chid %d, reason-code %d",
                 chid, err);        
        syslog_print(VQEC_ERROR, msg_buf);
    }

    return (ret);
}


/**---------------------------------------------------------------------------
 * Poll / clear all pending IRQs in the dataplane , and retrieve the event data
 * associated with them.
 *
 * @param[in] chid Dataplane identifier of the channel being polled.
 * @param[out] poll Poll API data structure.
 * @param[out] boolean Returns TRUE if the poll action succeeds.
 *---------------------------------------------------------------------------*/ 
static boolean
vqec_upcall_chan_poll_irq (vqec_dp_chanid_t chid,
                           vqec_dp_irq_poll_t *poll) 
{
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    boolean ret = FALSE;
    vqec_dp_error_t err;

    err = vqec_dp_chan_poll_upcall_irq(chid, poll);
    if (err == VQEC_DP_ERR_OK) {
        ret =TRUE;
    } else {
        snprintf(msg_buf, 
                 VQEC_LOGMSG_BUFSIZE,
                 "Upcall IRQ poll failure, chid %d, reason-code %d",
                 chid, err); 
        syslog_print(VQEC_ERROR, msg_buf);
    }

    return (ret);
}


/**---------------------------------------------------------------------------
 * Process an upcall IRQ event from the dataplane.
 * 
  *    -------------
 *    | API version (fixed length)
 *    -------------
 *    | IRQ event structure (fixed length)
 *    -------------
 *
 * @param[in] p_buf Pointer to the input data buffer.
 * @param[in] len Length of the input.
 *---------------------------------------------------------------------------*/ 
static void 
vqec_upcall_process_irq_event (char *p_buf, uint16_t len)
{
    char msg_buf[VQEC_LOGMSG_BUFSIZE];
    vqec_upcall_chan_dev_attrib_t attrib;
    vqec_dp_irq_poll_t irq_poll;
    vqec_dp_irq_ack_resp_t irq_resp;
    vqec_dp_upcall_irq_event_t irq_ev;
    uint32_t cp_chid;
    vqec_chan_t *chan;
    struct vqec_upcall_sock_desc *desc;
    vqec_dp_api_version_t version;

    desc = &s_upcall_irqsock;
    desc->upcall_events++;

    /* 
     * Length checking failure might be a valid error condition, e.g. when
     * the event is corrupted during transit. 
     */
    if (len != (sizeof(vqec_dp_upcall_irq_event_t) + 
                sizeof(vqec_dp_api_version_t))) {
        snprintf(msg_buf, 
                 VQEC_LOGMSG_BUFSIZE,
                 "dataplane upcall event length checking failed: "
                 "event length %d not equal to %d",
                 len, 
                 sizeof(vqec_dp_upcall_irq_event_t) + 
                 sizeof(vqec_dp_api_version_t));
        syslog_print(VQEC_ERROR, msg_buf);
        desc->upcall_error_events++;
        return;
    }

    version = *(vqec_dp_api_version_t *)p_buf;
    if (version != VQEC_DP_API_VERSION) {
        syslog_print(VQEC_ERROR,
                     "API version mismatch in IRQ event from dataplane");
        VQEC_DEBUG(VQEC_DEBUG_UPCALL, 
                   "UPC: Rx IRQ event with bad API version (%d/%d)\n",
                   version, VQEC_DP_API_VERSION);
        desc->upcall_error_events++;
        return;        
    }
    p_buf += sizeof(vqec_dp_api_version_t);
    

    /* Copy event to the local IRQ buffer. */
    memcpy(&irq_ev, p_buf, sizeof(irq_ev));

    /*
     * Parse the event. The event contains the channel index, from which
     * we can retrieve the channel structure, and deliver the upcall to 
     * the primary or repair session in the channel structure.
     */
    VQEC_DEBUG(VQEC_DEBUG_UPCALL,
               " UPC: Generation #: %x, dp channel: %x, cp channel: %x, "
               " channel genid: %d, dev: %d, devid: %d\n",
               irq_ev.upcall_generation_num, 
               irq_ev.chid,
               irq_ev.cp_handle,
               irq_ev.chan_generation_num,
               irq_ev.device,
               irq_ev.device_id);

    /* From control-plane channel index to channel ptr. */
    cp_chid = irq_ev.cp_handle;
    if (!(chan = vqec_chanid_to_chan(cp_chid))) {

        snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                 "failed to retrieve the channel using the "
                 "cp channel index (%d) from upcall event", cp_chid);
        syslog_print(VQEC_ERROR, msg_buf);

        desc->upcall_error_events++; 
        return;
    }

    /* Ensure that the device identifier is valid. */
    if (irq_ev.device < VQEC_DP_UPCALL_DEV_MIN ||
        irq_ev.device > VQEC_DP_UPCALL_DEV_MAX) {

        snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                 "channel %s, upcall message for bad device %d",
                 vqec_chan_print_name(chan),
                 irq_ev.device);
        syslog_print(VQEC_ERROR, msg_buf);

        desc->upcall_error_events++; 
        return;
    }

    /*
     * Ensure that the message is not for a different incarnation
     * of this control-plane channel identifier. 
     */
    if (!vqec_upcall_chan_generation_id_verify(chan, 
                                               irq_ev.chan_generation_num)) {

        snprintf(msg_buf, VQEC_LOGMSG_BUFSIZE,
                 "channel %s, mismatch between channel generations %d:%d\n",
                 vqec_chan_print_name(chan), 
                 vqec_upcall_chan_get_generation_id(chan),
                 irq_ev.chan_generation_num);
        syslog_print(VQEC_ERROR, msg_buf);

        desc->upcall_error_events++; 
        return;
    }
    
    /*
     * Check for message loss. If there is message loss, then we need
     * to poll for all IRQ's (all pending events) in the data-plane.
     */
    if (!vqec_upcall_chan_update_msg_generation_num(
            chan, 
            irq_ev.upcall_generation_num)) {
        
        /* lost IRQ - scan all the devices on the channel for IRQs. */
        desc->upcall_lost_events++;

        memset(&irq_poll, 0, sizeof(irq_poll));
        if (!vqec_upcall_chan_poll_irq(irq_ev.chid, 
                                       &irq_poll)) {
            
            /* Failed to read IRQ state from dataplane. */
            snprintf(msg_buf, 
                     VQEC_LOGMSG_BUFSIZE,
                     "failed to poll IRQ state in the dataplane for "
                     "dataplane channel identifier (%u)\n", irq_ev.chid);
            syslog_print(VQEC_ERROR, msg_buf);

            desc->upcall_ack_errors++;
            return; 
        }

        /* 
         * We had a generation number mismatch -  simply update to the
         * sequence number returned received in the poll event; it is possible 
         * that the sequence number discrepancy will persist for a few 
         * IRQ events if more IRQ events have been written to the socket, but 
         * will resolve itself once those events have been processed. (It is
         * assumed that the frequency of IRQ events is very small.)
         */
        vqec_chan_upc_data_ptr(chan)->upcall_last_generation_num =
            irq_poll.upcall_generation_num;

        /* deliver all upcall events data to objects / entities in the channel. */
        vqec_upcall_chan_event_batch(chan, &irq_poll);

    } else {
        
        /* acknowledge IRQ, i.e., fetch the data associated with the event */
        memset(&irq_resp, 0, sizeof(irq_resp));
        if (!vqec_upcall_chan_ack_irq(irq_ev.chid, 
                                      irq_ev.device, irq_ev.device_id, &irq_resp)) {

            /* Failed to read IRQ state from dataplane. */
            snprintf(msg_buf, 
                     VQEC_LOGMSG_BUFSIZE,
                     "failed to acknowledge IRQ to the dataplane for "
                     "dataplane channel identifier (%u)\n", irq_ev.chid);
            syslog_print(VQEC_ERROR, msg_buf);

            s_upcall_irqsock.upcall_ack_errors++;
            return;
        }
        
        /* deliver upcall event data to objects / entities in the channel. */
        memset(&attrib, 0, sizeof(attrib));
        attrib.chan_generation_num = irq_ev.chan_generation_num;
        attrib.device = irq_ev.device;
        attrib.device_id = irq_ev.device_id; 
        vqec_upcall_chan_event(chan, &attrib, &irq_resp);
    }
}


/**---------------------------------------------------------------------------
 * Process a NAT packet from the dataplane. The packet is sanity checked and
 * then delivered to the channel for further processing.
 * NOTE: The frequency of NAT packets should be very low, hence
 * per packet debugs are seen as ok.
 * 
 *    -------------
 *    | API version (fixed length)
 *    -------------
 *    | Packet header (fixed length)
 *    -------------
 *    | Packet contents (variable length)
 *    -------------
 *
 * @param[in] nat_pak Pointer to the NAT packet.
 * @param[in] len Length of the NAT packet.
 *---------------------------------------------------------------------------*/ 
void
vqec_upcall_process_nat_pak (char *nat_pak, uint16_t len)
{
    vqec_dp_pak_hdr_t *pak_hdr;
    struct vqec_upcall_sock_desc *desc;
    vqec_dp_api_version_t version;

    desc = &s_upcall_pak_ejectsock;
    desc->upcall_events++;

    if (len < (sizeof(*pak_hdr) + sizeof(vqec_dp_api_version_t))) {
        VQEC_DEBUG(VQEC_DEBUG_UPCALL, 
                   "UPC: Rx nat packet with bad header len (%d/%d)\n",
                   len, sizeof(*pak_hdr));
        desc->upcall_error_events++;
        return;
    }

    version = *(vqec_dp_api_version_t *)nat_pak;
    if (version != VQEC_DP_API_VERSION) {
        syslog_print(VQEC_ERROR,
                     "API version mismatch in packet header from dataplane");
        VQEC_DEBUG(VQEC_DEBUG_UPCALL, 
                   "UPC: Rx nat packet with bad API version (%d/%d)\n",
                   version, VQEC_DP_API_VERSION);
        desc->upcall_error_events++;
        return;        
    }
    nat_pak += sizeof(vqec_dp_api_version_t);
    len -= sizeof(vqec_dp_api_version_t);

    pak_hdr = (vqec_dp_pak_hdr_t *)nat_pak;
    if (pak_hdr->length  != (len - sizeof(*pak_hdr))) {
        VQEC_DEBUG(VQEC_DEBUG_UPCALL, 
                   "UPC: Rx nat packet with bad content len (%d/%d)\n",
                   pak_hdr->length, len - sizeof(*pak_hdr));
        desc->upcall_error_events++;
        return;
    }

    nat_pak += sizeof(*pak_hdr);
    len -= sizeof(*pak_hdr);
    VQEC_DEBUG(VQEC_DEBUG_UPCALL, 
               "UPC: Rx nat packet, channel id %x, length %d, timestamp %llu\n",
               pak_hdr->cp_handle,
               len, 
               TIME_GET_A(msec, pak_hdr->rx_timestamp));

    if (!vqec_chan_eject_rtp_nat_pak(pak_hdr->cp_handle,
                                     pak_hdr->dp_chanid, 
                                     nat_pak, 
                                     len, 
                                     pak_hdr->is_id,
                                     pak_hdr->src_addr,
                                     pak_hdr->src_port)) {
        VQEC_DEBUG(VQEC_DEBUG_UPCALL, 
                   "UPC: Rx nat packet, insertion failed for channel id %x\n",
                   pak_hdr->cp_handle, len);
        desc->upcall_error_events++;
    }
}



/**---------------------------------------------------------------------------
 * Event handler for upcall IRQ events generated by the dataplane. Each
 * channel has its own set of IRQs (messages), one for events on the 
 * primary session, one for the repair session, and one for events internal
 * to the dp channel.
 *
 * @param[in] ev Pointer to the libevent event.
 * @param[in] fd File descriptor of the socket.
 * @param[in] event Socket event.
 * @param[in] unused_dptr Unused.
 *---------------------------------------------------------------------------*/ 
#define VQEC_DP_UPCALL_EV_MAX_SIZE \
    (sizeof(vqec_dp_api_version_t) + sizeof(vqec_dp_upcall_irq_event_t))

static void
vqec_upcall_event_irq (const vqec_event_t * const ev,
                       int32_t fd, int16_t event, void *unused_dptr) 
{
    /* +1 will help detect expected size mismatch / truncation */
    char buf[VQEC_DP_UPCALL_EV_MAX_SIZE + 1];  
    struct sockaddr_in from;
    socklen_t  len = sizeof(from);

    while (TRUE) {

        memset(&from, 0, sizeof(from));
        ssize_t n = recvfrom(fd, 
                             buf, 
                             sizeof(buf), 
                             0, 
                             (struct sockaddr *)&from, 
                             &len);
        if (n > 0) {
            vqec_upcall_process_irq_event(buf, n);
        } else if (n == 0) {
            break;
        } else {
            if (errno != EAGAIN) {
                vqec_recv_sock_perror("recvfrom", errno);
            }
            break;
        }
    }
}


/**---------------------------------------------------------------------------
 * Event handler for "punted" or "ejected" packets from the dataplane.
 * In the current implementation only NAT packets are supported. 
 *
 * @param[in] ev Pointer to the libevent event.
 * @param[in] fd File descriptor of the socket.
 * @param[in] event Socket event.
 * @param[in] unused_dptr Unused.
 *---------------------------------------------------------------------------*/ 
static void
vqec_upcall_event_pak_eject (const vqec_event_t * const ev,
                             int32_t fd, int16_t event, void *unused_dptr) 
{
    struct sockaddr_in from;
    socklen_t  len = sizeof(from);
    
    while (TRUE) {

        memset(&from, 0, sizeof(from));
        ssize_t n = recvfrom(fd, 
                             s_upcall_pak_ejectsock.pak_buf, 
                             s_upcall_pak_ejectsock.buf_len, 
                             0, 
                             (struct sockaddr *)&from, 
                             &len);
        if (n > 0) {
            vqec_upcall_process_nat_pak(s_upcall_pak_ejectsock.pak_buf, n);
        } else if (n == 0) {
                break;
        } else {
            if (errno != EAGAIN) {                
                vqec_recv_sock_perror("recvfrom", errno);
            }
            break;
        }
    }
}




/**---------------------------------------------------------------------------
 * Setup a socket which receives unsolicited messages from the dataplane,
 * and register the socket with libevent.
 *
 * @param[in] un_sock_path Pathname for the named socket.
 * @param[in] desc Upcall socket data descriptor.
 * @param[in] handler Socket event handler.
 * @param[out] vqec_error_t Returns VQEC_OK on success.
 *---------------------------------------------------------------------------*/ 
typedef void (*upcall_evt_handler)(const vqec_event_t * const, int32_t, 
                                   int16_t, void *);
    
static vqec_error_t
vqec_setup_upc_socket (char *un_sock_path, 
                       struct vqec_upcall_sock_desc *desc,
                       upcall_evt_handler handler)
{
    struct sockaddr_un *name;
    size_t len;

    memset(desc, 0, sizeof(*desc));
    name = &desc->sock;
    name->sun_family = AF_LOCAL;
    len = strlcpy(name->sun_path, un_sock_path, 
                  sizeof(name->sun_path) - 1);
    if (len >= (sizeof(name->sun_path) - 1)) {
        syslog_print(VQEC_ERROR, 
                     "Unix socket name truncated!");
        return (VQEC_ERR_SYSCALL);        
    }

    desc->fd = vqec_create_named_socket(name);
    if (desc->fd == -1) {
        syslog_print(VQEC_ERROR, 
                     "Error in upcall socket syscalls");
        return (VQEC_ERR_SYSCALL);
    }

    /* Add a socket to libevent. */
    if ((!vqec_event_create(&desc->ev, VQEC_EVTYPE_FD,
                            VQEC_EV_READ | VQEC_EV_RECURRING,
                            handler, desc->fd, NULL)) ||
        (!vqec_event_start(desc->ev, NULL))) {
        
        if (desc->ev) {
            vqec_event_destroy(&desc->ev);
        }
        close(desc->fd);
        desc->fd = -1;
        syslog_print(VQEC_MALLOC_FAILURE,
                     "allocation of upcall event failed");
        return (VQEC_ERR_MALLOC);
    }

    desc->is_fd_valid = TRUE;
    return (VQEC_OK);
}


/**---------------------------------------------------------------------------
 * Setup IRQ socket.
 * 
 * @param[out] vqec_error_t Returns VQEC_ERR_OK on success.
 *---------------------------------------------------------------------------*/ 
#define PID_ASC_LEN 16
static vqec_error_t
vqec_upcall_setup_irqsock (void)
{
    vqec_error_t err = VQEC_OK;
    char name[sizeof(s_upcall_irqsock.sock.sun_path)], pid_asc[PID_ASC_LEN];
    size_t len;
    pid_t pid;
    vqec_dp_upcall_sockaddr_t addr;
    struct vqec_upcall_sock_desc *desc;

    desc = &s_upcall_irqsock;
    pid = getpid();
    len = snprintf(pid_asc, sizeof(pid_asc), "%d", pid);
    if (len >= sizeof(pid_asc)) {
        syslog_print(VQEC_ERROR, 
                     "Implementation error: pid string-write buffer is too small");
        err = VQEC_ERR_INTERNAL;
        goto done;
    }

    /* setup IRQ socket name. */
    len = strlcpy(name, VQEC_UPCALL_SOCKET_BASEDIR, sizeof(name));
    len = strlcat(name, VQEC_UPCALL_SOCKET_IRQEVENTS, sizeof(name));
    len = strlcat(name, pid_asc, sizeof(name));
    if (len >= sizeof(name)) {
        syslog_print(VQEC_ERROR, 
                     "Unix socket name length exceeds maximum bound");
        err = VQEC_ERR_INTERNAL;
        goto done;
    }
        
    /* establish IRQ socket. */
    err = vqec_setup_upc_socket(name, desc, vqec_upcall_event_irq);
    if (err != VQEC_OK) {
        goto done;
    }
    
    /* create IRQ socket in dataplane. */
    addr.sock = desc->sock;
    if (vqec_dp_create_upcall_socket(VQEC_UPCALL_SOCKTYPE_IRQEVENT,
                                     &addr, sizeof(addr)) != VQEC_DP_ERR_OK) {
        syslog_print(VQEC_ERROR, 
                     "Addition of IRQ upcall socket to dataplane failed");
        err = VQEC_ERR_INTERNAL;
        goto done;
    }

  done:
    return (err);
}


/**---------------------------------------------------------------------------
 * Setup the packet "punt" or "eject" socket, which receives NAT packets
 * (in this implementation) that were received on RTP session(s) in the 
 * dataplane.
 *
 * @param[in] max_paksize Maximum packet size bound for "punted"
 * or "ejected" packets received from the dataplane.
 * @param[out] vqec_error_t Returns VQEC_OK if the socket is setup
 * successfully, error code otherwise.
 *---------------------------------------------------------------------------*/ 
static vqec_error_t
vqec_upcall_setup_pak_ejectsock (uint32_t max_paksize)
{
    vqec_error_t err = VQEC_OK;
    char name[sizeof(s_upcall_pak_ejectsock.sock.sun_path)], pid_asc[PID_ASC_LEN];
    size_t len;
    pid_t pid;
    vqec_dp_upcall_sockaddr_t addr;
    struct vqec_upcall_sock_desc *desc;

    desc = &s_upcall_pak_ejectsock;
    pid = getpid();
    len = snprintf(pid_asc, sizeof(pid_asc), "%d", pid);
    if (len >= sizeof(pid_asc)) {
        syslog_print(VQEC_ERROR, 
                     "Implementation error: pid string-write buffer is too small");
        err = VQEC_ERR_INTERNAL;
        goto done;
    }
    
    len = strlcpy(name, VQEC_UPCALL_SOCKET_BASEDIR, sizeof(name)); 
    len = strlcat(name, VQEC_UPCALL_SOCKET_RX_PAKEJECT, sizeof(name));
    len = strlcat(name, pid_asc, sizeof(name));
    if (len >= sizeof(name)) {
        syslog_print(VQEC_ERROR, 
                     "Unix socket name length exceeds maximum bound");
        err = VQEC_ERR_INTERNAL;
        goto done;
    }
    
    /* establish packet eject socket. */
    err = vqec_setup_upc_socket(name, desc, vqec_upcall_event_pak_eject);
    if (err != VQEC_OK) {
        goto done;
    }
    
    /* create packet eject socket in dataplane. */
    addr.sock = desc->sock;
    if (vqec_dp_create_upcall_socket(VQEC_UPCALL_SOCKTYPE_PAKEJECT,
                                     &addr, sizeof(addr)) != VQEC_DP_ERR_OK) {
        syslog_print(VQEC_ERROR, 
                     "Addition of eject upcall socket to dataplane failed");
        err = VQEC_ERR_INTERNAL;
        goto done;
    }

    /* Allocate private packet buffer. */
    desc->pak_buf = malloc(max_paksize + 
                           sizeof(vqec_dp_pak_hdr_t) +
                           sizeof(vqec_dp_api_version_t));
    if (!desc->pak_buf) {
        syslog_print(VQEC_MALLOC_FAILURE,
                     "allocation of upcall packet buffer failed");
        err = VQEC_ERR_MALLOC;
        goto done;
    }
    desc->buf_len = max_paksize +
        sizeof(vqec_dp_pak_hdr_t) +
        sizeof(vqec_dp_api_version_t);

  done:
    return (err);
}


/**---------------------------------------------------------------------------
 * Close existing unix sockets. The inodes created in the "/tmp" directory
 * are destroyed via the "unlink" system call.
 *---------------------------------------------------------------------------*/ 
void
vqec_upcall_close_unsocks (void)
{
    if (s_upcall_irqsock.is_fd_valid) {
        unlink(s_upcall_irqsock.sock.sun_path);
        close(s_upcall_irqsock.fd);
    }
    if (s_upcall_irqsock.ev) {
        vqec_event_destroy(&s_upcall_irqsock.ev);
    }
    if (s_upcall_pak_ejectsock.is_fd_valid) {
        unlink(s_upcall_pak_ejectsock.sock.sun_path);
        close(s_upcall_pak_ejectsock.fd);
    }
    if (s_upcall_pak_ejectsock.ev) {
        vqec_event_destroy(&s_upcall_pak_ejectsock.ev);
    }
    if (s_upcall_pak_ejectsock.pak_buf) {
        free(s_upcall_pak_ejectsock.pak_buf);
    }
    
    memset(&s_upcall_irqsock, 0, sizeof(s_upcall_irqsock));
    memset(&s_upcall_pak_ejectsock, 0, sizeof(s_upcall_pak_ejectsock));

    if (vqec_dp_close_upcall_sockets() != VQEC_DP_ERR_OK) {
        syslog_print(VQEC_ERROR, 
                     "IPC failure in closing upcall sockets");
    }

}


/**---------------------------------------------------------------------------
 * Upcall socket setup.
 *
 * @param[in] max_paksize Maximum packet size bound for "punted"
 * or "ejected" packets received from the dataplane.
 * @param[out] vqec_error_t Returns VQEC_OK if the sockets are setup
 * successfully, error code otherwise.
 *---------------------------------------------------------------------------*/ 
vqec_error_t
vqec_upcall_setup_unsocks (uint32_t max_paksize)
{
    vqec_error_t err = VQEC_ERR_INVALIDARGS;

    if (!max_paksize) {
        return (err);
    }

    err = vqec_upcall_setup_irqsock();
    if (err != VQEC_OK) {
        vqec_upcall_close_unsocks();
        return (err);
    }

    err = vqec_upcall_setup_pak_ejectsock(max_paksize);
    if (err != VQEC_OK) {
        vqec_upcall_close_unsocks();
        return (err);
    }

    return (err);
}


/**---------------------------------------------------------------------------
 * Implementation of the global upcall display per socket descriptor.
 *
 * @param[in] desc Upcall socket descriptor state.
 *---------------------------------------------------------------------------*/ 
static void
vqec_upcall_display_desc_state (struct vqec_upcall_sock_desc *desc)
{
    CONSOLE_PRINTF(" Sock name:                 %-32s\n",
                   desc->sock.sun_path);
    CONSOLE_PRINTF(" Total Events:              %u\n",
                   desc->upcall_events);
    CONSOLE_PRINTF(" Lost Events:               %u\n",
                   desc->upcall_lost_events);
    CONSOLE_PRINTF(" Error Events:              %u\n",
                   desc->upcall_error_events);
    CONSOLE_PRINTF(" Ack Errors:                %u\n",
                   desc->upcall_ack_errors);
}


/**---------------------------------------------------------------------------
 * Display global upcall state.
 *---------------------------------------------------------------------------*/ 
void
vqec_upcall_display_state (void)
{
    struct vqec_upcall_sock_desc *desc;

    CONSOLE_PRINTF("--- IRQ events ---\n");
    desc = &s_upcall_irqsock;
    vqec_upcall_display_desc_state(desc);

    CONSOLE_PRINTF("\n--- NAT events ---\n");
    desc = &s_upcall_pak_ejectsock;
    vqec_upcall_display_desc_state(desc);
}


/**---------------------------------------------------------------------------
 * Get IRQ event statistics - used for unit-tests.
 *
 * @param[in] stats Pointer to the stats structure.
 *---------------------------------------------------------------------------*/ 
void
vqec_upcall_get_irq_stats (vqec_upcall_ev_stats_t *stats)
{
    if (stats) {
        stats->upcall_events = s_upcall_irqsock.upcall_events;
        stats->upcall_lost_events = s_upcall_irqsock.upcall_lost_events;
        stats->upcall_error_events = s_upcall_irqsock.upcall_error_events;
        stats->upcall_ack_errors = s_upcall_irqsock.upcall_ack_errors;
    }
}
