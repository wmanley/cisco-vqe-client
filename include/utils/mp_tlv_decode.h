/*------------------------------------------------------------------
* Mpeg Parser.  
* Definitions for Decode TLV Data
* May 2006, Jim Sullivan
*
* Copyright (c) 2006-2008 by cisco Systems, Inc.
* All rights reserved.
*------------------------------------------------------------------
*/
#ifndef __MP_TLV_DECODE_H__
#define __MP_TLV_DECODE_H__

#include <utils/queue_plus.h>
#include "mp_tlv.h"
#include "vam_types.h"

#define MP_TLV_PRINT(args...) VQE_FPRINTF(stdout, args)
#define MP_TLV_ASSERT(args...) ASSERT(args)

#define MP_NULL_PACKET_PID 0x1FFF

/*
 * Maximum number of TS Packets which can result from the decoding of a
 * single RCC (TSRAP TLV in RTCP APP Packet).  
 *     1  PAT
 *     1  PMT
 *     1  Sequence Header
 *     6  2 ECMs * 3 CA PIDs
 *  +  1  PCR
 *  ========================
 *     10
 */
#define MP_MAX_DECODED_TSRAP_TLV 10

/*
 * Fudge factor, in case a given TLV (above) decodes into more than
 * one TS packet.
 */
#define MP_TSRAP_TLV_FUDGE 5

/*
 * Maximum number of TS packets which can result from decoding
 * a single TSRAP TLV (e.g. a PMT TLV, ECM TLV, etc.)
 */
#define MP_MAX_TSPAKS_PER_TLV 7

/*
 * Number of TS packets which each DP Pak buffer must contain. 
 * Used to ensure number TS packets resulting from TSRAP decode
 * is a multiple of this number (if not, we add MPEG NULLs at
 * the end).
 */
#define MP_NUM_TSPAKS_PER_DP_PAK 7

/** 
* List of buffers to be filled with decoded TLV data.
* The buf_len should be a multiple of 188 Bytes.     
*/
typedef 
VQE_TAILQ_HEAD(vqe_tsrap_app_tailq_head_, mp_tlv_buflist_) 
    vqe_tsrap_app_tailq_head_t;
extern vqe_tsrap_app_tailq_head_t vqe_tsrap_app_list_head;

typedef struct mp_tlv_buflist_{
    uint32_t buf_len;	/**< Length of tspkt_buf. */ 
    uint32_t buf_rem;    /**< Bytes remaining in tspkt_buf */
    char *tspkt_buf;  	/**< Decoded TLV data is placed here */
    VQE_TAILQ_ENTRY(mp_tlv_buflist_) list_entry;
}mp_tlv_buflist_t;

/**
 * Pads buffer with NULL Transport Stream packets (if needed) to ensure
 * buffer contains a multiple of MP_NUM_TSPAKS_PER_DP_PAK TS packets.
 *
 * @param buf_list	- The buffer list entry to be filled
 * @param tspkt_cnt     - Number of NULL TS Packets to add
 * 
 * @return status
 */
mp_tlv_status_t mp_tlv_buf_pad(mp_tlv_buflist_t *buf_list, int tspkt_cnt);

/**
 * Wrapper for memcpy to seamlessly fill buf_list
 *
 * @param buf_list			- The list of buffers to be filled, updated to
 * 								current buffer
 * @param pkt_data			- TS Packet data to be placed into buf_list
 * @param data_len			- Length of packet data
 *
 * @return status
*/
int mp_tlv_buf_fill (mp_tlv_buflist_t *buf_list, uint8_t *pkt_data, 
                        int pkt_cnt);
/**
 * Allows manipulation of recreated PCR TS packets
 */
typedef struct mp_tlv_pcrknobs_ {
    uint8_t num_pcrs;   /**< number of PCRs */
    uint8_t cc;         /**< continuity count(cc) to use */
    boolean dind_flag;/**< TRUE if the discontinuity_indicator should 
                             be set on last PCR(see H.222 2.4.3.5) */
    boolean rap_flag; /**< TRUE if the random_access_indicator should 
                             be set on the first PCR */
}mp_tlv_pcrknobs_t;

typedef struct mp_tlv_mpegknobs_ {
    mp_tlv_pcrknobs_t pcr_knobs; 
    uint8_t num_patpmt;	/**< Number of PAT/PMT repititions present in output */
}mp_tlv_mpegknobs_t;

