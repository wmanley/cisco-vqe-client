/*------------------------------------------------------------------
 * Mpeg Parser.  
 * Functions for Decode TLV Data
 * May 2006, Jim Sullivan
 *
 * Copyright (c) 2006-2009 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */


#include <utils/mp_tlv_decode.h>
#include <utils/mp_mpeg.h>
#include <log/vqe_utils_syslog_def.h>

#define MP_TLV_MESSAGE_BUF_LEN 100

/*
 * mp component has a dependency on mp_tlv_to_tspkts() function, and for vam/mp
 * to build successfully this function must be available.
 */
 
/* 
 * VQEC should not require any MEG TLV decoding
 * unless we enable FCC support.
 */

/**
 * Head of APP packet buflist 
 */
vqe_tsrap_app_tailq_head_t vqe_tsrap_app_list_head;

/**
 * Temporary buffer to hold PAT data for replication.
 */
static uint8_t s_pat_buf[MP_PSISECTION_LEN];

/** 
 * Executes the proper callback function for a given TLV type. 
 * If a PAT_TLV is being processed, call the PAT callback
 * 
 * @param decodecb_ptr  - Pointers to the callback functions
 * @param tlv_type      - TLV type being processed
 * @param sectdata_ptr  - The TLV section data
 * @param sect_len      - Length of the section data 
 */
void mp_run_tlv_callback(mp_tlv_decodecb_t *decodecb_ptr, 
                         mp_tsraptlv_types_t tlv_type, uint8_t *sectdata_ptr,
                         uint32_t sect_len, uint16_t sect_pid,
                         void * callback_data);
/**
 * Create an MPEG TS packet header.
 * 
 * @param pid           - PID of the header
 * @param cc            - Continuity count value
 * @param pusi          - True to set payload start indicator active
 * @param af_ctrl       - Flags to indicate presence of adaptation field 
 *                              and/or payload
 * @param scram         - Flags for conditional access indicators
 */
static inline
uint32_t mp_mpeg_create_hdr (uint16_t pid, uint8_t cc, boolean pusi, 
                             uint8_t af_ctrl, uint8_t scram)
{
    uint32_t mpeg_hdr = 0x47000000 | pusi<<22 | pid<<8 | scram<<6 | 
                            af_ctrl<<4 | cc;
    return(htonl(mpeg_hdr));
}

/**
 * Create an MPEG-2 Adaptation Field with the given length 
 *
 * @param af_length  - Length of the AF 
 * @param tspkt_buf  - Destination of TS packets
 * @param buf_len    - Length available in tspkt_buf
 *
 * @return Number of bytes written to tspkt_buf 
 */
static inline
uint8_t mp_mpeg_create_af (uint8_t af_length,
                           uint8_t *tspkt_buf,
                           uint32_t buf_len) 
{
    uint8_t af_remain = af_length;

    if ((af_length > buf_len) || (af_length > MP_AF_MAXLEN)) {
        return (0);
    }
        
    /* Insert af_length into packet */
    *tspkt_buf = af_length - 1;
    tspkt_buf++;
    buf_len--;
    af_remain--;

    /* Mark all flags as 0 and pad the rest of the AF w/  0xFF's */
    *tspkt_buf = 0x00;
    tspkt_buf++;
    buf_len--;
    af_remain--;

    while ((af_remain > 0) && (buf_len > 0)) {
        *tspkt_buf = 0xFF;
        tspkt_buf++;
        buf_len--;
        af_remain--;
    }

    return (af_length);
}                           

/**
 * Create an MPEG2-TS PES packet header. See H.222 Section 2.4.3.6
 * 
 * @param pes_packet_length - Length of PES packet. Includes length of PES 
 *                            packet starting immediately after length field
 * @param tspkt_buf         - Destination of TS packets
 * @param buf_len           - Length available in tspkt_buf
 *
 * @return Number of bytes written to tspkt_buf
 */
#define MP_PES_HEADER_LENGTH 9
#define MP_PES_HEADER_FLAG_LENGTH 3
static inline
uint8_t mp_mpeg_create_pes_hdr (uint16_t pes_packet_length,
                                uint8_t *tspkt_buf,
                                uint32_t buf_len) 
{

    if (buf_len < MP_PES_HEADER_LENGTH) {
        return (0);
    }

    /* Create PES Header w/ stream type = H.262/H.264 */
    *((uint32_t*)tspkt_buf) = htonl(MP_MPEG_STARTCODE_PES_RESERVED);
    tspkt_buf += 4;
    buf_len -= 4;

    /* Create PES packet length as length + 3 */
    *((uint16_t*)tspkt_buf) = 
        htons(pes_packet_length + MP_PES_HEADER_FLAG_LENGTH);
    tspkt_buf += 2;
    buf_len -= 2;

    /* Set all flags to 0, spec says field begins with '10' bit sequence */
    *((uint16_t*)tspkt_buf) = htons(0x8000);
    tspkt_buf += 2;
    buf_len -=2;

    /* Set PES_header_data_length to 0 */
    *tspkt_buf = 0x00;
    tspkt_buf++;
    buf_len--;

    return (MP_PES_HEADER_LENGTH);
}

