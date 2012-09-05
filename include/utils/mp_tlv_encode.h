/*------------------------------------------------------------------
 * Mpeg Parser.  
 * Definitions for Encode TLV Data
 * May 2006, Jim Sullivan
 *
 * Copyright (c) 2006-2007 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#ifndef __MP_TLV_ENCODE_H__
#define __MP_TLV_ENCODE_H__

#include "mp_tlv.h"
#include <assert.h>

/** 
 * PAT, PMT, SEQ_HDR, SPS, PPS, SEI Sections 
 */
typedef struct mp_tlv_sectdsc__ {
    mp_tsraptlv_types_t  type;      /**< TLV type */
    uint16_t pid;                   /**< PID of TLV */ 
    uint16_t sect_len;              /**< Length of Section data */
    uint8_t *sect_ptr;              /**< Pointer to Section data */
}mp_tlv_sectdsc_t;

/**
 * Encode any section containing raw PSI/MPEG data into TLV data. This 
 * is used for PAT, PMT, SEQ_HDR, SPS, SPS, SEI sections 
 * @param sectdsc_ptr           - Section Data
 * @param buf_ptr               - TLV Encoded data in network byte order
 * @param buf_len               - Size of the buf_ptr
 * @return                      - Length of TLV Encoded data
 */
int mp_sect_to_tlv(const mp_tlv_sectdsc_t *sectdsc_ptr, uint8_t *buf_ptr,
                            int buf_len);
/**
 * Initialize raw data TLV section
 * @param sectdsc_ptr           - Resulting Section Data
 * @param type                  - Type of Section
 * @param pid                   - Program ID
 * @param buf_ptr               - Data included in Raw section
 * @param len                   - Length of buf_ptr
 */
static inline void mp_tlv_init_sectdsc(mp_tlv_sectdsc_t *sectdsc_ptr,
                    mp_tsraptlv_types_t type, uint16_t pid,
                    uint8_t * buf_ptr, uint16_t len)
{
    assert(sectdsc_ptr);
    sectdsc_ptr->type = type; 
    sectdsc_ptr->pid = pid;
    sectdsc_ptr->sect_len = len;
    sectdsc_ptr->sect_ptr = buf_ptr; 
    return;
}  

/**
 * PCR Sections 
 */
typedef struct mp_tlv_pcrdsc_ {
        mp_tsraptlv_types_t type;       /**< Type of TLV */
        uint16_t pid;                   /**< PID of TLV */ 
        unsigned long long pcr_base;    /**< PCR Base */
        uint16_t pcr_ext;               /**< PCR Extension */
        uint32_t tsrate;                /* TS rate in BPS(0 if unknown) */
}mp_tlv_pcrdsc_t; 

/**
 * Encode PCR section into TLV data
 * @param sectdsc_ptr           - PCR Section Data
 * @param buf_ptr               - TLV Encoded data in network byte order
 * @param buf_len               - Size of the buf_ptr
 * @return                      - Length of TLV Encoded data
 */
int mp_pcr_to_tlv(const mp_tlv_pcrdsc_t *pcrdsc_ptr, uint8_t *buf_ptr, int buf_len);

/**
 * Initialize PCR section
 * @param pcrdsc_ptr            - Resulting PCR Section Data
 * @param type                  - Type of Section
 * @param pid                   - Program ID
 * @param pcr_base              - PCR Base
 * @param pcr_ext               - PCR Extension
 * @param tsrate                - Transport Stream Rate
 */
static inline void mp_tlv_init_pcrdsc(mp_tlv_pcrdsc_t *pcrdsc_ptr, 
                             mp_tsraptlv_types_t type, uint16_t pid,
                             unsigned long long pcr_base, uint16_t pcr_ext, 
                             uint32_t tsrate)
{
    assert(pcrdsc_ptr);
    pcrdsc_ptr->type = type; 
    pcrdsc_ptr->pid = pid;
    pcrdsc_ptr->pcr_base = pcr_base;
    pcrdsc_ptr->pcr_ext = pcr_ext; 
    pcrdsc_ptr->tsrate = tsrate; 
    return;
} 

/**
 * PID List
 */

/**
 * Encode pidlist into TLV data
 * @param pidlist_ptr           - Pidlist Data 
 * @param buf_ptr               - TLV Encoded data in network byte order
 * @param buf_len               - Size of the buf_ptr
 * @return                      - Length of TLV Encoded data
 */
int mp_pidlist_to_tlv(const mp_tlv_pidlist_t *pidlist_ptr, uint8_t *buf_ptr, 
                        int buf_len);

/**
 *
 * Set continutity counter value for pid
 * 
 * @param pidlist_ptr   - Pointer to pidlist structure containing pid and
 *                          cc data
 * @param pid           - Indicates which PID to update
 * @param cc            - New value of the continuity counter
 * 
 * @return true on success
 * @see mp_cc_for_pid()
 */
boolean mp_set_cc_for_pid(mp_tlv_pidlist_t* pidlist_ptr, 
                               uint16_t pid, uint8_t cc);
#endif