/** 
 * Callback functions called during TLV decoding. These can be used for 
 * additional processing of TSRAP data(i.e. writing directly to hardware
 * registers). 
 */
typedef struct mp_tlv_decodecb_ {
    void (*pat_cb)(uint8_t *sect_ptr, uint32_t sect_len, void* data);
    void (*pmt_cb)(uint8_t *sect_ptr, uint32_t sect_len, uint16_t sect_pid,
                   void* data);
    void (*pcr_cb)(unsigned long long pcr_base, uint16_t pcr_ext, void* data);
    void (*pts_cb)(unsigned long long pts_base, void* data);
    void (*dts_cb)(unsigned long long dts_base, void* data);
    void (*seq_cb)(uint8_t *sect_ptr, uint32_t sect_len, void* data);
    void (*sps_cb)(uint8_t *sect_ptr, uint32_t sect_len, void* data);
    void (*pps_cb)(uint8_t *sect_ptr, uint32_t sect_len, void* data);
    void (*sei_cb)(uint8_t *sect_ptr, uint32_t sect_len, void* data);
    void (*ecm_cb)(uint8_t *sect_ptr, uint32_t sect_len, void* data);
    void (*emm_cb)(uint8_t *sect_ptr, uint32_t sect_len, void* data);
    void (*cat_cb)(uint8_t *sect_ptr, uint32_t sect_len, void* data);
    void (*pid_cb)(mp_tlv_pidlist_t *pidlist_ptr, void* data);
}mp_tlv_decodecb_t;

 /**
 *  mp_tlv_to_tspkts
 *  
 * Decode TLV data and generate MPEG TS packets that can be sent to a
 * MPEG System Decoder
 *  
 * @param tlvdata_ptr    - Pointer to the TLV encoded data
 * @param tlvdata_len   - Length of the TLV data
 * @param knobs_ptr     - A pointer to a control structure that allows the
 * caller to manipulate the TSRAP data. This can be used to generate MPEG TS
 * packets that are specific to a particular MPEG System Decoder. For example
 * one decoder may require three PCRs and another may require two. 
 * @param decodecb_ptr  - A pointer to callback functions to further process 
 *                 the decoded TSRAP data
 * @param app_paks_per_rcc - number of copies of the decoded APP app packet (set
 *                           of TS packets) which should be sent to the decoder.
 * @param buf_list      - Pointer to output buflist
 * @param user_data
 * @param pidlist       - Pointer to output pidlist
 * 
 * @return The total number of bytes in the buffers
 */
int mp_tlv_to_tspkts (uint8_t *tlvdata_ptr, uint32_t tlvdata_len,
                      mp_tlv_mpegknobs_t *knobs_ptr,
                      uint32_t app_paks_per_rcc,
                      mp_tlv_decodecb_t *decodecb_ptr,
                      mp_tlv_buflist_t* buf_list,
                      void * user_data,
                      mp_tlv_pidlist_t *pidlist);

/**
 *
 * Fixup the continuity counters for the TS packets in buf_list.
 *
 * @param pidlist       - Pointer to pidlist structure containing the pid
 *                          and cc data for the TS packets in buf_list
 * @param buf_list      - Pointer to the buf_list containing the TS packets
 * @param tspkt_cnt     - Number of TS packets that are in the buf_list
 */
void mp_correct_tspkt_cc(mp_tlv_pidlist_t *pidlist, mp_tlv_buflist_t *buf_list,
                        int tspkt_cnt);

/**
 *  
 * Get a continutity counter value for pid
 * 
 * @param pidlist_ptr   - Pointer to pidlist structure containing pid and
 *                          cc data
 * @param pid           - Indicates which PID to update
 * @param cc_ptr        - Value of the CC is stored in cc_ptr
 * 
 * @return true on success
 */
boolean  mp_find_cc_for_pid(mp_tlv_pidlist_t* pidlist_ptr, 
                                uint16_t pid, uint8_t *cc_ptr);

/* utility routines to dump the MPEG TS packets */

void mp_dump_tspkts (uint8_t tspkt_buf[][188], uint32_t tspkt_cnt);
void mp_dump_bytes(uint8_t *buf_ptr, uint32_t len);
int mp_dump_list(mp_tlv_buflist_t *buf_list, char *buffer, int data_len);
#endif