/**
 * Converts Program Specific Information TLV to Transport Stream packets
 *
 * If pes_priv is FALSE, the supplied TLV is assumed to contain a TS packet's 
 * PSI section.  It is converted into one (or more) TS packets, starting with
 * TS header, pointer field, and contents of TLV (spread across one
 * or more TS packets).  The pointer field is only present in the first TS
 * packet.
 *
 * If pes_priv is TRUE, the supplied TLV is assumed to contain an entire PES
 * packet (header and all) with private data.  It is converted into one (and
 * only one) TS packet with PES packet as its payload.
 *
 * If pes_need_hdr is TRUE, the supplied TLV contains Video ES data, but no 
 * PES header. The PES header must be created before placing the TLV data into
 * the TS packet payload. 
 *
 * Note: the "sect" part of this function's name and internal variables are
 * misleading.  Initially, it was used only for TLVs containing PSI sections.
 * Now it is used for both TLVs containing PSI sections and TLVs containing
 * entire PES packets.  The code is almost identical, but the names (here
 * and in the encoding of these TLVs) is misleading.
 *
 * @param tlvdata_ptr       - TLV Data
 * @param tlvdata_len       - Length of TLV data
 * @param tspkt_buf         - Destination of TS packets
 * @param tspkt_bufcnt      - Number of packets that can fit in tspkt_buf
 * @param pes_priv          - Whether this TLV has PES pkt w/ private data
 * @param pes_need_hdr      - Whether this TLV has PES pkt w/o PES Header
 *
 * return Number of packets written
 */
static int mp_secttlv_to_tspkts (uint8_t *tlvdata_ptr, 
                                 uint32_t tlvdata_len, 
                                 uint8_t tspkt_buf[][MP_MPEG_TSPKT_LEN], 
                                 uint32_t tspkt_bufcnt,
                                 boolean pes_priv,
                                 boolean pes_need_hdr)
{
    uint32_t data_idx;/* Current offset into TS packet payload */
    uint32_t tmp_len; /* Length of data to copy into TS packet */
    uint16_t sect_len, pid; 
    boolean pusi;
    uint8_t cc, scram, af_ctrl;
    uint32_t tspkt_cnt, tspkt_idx;   /* Number of TS Packets to output */
    mp_tlvsect_pac_t *pac_ptr;
    uint8_t *sectdata_ptr; /* Pointer to TLV section data */
    
    
    do { 

        /***************************************************************
         * - Get the length of the section data from the tlv input 
         * - Copy the section data into the tspkt buffers, pad the last 
         *   buffer if necessary and record how many TS pkts are needed 
         *   to hold the data. 
         ***************************************************************/
        /* get data from TLV input */
        pac_ptr = (mp_tlvsect_pac_t*)tlvdata_ptr;
        pid      = MP_GET_SECTTLV_PID(pac_ptr);
        sect_len = MP_GET_SECTTLV_LEN(pac_ptr);
        sectdata_ptr = (uint8_t*)(pac_ptr+1);
        
        /* Determine the number of MPEG TS pkts needed to hold the data,
         * and verify there are enough buffers. There will be 184 bytes 
         * available in every buffer except the first where we lose the
         * byte that holds the psi start of section offset */
        tspkt_cnt = (sect_len+1) / 184;
        tspkt_cnt = (sect_len+1) % 184 ? tspkt_cnt+1 : tspkt_cnt; 
        
        if (pes_need_hdr == TRUE) {
            /* Compute number of TS packets taking into acct PES header
             * length */
            tspkt_cnt = (sect_len + MP_PES_HEADER_LENGTH) / 184;
            tspkt_cnt = (sect_len + MP_PES_HEADER_LENGTH) % 184 ?
                tspkt_cnt + 1 : tspkt_cnt;
        }

        if (tspkt_cnt > tspkt_bufcnt) {
            tspkt_cnt = 0;
            break; 
        }

        if (pes_priv == TRUE) {
            if (sect_len > 184) {
                /* we only handle PES packet in a single TS payload */
                tspkt_cnt = 0;
                break; 
            }
            tspkt_cnt = 1;
        }
        
        cc = 0;   /* note - cc is zero for all pkts generated below */
        scram=0;
        
        /***************************************************************
         * Copy the section data into 188B mpeg TS pkts. The first pkt 
         * has the start of a PSI section so the pusi and offset are set.
         ***************************************************************/
        for (tspkt_idx=0, pusi=TRUE;  sect_len > 0; tspkt_idx++) {
            data_idx = 0;
            if (tspkt_idx > 0) {
                pusi = FALSE;
            }

            /* There are a few cases in the following code. 
             * Case 1) This type of TLV being decoded requires a PES header.
             *         Also, we are in the last packet of the TLV, so the AF
             *         field must be used to pad the packet to the full 188B.
             *
             * Case 2) This type of TLV being decoded does not require a PES 
             *         header (i.e. PSI). Also, we are in the last packet of
             *         the TLV so pad the end of the packet with 0xFF
             * 
             * Case 3) This type of TLV being decoded requires a PES header.
             *         Also, we are in the first packet of the TLV so must
             *         create the PES header manually and set PUSI bit. 
             *
             * Case 4) This type of TLV being coded does not require a PES
             *         header. We are in the first packet of the TLV so
             *         must add the 1-byte pointer_field and set PUSI bit.
             *
             * It is possible for Case 1 & Case 3 to exist for the same TLV
             * if it is less than 184 bytes. Similarly case 2 & case 4 may
             * coexist if the TLV only requires 1 TS packet
             *
             * The above cases follow 3 main rules. Only the first packet
             * should have the PUSI bit set. The final packet should include 
             * padding if necessary. Each packet should either include the
             * pointer_field or a PES header. In some cases (ECM_WV), the
             * PES header is included in the TLV, while in others (SEQ, SPS,
             * PPS), the PES must be created manually.
             *
             */
            if (pes_need_hdr && (sect_len < MP_AF_MAXLEN + 1)) {

                af_ctrl = MP_TSHDR_AF_PAYLOAD_PRESENT | MP_TSHDR_AF_PRESENT;
                *((uint32_t*)&tspkt_buf[tspkt_idx][data_idx])= 
                    mp_mpeg_create_hdr(pid, cc, pusi, af_ctrl, scram);
                data_idx += 4;

                /* Create AF to Pad packet to full 188 bytes if this is 
                 * the last packet created from this TLV */
                tmp_len =  mp_mpeg_create_af((MP_AF_MAXLEN + 1) - sect_len,
                                             &tspkt_buf[tspkt_idx][data_idx],
                                             MP_MPEG_TSPKT_LEN - data_idx);
                data_idx += tmp_len;

            } else {
                af_ctrl=MP_TSHDR_AF_PAYLOAD_PRESENT; 
                *((uint32_t*)&tspkt_buf[tspkt_idx][data_idx])= 
                    mp_mpeg_create_hdr(pid, cc, pusi, af_ctrl, scram);
                data_idx += 4;
            }
            
            if (pusi && pes_need_hdr) {
                /* Create PES Header here */
                tmp_len = mp_mpeg_create_pes_hdr(sect_len,
                                               &tspkt_buf[tspkt_idx][data_idx],
                                                 MP_MPEG_TSPKT_LEN - data_idx);
                data_idx += tmp_len;

            } else if ((pusi) && (!pes_priv)) {
                tspkt_buf[tspkt_idx][data_idx] = 0;   /* set pointer_field */
                data_idx++;
            }

            /* 
             * Take the minimum of the remaining TS packet payload or
             * the remaining TLV section length
             */
            tmp_len = MP_MPEG_TSPKT_LEN-data_idx; 
            tmp_len = (sect_len > tmp_len) ? tmp_len : sect_len; 
            memcpy(&tspkt_buf[tspkt_idx][data_idx], 
                   sectdata_ptr, 
                   tmp_len);

            sectdata_ptr += tmp_len;
            data_idx += tmp_len; 
            sect_len -= tmp_len;

            /* Fill rest of TS pak with padding if the section is complete */
            if ((sect_len == 0) && (data_idx < MP_MPEG_TSPKT_LEN)) {
                memset(&tspkt_buf[tspkt_idx][data_idx] , 0xff,
                    MP_MPEG_TSPKT_LEN-data_idx);
            }
        }
    } while (0);
    return (tspkt_cnt);
}


