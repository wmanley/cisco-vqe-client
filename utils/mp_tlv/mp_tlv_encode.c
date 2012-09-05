/*------------------------------------------------------------------
 * Mpeg Parser.  
 * Functions for Encode TLV Data
 * May 2006, Jim Sullivan
 *
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <utils/mp_tlv_encode.h>
#include <utils/mp_mpeg.h>
#include <string.h>
#include <netinet/in.h>
#include <utils/vam_debug.h>

int mp_sect_to_tlv (const mp_tlv_sectdsc_t *sectdsc_ptr, uint8_t *buf_ptr,
                    int buf_len)
{
    mp_tlvsect_pac_t *pac_ptr = (mp_tlvsect_pac_t*)buf_ptr; 
    int len = sizeof(mp_tlvsect_pac_t) + sectdsc_ptr->sect_len; 
    int tlv_len = len - sizeof(pac_ptr->tlvhdr); 
    do {
        if ( len > buf_len) {
            len=0;
            break;
        } 
        memset(pac_ptr, 0, sizeof(mp_tlvsect_pac_t));
        pac_ptr->tlvhdr.type = htons(sectdsc_ptr->type);
        pac_ptr->tlvhdr.len = htons(tlv_len);
        pac_ptr->pid = htons(sectdsc_ptr->pid);
        pac_ptr->sect_len = htons(sectdsc_ptr->sect_len);
        memcpy(&pac_ptr->sect_data[0], sectdsc_ptr->sect_ptr, 
               sectdsc_ptr->sect_len);
    } while (0);
    return (len);
}


int mp_pcr_to_tlv (const mp_tlv_pcrdsc_t *pcrdsc_ptr, uint8_t *buf_ptr,
                    int buf_len)
{
    mp_tlvpcr_pac_t *pac_ptr = (mp_tlvpcr_pac_t *)buf_ptr; 
    int len = sizeof(*pac_ptr); 
    int tlv_len = len - sizeof(pac_ptr->tlvhdr);
    do {
        if ( len > buf_len) {
            len=0;
            break;
        } 
        pac_ptr->tlvhdr.type = htons(pcrdsc_ptr->type);
        pac_ptr->tlvhdr.len = htons(tlv_len);
        pac_ptr->pid = htons(pcrdsc_ptr->pid);
        /* See mp_tlv.h for syntax */
        pac_ptr->pcr_base_tsrate = 
            htonll((pcrdsc_ptr->pcr_base<<31) | 
            (pcrdsc_ptr->tsrate & 0x7FFFFFFF));
        pac_ptr->pcr_ext = htons((pcrdsc_ptr->pcr_ext & 0x01FF));
    } while (0);
    return (len);
}

int mp_pidlist_to_tlv (const mp_tlv_pidlist_t *pidlist_ptr, uint8_t *buf_ptr,
    int buf_len)
{
    mp_tlv_pidlist_pac_t *pac_ptr = (mp_tlv_pidlist_pac_t *)buf_ptr; 
    int tlv_len = pidlist_ptr->cnt*sizeof(mp_tlv_pidelement_pac_t);
    int i, len = sizeof(pac_ptr->tlvhdr) + tlv_len;   
    do {
        if ( len > buf_len) {
            len=0;
            break;
        } 
        pac_ptr->tlvhdr.type = htons(MP_TSRAP_PIDLIST_TYPE);
        pac_ptr->tlvhdr.len  = htons(tlv_len);
        for ( i=0; i<pidlist_ptr->cnt; i++) {
            pac_ptr->dat[i].pid = htons(pidlist_ptr->dat[i].pid&0x1fff);
            pac_ptr->dat[i].cc  = pidlist_ptr->dat[i].cc&0xf;
            pac_ptr->dat[i].reserved = 0;
        }
    } while (0);
    return (len);
}


