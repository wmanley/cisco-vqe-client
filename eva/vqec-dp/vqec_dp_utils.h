/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Dataplane utility methods.
 *
 * Documents: 
 *
 *****************************************************************************/

#ifndef __VQEC_DP_UTILS_H__
#define __VQEC_DP_UTILS_H__
 
#include "vqec_dp_api.h"
#include "vqec_seq_num.h"
#include "vqec_dp_tlm.h"

/**
 * If a packet is to be dropped, this method will return a
 * true, otherwise it will return a false. 
 */
boolean 
vqec_dp_drop_sim_drop(vqec_seq_num_t seqnum);

/**
 * Write an IRQ event to the upcall IRQ socket.
 */
boolean
vqec_dp_ipcserver_tx_upcall_irq(vqec_dp_upcall_irq_event_t *ev);

/**
 * Write an upcall event to the upcall socket.
 */
boolean
vqec_dp_ipcserver_eject_pak(vqec_dp_pak_hdr_t *hdr, vqec_pak_t *pak);

/**---------------------------------------------------------------------------
 * Detect the encapsulation type of a packet buffer, as either RTP or UDP MPEG.
 * This API does not check the entire RTP header, but merely the first two
 * bits.  If the packet is detected as RTP, the full header should go on to be
 * validated.
 *
 * @param[in] char  Pointer to beginning of packet buffer.
 * @param[out] vqec_dp_encap_type_t  Returns encapsulation type.
 *---------------------------------------------------------------------------*/ 
static inline vqec_dp_encap_type_t
vqec_dp_detect_encap (char *buff)
{
    /* analyze the bits in the first byte of the payload */
    if ((buff[0] & 0xc0) == (0x80)) {
        /* the first two bits of the payload are 0b10; packet is likely RTP */
        return (VQEC_DP_ENCAP_RTP);
    } else if (buff[0] == 0x47) {
        /* the first byte is the MPEG sync byte; packet is UDP MPEG */
        return (VQEC_DP_ENCAP_UDP);
    } else {
        /* packet payload is of unrecognized encapsulation */
        return (VQEC_DP_ENCAP_UNKNOWN);
    }
}

#endif /* __VQEC_DP_UTILS_H__ */