/*
 *
 * Creates the Adaptation Field of a Transport Stream Packet
 *
 *@param buf_ptr            - Destination of TS packet adata
 *@param pcr_base           - Value of PCR Base
 *@param pcr_ext            - Value of PCR Extension
 *@param dind_flag          - Set Discontinuity Indicator
 *@param rap_flag           - Set Random Access Point Indicator
 *
 *@return status
 */
#define MP_AF_OPTIONS_DIND    0x80
#define MP_AF_OPTIONS_RAIND   0x40
#define MP_AF_OPTIONS_PCRFLAG 0x10
mp_tlv_status_t mp_mpeg_create_pcraf (uint8_t* buf_ptr, 
                         unsigned long long pcr_base, 
                         unsigned short pcr_ext, boolean dind_flag, 
                         boolean rap_flag)
{
    int status = MP_STATUS_TLV_SUCCESS;
    int i;
    unsigned long long pcr_val; 
    uint8_t af_options; 

    buf_ptr[0] = 183;
    af_options = MP_AF_OPTIONS_PCRFLAG;
    if (dind_flag) {
        af_options |= MP_AF_OPTIONS_DIND;
    }
    if (rap_flag) {
        af_options |= MP_AF_OPTIONS_RAIND;
    }
    buf_ptr[1] = af_options;
    pcr_val = (pcr_base<<31) | (0x3f<<25) | (pcr_ext<<16);
    *((unsigned long long*)&buf_ptr[2]) = htonll(pcr_val);
    for (i=8; i<184; i++) {
        buf_ptr[i] = 0xff;
    }
    return(status);
}

/**
 * This function decodes a PCR TLV and generates one or more TS packets,
 * each containing an adjusted PCR time stamp. The PCR TLV contains a PCR 
 * timestamp which corresponds to the first byte of the first cell of the
 * repair burst. The PCRs contained in each of the TS pkts generated here 
 * are adjusted based on the TS rate and byte position from the first byte
 * of the repair burst.    
 * 
 * @param tlvdata_ptr   - a pointer to a structure which contains the
                                tlv encoded section data 
 * @param tlv_len       - the length of the buffer pointed to by tlvdata_ptr
 * @param knob_ptr      - a pointer to a structure defining the knobs
 *                              to apply. This allows the caller to control
 *                               the number of PCRs and other properties of 
 *                               encode that may be specific to a MPEG System
 *                               decoder. 
 * @param offset        - this offset indicates the position of the TS pkt 
 *                               containing the first PCR to the TS pkt that
 *                               contains the start of the repair burst. 
 *                               A value of 0 or 1 indicates the first PCR
 *                               generated will immediately precede the
 *                               repair burst.  
 * @param tspkt_buf     - a pointer to a buffer that will receive the MPEG data
 * @param tspkt_bufcnt  - the number of 188 bytes pkts that can fit
                                 into tspkt_buf
 * @return One or more MPEG TS packets containing PCRs as described above. 
 */
#define MP_MIN_PCR_PKTS 1 
int mp_pcrtlv_to_tspkts (uint8_t *tlvdata_ptr, uint32_t tlvdata_len,  
                         mp_tlv_pcrknobs_t *knobs_ptr, uint32_t offset, 
                         uint8_t tspkt_buf[][MP_MPEG_TSPKT_LEN],
                         uint32_t tspkt_bufcnt)
{
    int status = MP_STATUS_TLV_SUCCESS; 
    uint32_t data_idx,tspkt_cnt=knobs_ptr->num_pcrs;
    uint32_t pcr_u_perpkt, rate=0;
    int i;
    uint16_t pcr_ext=0, pid;
    uint8_t cc_last=0, cc, scram, af_ctrl;
    boolean pusi, dind_flag, rap_flag; 
    unsigned long long pcr_base=0; 
    mp_tlvpcr_pac_t *pac_ptr; 
    
    do {
        /* get data from TLV input */
        pac_ptr = (mp_tlvpcr_pac_t*)tlvdata_ptr;
        pid = MP_GET_PID(&pac_ptr->pid);
        pcr_base = MP_GET_PCR_BASE(pac_ptr); 
        pcr_ext = MP_GET_PCR_EXT(pac_ptr);
        rate = MP_GET_PCR_TSRATE(pac_ptr);
        cc_last = knobs_ptr->cc;
        dind_flag = knobs_ptr->dind_flag; 
        rap_flag = knobs_ptr->rap_flag; 

        /*********************************************************
         * Figure out how many tspkts to send, then adjust the 
         * pcr_base to correspond to the first.  
         *********************************************************/
        tspkt_cnt = (tspkt_cnt > MP_MIN_PCR_PKTS) ? tspkt_cnt : MP_MIN_PCR_PKTS;
        if (tspkt_cnt > tspkt_bufcnt) {
            tspkt_cnt = 0; 
            break; 
        }
        offset = (offset == 0) ? 1 : offset;
        pcr_u_perpkt =
            mp_get_pcrunit_per_tspkt(rate, 1); /* pcr units per TS pkt */
        pcr_base -= ((tspkt_cnt-offset)*pcr_u_perpkt); 

        pusi = FALSE; 
        cc = (uint8_t)((int)cc_last - (int)(tspkt_cnt-1)) & 0x0f;
        af_ctrl=MP_TSHDR_AF_PRESENT; 
        scram=0;

        for (i=0; i<tspkt_cnt; i++) {
            data_idx = 0;
            *((uint32_t*)&tspkt_buf[i][data_idx])= 
                mp_mpeg_create_hdr(pid, cc, pusi, af_ctrl, scram);
            data_idx += 4;
            status = mp_mpeg_create_pcraf(&tspkt_buf[i][data_idx],
                    pcr_base, pcr_ext,
                    ((i == 0) && dind_flag),
                    ((i == (tspkt_cnt - 1)) && rap_flag));
            if (status != MP_STATUS_TLV_SUCCESS) {
                tspkt_cnt = 0;
                break; 
            }
            pcr_base += pcr_u_perpkt;
            cc = MP_GET_NEXT_MPEGCC(cc);
        }
    } while (0);
    
    return (tspkt_cnt);
}

/**
 * Decodes a Pidlist TLV into native Pidlist structure
 *
 *@param tlvdata_ptr            - Pointer to TLV Data
 *@param tlvdata_len            - Length of TLV Data
 *@param pidlist_ptr            - Pidlist to unpack data into
 *
 */
int mp_pidlisttlv_to_pidlist (const uint8_t *tlvdata_ptr, 
                              uint32_t tlvdata_len, 
                              mp_tlv_pidlist_t *pidlist_ptr)
{
    int status = MP_STATUS_TLV_SUCCESS;
    mp_tlv_pidlist_pac_t *pac_ptr = (mp_tlv_pidlist_pac_t*)tlvdata_ptr;
    mp_tlvhdr_pac_t *tlvhdr_ptr = &pac_ptr->tlvhdr; 
    mp_tlv_pidelement_pac_t *pide_ptr;
    uint16_t tlv_len = MP_GET_TLV_LEN(tlvhdr_ptr);
    int i, len=0; 
    char msgbuf[MP_TLV_MESSAGE_BUF_LEN];
    do { 
        for (i=0; ((i<MP_MAX_TSRAP_PIDS) && (len < tlv_len)); i++) {
            if (len%sizeof(mp_tlv_pidelement_pac_t) != 0) {
                status = MP_STATUS_TLV_PIDLIST_DECODE_FAILED;
                break; 
            }
            pide_ptr = &pac_ptr->dat[i];
            pidlist_ptr->dat[i].pid = MP_GET_PID(&pide_ptr->pid);
            pidlist_ptr->dat[i].cc = MP_GET_PID_ELEMENT_CC(pide_ptr);
            len+=sizeof(*pide_ptr);
        }
        pidlist_ptr->cnt = i; 
        if (i==MP_MAX_TSRAP_PIDS) {
            status = MP_STATUS_TLV_PIDLIST_DECODE_FAILED;
            break; 
        }
    } while (0);
    
    if (status != MP_STATUS_TLV_SUCCESS) {
        snprintf(msgbuf, MP_TLV_MESSAGE_BUF_LEN, 
                 "Error decoding TLV packets, status=%d", status);
        syslog_print(VQE_MP_TLV_ERR_PKTDEC, msgbuf);
    }

    return (status);

}

#define MP_TLV_HDR_LEN sizeof(mp_tlvhdr_pac_t)
int mp_tlv_to_tspkts (uint8_t *tlvdata_ptr, uint32_t tlvdata_len,
                      mp_tlv_mpegknobs_t *knobs_ptr,
                      uint32_t app_paks_per_rcc,
                      mp_tlv_decodecb_t *decodecb_ptr,
                      mp_tlv_buflist_t* buf_list,
                      void *callback_data,
                      mp_tlv_pidlist_t *pidlist)
{
    mp_tlv_status_t status = MP_STATUS_TLV_SUCCESS; 
    mp_tlvhdr_pac_t *tlvhdr_ptr;
    uint32_t tmp_cnt, len = 0;  
    uint32_t sect_len;
    uint16_t sect_pid;
    uint8_t *sectdata_ptr;
    uint16_t tlv_len; 
    int tlv_cnt=0, tspkt_cnt=0;
    uint32_t tspkt_bufcnt;
    uint32_t offset=0; 
    mp_tlvsect_pac_t *pac_sect_ptr;
    unsigned long long pcr_base = 0, pts_base = 0; 
    uint16_t pcr_ext, pts_ext;
    mp_tlvpcr_pac_t *pac_pcr_ptr, *pac_pts_ptr;
    mp_tsraptlv_types_t tlv_type = MP_TSRAP_TYPE_NONE;
    int pat_size=0;
    int b_written = 0;
    int i;
    int ts_pkts_stored; /* Num TS packets in buf_list so far */
    int ts_pkts_needed; /* Num TS packets we must now add    */
    int ts_pkts_avail;  /* Num TS pkts which can fit         */
    int left_over_ts_pkts;
    int num_nulls_needed;
    char msgbuf[MP_TLV_MESSAGE_BUF_LEN];

    /*
     * Buffer for decoding one TSRAP TLV into (up to
     * MP_MAX_TSPAKS_PER_TLV) TS Packets.
     */
    static uint8_t tspkt_buf[MP_MAX_TSPAKS_PER_TLV][MP_MPEG_TSPKT_LEN];

    tspkt_bufcnt = MP_MAX_TSPAKS_PER_TLV;

    memset(pidlist, 0, sizeof(*pidlist));
    memset(tspkt_buf, 0, (MP_MAX_TSPAKS_PER_TLV * MP_MPEG_TSPKT_LEN));
        
    tlvhdr_ptr = (mp_tlvhdr_pac_t *)tlvdata_ptr;
    for (; len < tlvdata_len;) {
        if ((tlvdata_len - len) < sizeof(*tlvhdr_ptr)) {
            status = MP_STATUS_TLV_INVALID_LEN;
            break; 
        }
        tlv_type = (mp_tsraptlv_types_t)MP_GET_TLV_TYPE(tlvhdr_ptr);
        tlv_len =  MP_GET_TLV_LEN(tlvhdr_ptr);
        switch (tlv_type) { 
        case MP_TSRAP_TYPE:
            /*************************************************************
             * The MP_TSRAP_TYPE is a special case because it marks the
             * beginning of the TLV data set. The len and value correspond 
             * to all the rest of the tlv data that is in the set. 
             * If  present it must be the first. 
             *************************************************************/
            if (tlv_cnt++ != 0) {
                status = MP_STATUS_TLV_SHOULD_BE_FIRST;
                break;
            }
            len += MP_TLV_HDR_LEN;
            tlvhdr_ptr += 1;
            continue;
        case MP_TSRAP_PA_TYPE:
        case MP_TSRAP_PMAP_TYPE:
        case MP_TSRAP_SEQPEXT_TYPE:
        case MP_TSRAP_SPS_TYPE:
        case MP_TSRAP_PPS_TYPE:
        case MP_TSRAP_SEI_TYPE:
        case MP_TSRAP_ECM_TYPE:
        case MP_TSRAP_ECM_PES_TYPE:
            /*************************************************************
             * Process a section TLV containing raw data. This could be
             * PAT, PMT, or Sequence/SEI data
             * Create 188B Mpeg pkt(s) that carry the data
             * Return number of TS pkts created. 
             *************************************************************/
            pac_sect_ptr = (mp_tlvsect_pac_t*)tlvhdr_ptr;
            sect_len = MP_GET_SECTTLV_LEN(pac_sect_ptr);
            sect_pid = MP_GET_SECTTLV_PID(pac_sect_ptr);
            sectdata_ptr = (uint8_t*)(pac_sect_ptr+1);
            tmp_cnt = mp_secttlv_to_tspkts 
                ((uint8_t*)tlvhdr_ptr,
                 tlv_len + MP_TLV_HDR_LEN, 
                 tspkt_buf,
                 tspkt_bufcnt, 
                 (tlv_type == MP_TSRAP_ECM_PES_TYPE),
                 ((tlv_type == MP_TSRAP_SEQPEXT_TYPE) ||
                  (tlv_type == MP_TSRAP_SPS_TYPE) ||
                  (tlv_type == MP_TSRAP_PPS_TYPE)));

            if (tmp_cnt == 0){
                status = MP_STATUS_TLV_DECODE_FAILED;
                break; 
            }
            b_written = mp_tlv_buf_fill(buf_list, (uint8_t *)&tspkt_buf, 
                                        tmp_cnt);  
            if (b_written != tmp_cnt * MP_MPEG_TSPKT_LEN) {
                syslog_print(VQE_MP_TLV_ERR_BUF_TOO_SMALL, 
                             "MP - Couldn't write all packets to buf");
                status = MP_STATUS_BUFF_TOO_SMALL;
                break;
            }

            tspkt_cnt += tmp_cnt; 

            mp_run_tlv_callback(decodecb_ptr, tlv_type, sectdata_ptr,
                                sect_len, sect_pid, callback_data);

            /* Now we deal with the PAT & PMT individually. If we just
             * processed the PAT, allocate a buffer for it and save it away
             * so it can be used in duplicating the PAT & PMT.
             */
            if (knobs_ptr->num_patpmt > 1) { 
                if(tlv_type == MP_TSRAP_PA_TYPE){
                    pat_size = tmp_cnt;
                    memcpy(s_pat_buf, &tspkt_buf, tmp_cnt * MP_MPEG_TSPKT_LEN);
                }
                else if(tlv_type == MP_TSRAP_PMAP_TYPE){
                    int i = 0;
                    for(i = 0; i < (knobs_ptr->num_patpmt -1); i++){
                        b_written = mp_tlv_buf_fill(buf_list, 
                                                    s_pat_buf, 
                                                    pat_size);
                        if (b_written != pat_size * MP_MPEG_TSPKT_LEN) {
                            syslog_print(VQE_MP_TLV_ERR_BUF_TOO_SMALL, 
                                         "MP - Couldn't write all "
                                         "packets to buf");
                            status = MP_STATUS_BUFF_TOO_SMALL;
                            break;
                        }
                        tspkt_cnt += pat_size;

                        b_written = mp_tlv_buf_fill(buf_list, 
                                                    (uint8_t*)&tspkt_buf,
                                                    tmp_cnt);
                        if (b_written != tmp_cnt * MP_MPEG_TSPKT_LEN) {
                            syslog_print(VQE_MP_TLV_ERR_BUF_TOO_SMALL, 
                                         "MP - Couldn't write all "
                                         "packets to buf");
                            status = MP_STATUS_BUFF_TOO_SMALL;
                            break;
                        }
                        tspkt_cnt += tmp_cnt;
                    }
                }
            }
            break;
        case MP_TSRAP_PCR_TYPE:
            pac_pcr_ptr = (mp_tlvpcr_pac_t*)tlvhdr_ptr;
            pcr_base = MP_GET_PCR_BASE(pac_pcr_ptr);
            pcr_ext = MP_GET_PCR_EXT(pac_pcr_ptr);
            if (decodecb_ptr && decodecb_ptr->pcr_cb) {
                decodecb_ptr->pcr_cb(pcr_base, pcr_ext, callback_data);
            }
 
            offset = 1; 
            tmp_cnt = mp_pcrtlv_to_tspkts ((uint8_t*)tlvhdr_ptr,
                                           tlv_len+MP_TLV_HDR_LEN,
                                           &knobs_ptr->pcr_knobs, offset, 
                                           tspkt_buf, tspkt_bufcnt);
            if ( tmp_cnt == 0 ){
                status = MP_STATUS_TLV_PCR_DECODE_FAILED ;
                break; 
            }
            
            b_written = mp_tlv_buf_fill(buf_list, 
                                        (uint8_t *)&tspkt_buf, 
                                        tmp_cnt);
            if (b_written != tmp_cnt * MP_MPEG_TSPKT_LEN) {
                syslog_print(VQE_MP_TLV_ERR_BUF_TOO_SMALL, 
                             "MP - Couldn't write all packets to buf");
                status = MP_STATUS_BUFF_TOO_SMALL;
                break;
            }
            tspkt_cnt += tmp_cnt; 

            break;

        case MP_TSRAP_PTS_TYPE:
            pac_pts_ptr = (mp_tlvpcr_pac_t*)tlvhdr_ptr;
            pts_base = MP_GET_PCR_BASE(pac_pts_ptr);
            pts_ext = MP_GET_PCR_EXT(pac_pts_ptr);
            if (decodecb_ptr && decodecb_ptr->pts_cb) {
                decodecb_ptr->pts_cb(pts_base, callback_data);
            }
            break;

        case MP_TSRAP_PIDLIST_TYPE:
            status = mp_pidlisttlv_to_pidlist ((uint8_t*)tlvhdr_ptr,
                                               tlv_len+MP_TLV_HDR_LEN, 
                                               pidlist);
            break;
        case MP_TSRAP_CA_TYPE:
        default:
            /* ignore */
            break;
        }

        if (status != MP_STATUS_TLV_SUCCESS) {
            break; 
        }
        tlv_cnt++;
        len += (tlv_len + MP_TLV_HDR_LEN);
        tlvhdr_ptr = (mp_tlvhdr_pac_t*)(tlvdata_ptr + len);
    }
    if (status == MP_STATUS_TLV_SUCCESS) {

        /* If the buffer contains a number of TS packets which are not a 
         * multiple of MP_NUM_TSPAKS_PER_DP_PAK, add MPEG nulls to make
         * it such a multiple.
         */
        left_over_ts_pkts = tspkt_cnt % MP_NUM_TSPAKS_PER_DP_PAK;

        if (left_over_ts_pkts != 0) {

            num_nulls_needed = MP_NUM_TSPAKS_PER_DP_PAK - left_over_ts_pkts;
            if (mp_tlv_buf_pad(buf_list, num_nulls_needed) 
                != MP_STATUS_TLV_SUCCESS) {
              
                syslog_print(VQE_MP_TLV_ERR_OUTBUF_PAD,
                             "MP - Padding output packet buffer failed");
                goto failed;
            }
            tspkt_cnt += num_nulls_needed;
        }

        if (app_paks_per_rcc > 1) {

            /* 
             * Note: we assume at most one buffer in buf_list - current 
             * limitation, and should replace buf_list with a single global
             * buffer in future
             */
            ts_pkts_stored = (buf_list->buf_len - buf_list->buf_rem) 
                / MP_MPEG_TSPKT_LEN;
            ts_pkts_needed = ts_pkts_stored * (app_paks_per_rcc - 1);
            ts_pkts_avail  = buf_list->buf_rem / MP_MPEG_TSPKT_LEN;

            if (ts_pkts_needed > ts_pkts_avail) {
                syslog_print(VQE_MP_TLV_ERR_BUF_TOO_SMALL, 
                             "MP - Couldn't write all packets to buf");
                status = MP_STATUS_BUFF_TOO_SMALL;
                goto failed;
            }

            for (i=0; i < (app_paks_per_rcc - 1); i++) {
                memcpy(&buf_list->tspkt_buf
                       [buf_list->buf_len - buf_list->buf_rem],
                       &buf_list->tspkt_buf[0],
                       ts_pkts_stored * MP_MPEG_TSPKT_LEN);
                buf_list->buf_rem -= (ts_pkts_stored * MP_MPEG_TSPKT_LEN);
                tspkt_cnt += ts_pkts_stored;
            }
        }

        /* Now loop through packets that are being prepended to the front
         * of the burst in REVERSE order to make the cc work out.
         */

        mp_correct_tspkt_cc(pidlist, buf_list, tspkt_cnt);
        snprintf(msgbuf, MP_TLV_MESSAGE_BUF_LEN, 
                 "MP - Decoded TSRAP TLV data, tlv proc=%d, "
                 "TS pkts generated=%d", tlv_cnt, tspkt_cnt);
        syslog_print(VQE_MP_TLV_INFO, msgbuf);
    } else {
failed:
        snprintf(msgbuf, MP_TLV_MESSAGE_BUF_LEN, 
                 "MP - Error decoding TLV pkts type=%d, "
                 "status=%d", tlv_type, status);
        syslog_print(VQE_MP_TLV_ERR_PKT_DEC_TYPE, msgbuf);
        tspkt_cnt = 0; /* tell caller we failed */
    }

    return (tspkt_cnt * MP_MPEG_TSPKT_LEN);
}

void mp_run_tlv_callback(mp_tlv_decodecb_t *decodecb_ptr, 
                         mp_tsraptlv_types_t tlv_type, uint8_t *sectdata_ptr,
                         uint32_t sect_len, uint16_t sect_pid,
                         void *callback_data){
    if(!decodecb_ptr){
        return;
    }
    
    if (tlv_type == MP_TSRAP_PA_TYPE && decodecb_ptr->pat_cb) {
        decodecb_ptr->pat_cb(sectdata_ptr, sect_len, callback_data);
    }
    else if (tlv_type == MP_TSRAP_PMAP_TYPE && decodecb_ptr->pmt_cb) {
        decodecb_ptr->pmt_cb(sectdata_ptr, sect_len, sect_pid, callback_data);
    }
    else if(tlv_type == MP_TSRAP_SEQPEXT_TYPE && decodecb_ptr->seq_cb){
        decodecb_ptr->seq_cb(sectdata_ptr, sect_len, callback_data);
    }
    else if(tlv_type == MP_TSRAP_SPS_TYPE && decodecb_ptr->sps_cb){
        decodecb_ptr->sps_cb(sectdata_ptr, sect_len, callback_data);
    }
    else if(tlv_type == MP_TSRAP_PPS_TYPE && decodecb_ptr->pps_cb){
        decodecb_ptr->pps_cb(sectdata_ptr, sect_len, callback_data);
    }
    else if(tlv_type == MP_TSRAP_SEI_TYPE && decodecb_ptr->sei_cb){
        decodecb_ptr->sei_cb(sectdata_ptr, sect_len, callback_data);
    }
    return;               
}

/**
 * Sets Continuity Count to correct value. The packets generated by 
 * mp_tlv_to_tspkts will be prepended to the repair burst so the 
 * CC must be decremented by 1 for each TS Pkt on a given PID.
 *
 * If correct CC cannot be found (not in PIDLIST), a CC value of zero
 * is used.  While this may be incorrect, it is preferable to failing
 * the RCC, as CCs are often not checked by STBs.
 *
 * @param pidlist               - Pointer to pidlist table
 * @param buf_list              - Contains Transport Stream Packets
 * @param tspkt_cnt             - Number of packets in buf_list
 */
void mp_correct_tspkt_cc(mp_tlv_pidlist_t *pidlist, mp_tlv_buflist_t *buf_list,
                        int tspkt_cnt){

    /* Find last used buffer in list */
    while(buf_list!=VQE_TAILQ_LAST(&vqe_tsrap_app_list_head, 
                                   vqe_tsrap_app_tailq_head_)){
       buf_list = VQE_TAILQ_NEXT(buf_list, list_entry);
    }
    while(tspkt_cnt >0 && buf_list){
        int bufpkt_cnt;

        bufpkt_cnt = (buf_list->buf_len - buf_list->buf_rem)/MP_MPEG_TSPKT_LEN;
        while(bufpkt_cnt>0){
            uint16_t pid;
            uint8_t cc, af_ctrl;
            boolean decrement_flag = TRUE;

            mp_tsheader_t *tshdr_ptr = (mp_tsheader_t *)
                ((uint32_t *)&buf_list->tspkt_buf[(bufpkt_cnt-1) *
                                                  MP_MPEG_TSPKT_LEN]);
            pid = MP_GET_TSHDR_PID(tshdr_ptr);
            af_ctrl = MP_GET_TSHDR_AF(tshdr_ptr);

            if (pid == MP_NULL_PACKET_PID) {
                goto next_ts_packet;
            }

            /* MPEG2-TS spec says CC value is not decremented when 
             * only an adaptation field and no payload is present
             */
            if (af_ctrl ==  MP_TSHDR_AF_PRESENT) {
                decrement_flag = FALSE;
            }

            if (mp_get_cc_for_pid(pidlist, pid, &cc, decrement_flag) == 
                FALSE) { 
                syslog_print(VQE_MP_TLV_ERR_PID, 
                             "MP - PID for TSRAP TS Packet not found");
                cc=0;
            }

            tshdr_ptr->ts_b4 = (tshdr_ptr->ts_b4&0xf0)| cc;  

next_ts_packet:
            bufpkt_cnt--;
            tspkt_cnt--;
        }
        buf_list = VQE_TAILQ_PREV(buf_list, 
                                  vqe_tsrap_app_tailq_head_, 
                                  list_entry);
    }
}

/**
 * Pads buffer with NULL Transport Stream packets (if needed) to ensure
 * buffer contains a multiple of MP_NUM_TSPAKS_PER_DP_PAK TS packets.
 * 
 * @param buf_list      - Buffer to be filled
 * @param tspkt_cnt     - Number of NULL TS Packets to add
 */
mp_tlv_status_t mp_tlv_buf_pad (mp_tlv_buflist_t *buf_list,
                                int num_nulls_needed)
{
    /* Create one MPEG-TS Padding packet, it will be
       copied multiple times into the output buffers */
    char padding[MP_MPEG_TSPKT_LEN];
    int hdr_data = 0;

    hdr_data = mp_mpeg_create_hdr(MP_NULL_PACKET_PID , 0, 0,
                                  MP_TSHDR_AF_PAYLOAD_PRESENT, 0);
    memcpy(&padding, &hdr_data, sizeof(hdr_data));
    memset(&padding[4], 0xff, MP_MPEG_TSPKT_LEN-sizeof(hdr_data));

    /* 
     * Fill the output buffer with as many NULL MPEG TS packets
     * as are needed
     */
    while (buf_list->buf_rem > 0 && 
          buf_list->buf_rem >= MP_MPEG_TSPKT_LEN &&
          num_nulls_needed != 0) {
        if (mp_tlv_buf_fill(buf_list, (uint8_t *)&padding, 1) != 
            MP_MPEG_TSPKT_LEN) {
            syslog_print(VQE_MP_TLV_ERR_BUF_TOO_SMALL, 
                         "MP - Couldn't write all packets to buf");
            return (MP_STATUS_BUFF_TOO_SMALL);
        }
        num_nulls_needed --;
    }

    if (num_nulls_needed == 0) {
        return (MP_STATUS_TLV_SUCCESS);
    } else {
        return (MP_STATUS_BUFF_TOO_SMALL);
    }
}

/**
 * Fills a list of buffers with TS packet data
 * 
 * @param buf_list      - Buffer list to receive data
 * @param pkt_data      - TS packets to be put into buf_list
 * @param pkt_cnt       - Number of packets in pkt_data
 * 
 * @return Number of bytes written into buf_list
 */
int mp_tlv_buf_fill (mp_tlv_buflist_t *buf_list, uint8_t *pkt_data, int pkt_cnt)
{
    int b_written=0;
    
    MP_TLV_ASSERT(buf_list, "mp_tlv_buf_fill: buf_list is NULL");
    while(pkt_cnt > 0){
        int buf_cnt = buf_list->buf_rem / MP_MPEG_TSPKT_LEN;
        int offset = buf_list->buf_len - buf_list->buf_rem;
        
        if(pkt_cnt<=buf_cnt){
            /* The current buffer can hold the entire contents 
               of the data. Place all of pkt_data into tspkt_buf
            */
            memcpy(&buf_list->tspkt_buf[offset], &pkt_data[b_written],
                    pkt_cnt * MP_MPEG_TSPKT_LEN);
            buf_list->buf_rem -= pkt_cnt * MP_MPEG_TSPKT_LEN;
            b_written += pkt_cnt * MP_MPEG_TSPKT_LEN;
            pkt_cnt=0;
        }
        else{
            /* The current buffer is too small for all the data. 
               Write as much as is possible to the current entry
               and then move on to the next one.
            */
            memcpy(&buf_list->tspkt_buf[offset], &pkt_data[b_written], 
                    buf_cnt * MP_MPEG_TSPKT_LEN);
            buf_list->buf_rem -= buf_cnt * MP_MPEG_TSPKT_LEN;
            b_written += buf_cnt * MP_MPEG_TSPKT_LEN;
            pkt_cnt-=buf_cnt;
            
            buf_list = VQE_TAILQ_NEXT(buf_list, list_entry);
            if(!buf_list && pkt_cnt > 0){
                syslog_print(VQE_MP_TLV_ERR_BUF_TOO_SMALL, 
                             "Buffer List is not large enough to store "
                             "all TS Packet Data.");
                pkt_cnt = 0;
            }
        }
    }
    return b_written;
}



/* utility routines for debug */ 
void mp_dump_bytes (uint8_t *buf_ptr, uint32_t len)
{
    int i,j;
    for (i=0; i<len;) {
        for (j=0; j<16 && i<len; j++, i++) {
            MP_TLV_PRINT("%02x ", buf_ptr[i]);
        }
        MP_TLV_PRINT("\r\n");
    }
    return;
}

void mp_dump_tspkts (uint8_t tspkt_buf[][MP_MPEG_TSPKT_LEN],
                     uint32_t tspkt_cnt)
{
    int i;
    for (i=0; i<tspkt_cnt; i++) {
        MP_TLV_PRINT("MP - dump of MPEG TS pkt\r\n");
        mp_dump_bytes(&tspkt_buf[i][0], MP_MPEG_TSPKT_LEN);
    }
    return;
}
