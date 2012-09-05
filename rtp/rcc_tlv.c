/*
 *------------------------------------------------------------------
 * rcc_tlv.c  -- Encode/decode RCC specific TLVs for PPDD/PLII/NCSI APP msgs
 *
 * March 2008, Donghai Ma
 *
 * Copyright (c) 2006-2009 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include <assert.h>
#include "rcc_tlv.h"

/* TLV types and length */
static struct tl {
    rcc_tlv_type_t type;
    int len;
} rcc_tl[] = {
    {PADDING, 0},
    {START_SEQ_NUMBER, 4},
    {START_RTP_TIMESTAMP, 4},
    {DT_EARLIEST_TIME_TO_JOIN, 4},
    {DT_REPAIR_END_TIME, 4},
    {TSRAP, -1},
    {MIN_RCC_FILL, 4},
    {MAX_RCC_FILL, 4},
    {ACT_RCC_FILL, 4},
    {ACT_RCC_FILL_AT_JOIN, 4},
    {ER_HOLDOFF_TIME, 4},
    {FIRST_MCAST_PKT_SEQ_NUM, 4},
    {FIRST_MCAST_PKT_RECV_TIME, 4},
    {DO_FASTFILL, 4},
    {ACTUAL_FASTFILL, 4},
    {MAXIMUM_RECV_BW, 4},
    {MAXIMUM_FASTFILL_TIME, 4},
    {RCC_STATUS, 4}, /* UNUSED */

    /* Must be Last! */
    {MAX_RCC_TLV_TYPE, 0}
};


/**
 * ppdd_tlv_encode_allocate
 * @note
 *  Refer to header file rcc_tlv.h for complete function description.
 */
uint8_t *ppdd_tlv_encode_allocate (const ppdd_tlv_t tlv, 
                                   uint8_t *tsrap, uint16_t tsrap_len,
                                   uint32_t *data_len)
{
    int i, l, num_tlv;
    rcc_tlv_type_t t;
    uint8_t  *p_data = NULL, *ptr = NULL;
    uint32_t total_len = 0, tlen = 0, u32_nl;
    uint16_t len, padding;

    /* List all PPDD types in an array so that we can iterate */
    const rcc_tlv_type_t ppdd_types[] = {
        START_SEQ_NUMBER,
        START_RTP_TIMESTAMP,
        DT_EARLIEST_TIME_TO_JOIN,
        DT_REPAIR_END_TIME,
        TSRAP,
        ACT_RCC_FILL,
        ACT_RCC_FILL_AT_JOIN,
        ER_HOLDOFF_TIME,
        ACTUAL_FASTFILL,
    };
    
    /* Calculate the total_len first so we can allocate enough memory */
    *data_len = 0;

    num_tlv = sizeof(ppdd_types)/sizeof(ppdd_types[0]);
    for (i = 0; i < num_tlv; i++) {
        t = ppdd_types[i];
        if (t == TSRAP) {
            if (tsrap_len) {
                l = tsrap_len;
            } else {
                /* Skip TS-RAP type */
                continue;
            }
        }  else {
            l = rcc_tl[t].len;
        }
        tlen += (1 + 2 + l); /* Combined length for Type, Length and Value */
    }

    /* May need to pad */
    padding = 4 - (tlen & 3);
    tlen += padding;

    p_data = malloc(tlen);
    if (!p_data) {
        RTP_LOG_ERROR("Failed to malloc PPDD TLV data\n");
        return NULL;
    }
    memset(p_data, 0, tlen);

    *data_len = tlen;
    ptr = p_data;

    for (i = 0; i < num_tlv; i++) {
        t = ppdd_types[i];
        if (t == TSRAP) {
            if (tsrap_len) {
                l = tsrap_len;
            } else {
                /* There is no TS-RAP to be encoded*/
                continue;
            }
        } else {
            l = rcc_tl[t].len;
        }
        *ptr = t;
        ptr += TYPE_FIELD_LEN;
        len = htons(l);
        memcpy(ptr, &len, LENGTH_FIELD_LEN);
        ptr += LENGTH_FIELD_LEN;
        
        switch (t) {
        case START_SEQ_NUMBER:
            u32_nl = htonl(tlv.start_seq_number);
            memcpy(ptr, &u32_nl, l);
            break;
        case START_RTP_TIMESTAMP:
            /* Already in network order from CM: need to change this? */
            memcpy(ptr, &(tlv.start_rtp_time), l);
            break;
        case DT_EARLIEST_TIME_TO_JOIN:
            u32_nl = htonl(tlv.dt_earliest_join);
            memcpy(ptr, &u32_nl, l);
            break;
        case DT_REPAIR_END_TIME:
            u32_nl = htonl(tlv.dt_repair_end);
            memcpy(ptr, &u32_nl, l);
            break;
        case TSRAP:
            memcpy(ptr, tsrap, l);
            break;
        case ACT_RCC_FILL:
            u32_nl = htonl(tlv.act_rcc_fill);
            memcpy(ptr, &u32_nl, l);
            break;
        case ACT_RCC_FILL_AT_JOIN:
            u32_nl = htonl(tlv.act_rcc_fill_at_join);
            memcpy(ptr, &u32_nl, l);
            break;
        case ER_HOLDOFF_TIME:
            u32_nl = htonl(tlv.er_holdoff_time);
            memcpy(ptr, &u32_nl, l);
            break;
        case ACTUAL_FASTFILL:
            u32_nl = htonl(tlv.actual_fastfill);
            memcpy(ptr, &u32_nl, l);
            break;
        default:
            /* No other types but need this to silent compiler warning */
            break;
        }

        ptr += l;
        total_len += 1 + 2 + l;
    }

    /* The total length shall be a multiple of 4 (32 bits) */
    padding = 4 - (total_len & 3);
    total_len += padding;
    
    /* Double check */
    assert(tlen == total_len);

    /* Add padding */
    while(padding-- > 0) {
        *ptr++ = PADDING;
    }

    return(p_data);
}


/**
 * plii_tlv_encode_allocate
 * @note
 *  Refer to header file rcc_tlv.h for complete function description.
 */
uint8_t *plii_tlv_encode_allocate (const plii_tlv_t tlv,
                                   uint32_t *data_len)
{
    int i, l, num_tlv;
    rcc_tlv_type_t t;
    uint8_t  *p_data = NULL, *ptr = NULL;
    uint32_t total_len = 0, tlen = 0, u32_nl;
    uint16_t len, padding;

    const rcc_tlv_type_t plii_types[] = {
        MIN_RCC_FILL,
        MAX_RCC_FILL,
        DO_FASTFILL,
        MAXIMUM_RECV_BW,
        MAXIMUM_FASTFILL_TIME,
    };

    /* Calculate the total_len first so we can allocate enough memory */
    *data_len = 0;

    num_tlv = sizeof(plii_types)/sizeof(plii_types[0]);
    for (i = 0; i < num_tlv; i++) {
        t = plii_types[i];
        l = rcc_tl[t].len;
        tlen += (1 + 2 + l); /* Combined length for Type, Length and Value */
    }

    /* May need to pad */
    padding = 4 - (tlen & 3);
    tlen += padding;

    p_data = malloc(tlen);
    if (!p_data) {
        RTP_LOG_ERROR("Failed to malloc PLII TLV data\n");
        return NULL;
    }
    memset(p_data, 0, tlen);

    *data_len = tlen;
    ptr = p_data;

    for (i = 0; i < num_tlv; i++) {
        t = plii_types[i];
        *ptr = t;
        ptr += TYPE_FIELD_LEN;
        l = rcc_tl[t].len;
        len = htons(l);
        memcpy(ptr, &len, LENGTH_FIELD_LEN);
        ptr += LENGTH_FIELD_LEN;
        
        switch (t) {
        case MIN_RCC_FILL:
            u32_nl = htonl(tlv.min_rcc_fill);
            memcpy(ptr, &u32_nl, l);
            break;
        case MAX_RCC_FILL:
            u32_nl = htonl(tlv.max_rcc_fill);
            memcpy(ptr, &u32_nl, l);
            break;
        case DO_FASTFILL:
            u32_nl = htonl(tlv.do_fastfill);
            memcpy(ptr, &u32_nl, l);
            break;
        case MAXIMUM_RECV_BW:
            u32_nl = htonl(tlv.maximum_recv_bw);
            memcpy(ptr, &u32_nl, l);
            break;
        case MAXIMUM_FASTFILL_TIME:
            u32_nl = htonl(tlv.maximum_fastfill_time);
            memcpy(ptr, &u32_nl, l);
            break;
        default:
            /* No other types but need this to silent compiler warning */
            break;
        }

        ptr += l;
        total_len += 1 + 2 + l;
    }

    /* The total length shall be a multiple of 4 (32 bits) */
    padding = 4 - (total_len & 3);
    total_len += padding;
    
    /* Double check */
    assert(tlen == total_len);

    /* Add padding */
    while(padding-- > 0) {
        *ptr++ = PADDING;
    }

    return(p_data);
}


/**
 * ncsi_tlv_encode_allocate
 * @note
 *  Refer to header file rcc_tlv.h for complete function description.
 */
uint8_t *ncsi_tlv_encode_allocate (const ncsi_tlv_t tlv,
                                   uint32_t *data_len)
{
    int i, l, num_tlv;
    rcc_tlv_type_t t;
    uint8_t  *p_data = NULL, *ptr = NULL;
    uint32_t total_len = 0, tlen = 0, u32_nl;
    uint16_t len, padding;

    const rcc_tlv_type_t ncsi_types[] = {
        FIRST_MCAST_PKT_SEQ_NUM,
        FIRST_MCAST_PKT_RECV_TIME,
    };

    /* Calculate the total_len first so we can allocate enough memory */
    *data_len = 0;

    num_tlv = sizeof(ncsi_types)/sizeof(ncsi_types[0]);
    for (i = 0; i < num_tlv; i++) {
        t = ncsi_types[i];
        l = rcc_tl[t].len;
        tlen += (1 + 2 + l); /* Combined length for Type, Length and Value */
    }

    /* May need to pad */
    padding = 4 - (tlen & 3);
    tlen += padding;

    p_data = malloc(tlen);
    if (!p_data) {
        RTP_LOG_ERROR("Failed to malloc NCSI TLV data\n");
        return NULL;
    }
    memset(p_data, 0, tlen);

    *data_len = tlen;
    ptr = p_data;

    for (i = 0; i < num_tlv; i++) {
        t = ncsi_types[i];
        *ptr = t;
        ptr += TYPE_FIELD_LEN;
        l = rcc_tl[t].len;
        len = htons(l);
        memcpy(ptr, &len, LENGTH_FIELD_LEN);
        ptr += LENGTH_FIELD_LEN;
        
        switch (t) {
        case FIRST_MCAST_PKT_SEQ_NUM:
            u32_nl = htonl(tlv.first_mcast_seq_number);
            memcpy(ptr, &u32_nl, l);
            break;
        case FIRST_MCAST_PKT_RECV_TIME:
            u32_nl = htonl(tlv.first_mcast_recv_time);
            memcpy(ptr, &u32_nl, l);
            break;
        default:
            /* No other types but need this to silent compiler warning */
            break;
        }

        ptr += l;
        total_len += 1 + 2 + l;
    }

    /* The total length shall be a multiple of 4 (32 bits) */
    padding = 4 - (total_len & 3);
    total_len += padding;
    
    /* Double check */
    assert(tlen == total_len);

    /* Add padding */
    while(padding-- > 0) {
        *ptr++ = PADDING;
    }

    return(p_data);
}



/**
 * erri_tlv_encode_allocate
 * @note
 *  Refer to header file rcc_tlv.h for complete function description.
 */
uint8_t *erri_tlv_encode_allocate (const erri_tlv_t tlv,
                                   uint32_t *data_len)
{
    int i, l, num_tlv;
    rcc_tlv_type_t t;
    uint8_t  *p_data = NULL, *ptr = NULL;
    uint32_t total_len = 0, tlen = 0, u32_nl;
    uint16_t len, padding;

    const rcc_tlv_type_t erri_types[] = {
        MAXIMUM_RECV_BW,
    };

    /* Calculate the total_len first so we can allocate enough memory */
    *data_len = 0;

    num_tlv = sizeof(erri_types)/sizeof(erri_types[0]);
    for (i = 0; i < num_tlv; i++) {
        t = erri_types[i];
        l = rcc_tl[t].len;
        tlen += (1 + 2 + l); /* Combined length for Type, Length and Value */
    }

    /* May need to pad */
    padding = 4 - (tlen & 3);
    tlen += padding;

    p_data = malloc(tlen);
    if (!p_data) {
        RTP_LOG_ERROR("Failed to malloc ERRI TLV data\n");
        return NULL;
    }
    memset(p_data, 0, tlen);

    *data_len = tlen;
    ptr = p_data;

    for (i = 0; i < num_tlv; i++) {
        t = erri_types[i];
        *ptr = t;
        ptr += TYPE_FIELD_LEN;
        l = rcc_tl[t].len;
        len = htons(l);
        memcpy(ptr, &len, LENGTH_FIELD_LEN);
        ptr += LENGTH_FIELD_LEN;
        
        switch (t) {
        case MAXIMUM_RECV_BW:
            u32_nl = htonl(tlv.maximum_recv_bw);
            memcpy(ptr, &u32_nl, l);
            break;
        default:
            /* No other types but need this to silent compiler warning */
            break;
        }

        ptr += l;
        total_len += 1 + 2 + l;
    }

    /* The total length shall be a multiple of 4 (32 bits) */
    padding = 4 - (total_len & 3);
    total_len += padding;
    
    /* Double check */
    assert(tlen == total_len);

    /* Add padding */
    while(padding-- > 0) {
        *ptr++ = PADDING;
    }

    return(p_data);
}

/**
 * rcc_tlv_encode_destroy
 * @note
 *  Refer to header file rcc_tlv.h for complete function description.
 */
void rcc_tlv_encode_destroy (uint8_t *p_data)
{
    if (p_data) {
        free(p_data);
    }
}


/**
 * ppdd_tlv_decode_allocate
 * @note
 *  Refer to header file rcc_tlv.h for complete function description.
 */
ppdd_tlv_t *ppdd_tlv_decode_allocate (uint8_t *data, uint16_t data_len)
{
    boolean valid_type_seen = FALSE;
    uint8_t *ptr = NULL, type;
    uint16_t len;
    uint8_t *p_end;
    ppdd_tlv_t *p_tlv = NULL;
    uint32_t start_seq_number = 0;
    uint32_t start_rtp_time = 0;
    uint32_t dt_earliest_join = 0;
    uint32_t dt_repair_end = 0;
    uint32_t act_rcc_fill = 0;
    uint32_t act_rcc_fill_at_join = 0;
    uint32_t er_holdoff_time = 0;
    uint32_t actual_fastfill = 0;

    if (!data || data_len==0) {
        return (NULL);
    }

    ptr = data;
    p_end = data + data_len;
    while (ptr - data < data_len) {
        type = *ptr;
        ptr++;

        if (type == PADDING) {
            /* This type has no length */
            continue;
        }

        if (ptr + LENGTH_FIELD_LEN > p_end) {
            if (p_tlv) {
                free(p_tlv);
            }
            return (NULL);
        }
        memcpy(&len, ptr, sizeof(len));
        len = ntohs(len);
        ptr += LENGTH_FIELD_LEN;  /* 2-byte length field */
        if (ptr + len > p_end) {
            if (p_tlv) {
                free(p_tlv);
            }
            return (NULL);
        }

        /* Skip over unknown types */
        if (type >= MAX_RCC_TLV_TYPE) {
            ptr += len;
            continue;
        }

        /* Validate length field to protect against 
         * attacks/corrupted packets. TSRAP type is variable
         * length, with no actual maximum
         */
        if ((type != TSRAP) && (len > rcc_tl[type].len)) { 
            if (p_tlv) {
                free(p_tlv);
            }
            return (NULL);
        }

        switch(type) {
        case START_SEQ_NUMBER:
            valid_type_seen = TRUE;
            memcpy(&start_seq_number, ptr, len);
            break;
        case START_RTP_TIMESTAMP:
            valid_type_seen = TRUE;
            memcpy(&start_rtp_time, ptr, len);
            break;
        case DT_EARLIEST_TIME_TO_JOIN:
            valid_type_seen = TRUE;
            memcpy(&dt_earliest_join, ptr, len);
            break;
        case DT_REPAIR_END_TIME:
            valid_type_seen = TRUE;
            memcpy(&dt_repair_end, ptr, len);
            break;
        case TSRAP:
            valid_type_seen = TRUE;
            p_tlv = malloc(sizeof(ppdd_tlv_t) + len);
            if (!p_tlv) {
                RTP_LOG_ERROR("Failed to malloc a ppdd_tlv_t struct\n");
                return NULL;
            }
            memset(p_tlv, 0, sizeof(ppdd_tlv_t) + len);
            memcpy(p_tlv->tsrap, ptr, len);
            p_tlv->tsrap_len = len;
            break;
        case ACT_RCC_FILL:
            valid_type_seen = TRUE;
            memcpy(&act_rcc_fill, ptr, len);
            break;
        case ACT_RCC_FILL_AT_JOIN:
            valid_type_seen = TRUE;
            memcpy(&act_rcc_fill_at_join, ptr, len);
            break;
        case ER_HOLDOFF_TIME:
            valid_type_seen = TRUE;
            memcpy(&er_holdoff_time, ptr, len);
            break;
        case ACTUAL_FASTFILL:
            valid_type_seen = TRUE;
            memcpy(&actual_fastfill, ptr, len);
            break;
        case RCC_STATUS:
            /* Do nothing */
            break;
        }

        ptr += len;
    }

    /* Note p_tlv could be NULL at this point if we are decoding an explicit
     * NULL APP packet. In this case just allocate a ppdd_tlv_t struct.
     */
    if (valid_type_seen) {
        if (!p_tlv) {
            if ((p_tlv = malloc(sizeof(ppdd_tlv_t))) == NULL) {
                RTP_LOG_ERROR("Failed to malloc a ppdd_tlv_t struct\n");
                return NULL;
            }
            memset(p_tlv, 0, sizeof(ppdd_tlv_t));
        }

        p_tlv->start_seq_number = ntohl(start_seq_number);
        p_tlv->start_rtp_time = ntohl(start_rtp_time);
        p_tlv->dt_earliest_join = ntohl(dt_earliest_join);
        p_tlv->dt_repair_end = ntohl(dt_repair_end);
        p_tlv->act_rcc_fill = ntohl(act_rcc_fill);
        p_tlv->act_rcc_fill_at_join = ntohl(act_rcc_fill_at_join);
        p_tlv->er_holdoff_time = ntohl(er_holdoff_time);
        p_tlv->actual_fastfill = ntohl(actual_fastfill);
    }

    return p_tlv;
}

/**
 * plii_tlv_decode_allocate
 * @note
 *  Refer to header file rcc_tlv.h for complete function description.
 */
plii_tlv_t *plii_tlv_decode_allocate (uint8_t *data, uint16_t data_len)
{
    boolean valid_type_seen = FALSE;
    uint8_t *ptr = NULL, type;
    uint16_t len;
    uint8_t *p_end;
    plii_tlv_t *p_tlv = NULL;
    uint32_t min_rcc_fill = 0;
    uint32_t max_rcc_fill = 0;
    uint32_t do_fastfill = 0;
    uint32_t maximum_recv_bw = 0;
    uint32_t maximum_fastfill_time = 0;

    if (!data || data_len==0) {
        return (NULL);
    }

    ptr = data;
    p_end = data + data_len;
    while (ptr - data < data_len) {
        type = *ptr;
        ptr++;

        if (type == PADDING) {
            /* This type has no length */
            continue;
        }

        if (ptr + LENGTH_FIELD_LEN > p_end) {
            return (NULL);
        }
        memcpy(&len, ptr, sizeof(len));
        len = ntohs(len);
        ptr += LENGTH_FIELD_LEN;  /* 2-byte length field */
        if (ptr + len > p_end) {
            return (NULL);
        }

        /* Skip over unknown types */
        if (type >= MAX_RCC_TLV_TYPE) {
            ptr += len;
            continue;
        }

        /* Validate length field to protect against 
         * attacks/corrupted packets 
         */
        if (len > rcc_tl[type].len) { 
            return (NULL);
        }

        switch(type) {
        case MIN_RCC_FILL:
            valid_type_seen = TRUE;
            memcpy(&min_rcc_fill, ptr, len);
            break;
        case MAX_RCC_FILL:
            valid_type_seen = TRUE;
            memcpy(&max_rcc_fill, ptr, len);
            break;
        case DO_FASTFILL:
            valid_type_seen = TRUE;
            memcpy(&do_fastfill, ptr, len);
            break;
        case MAXIMUM_RECV_BW:
            valid_type_seen = TRUE;
            memcpy(&maximum_recv_bw, ptr, len);
            break;
        case MAXIMUM_FASTFILL_TIME:
            valid_type_seen = TRUE;
            memcpy(&maximum_fastfill_time, ptr, len);
            break;
        default:
            RTP_LOG_ERROR("Found an invalid type in a PLII APP message\n");
            break;
        }

        ptr += len;
    }

    if (valid_type_seen) {
        if ((p_tlv = malloc(sizeof(plii_tlv_t))) == NULL) {
            RTP_LOG_ERROR("Failed to malloc a plii_tlv_t struct\n");
            return NULL;
        }
        memset(p_tlv, 0, sizeof(plii_tlv_t));

        p_tlv->min_rcc_fill = ntohl(min_rcc_fill);
        p_tlv->max_rcc_fill = ntohl(max_rcc_fill);
        p_tlv->do_fastfill = ntohl(do_fastfill);
        p_tlv->maximum_recv_bw = ntohl(maximum_recv_bw);
        p_tlv->maximum_fastfill_time = ntohl(maximum_fastfill_time);
    }

    return p_tlv;
}

/**
 * ncsi_tlv_decode_allocate
 * @note
 *  Refer to header file rcc_tlv.h for complete function description.
 */
ncsi_tlv_t *ncsi_tlv_decode_allocate (uint8_t *data, uint16_t data_len)
{
    boolean valid_type_seen = FALSE;
    uint8_t *ptr = NULL, type;
    uint16_t len;
    uint8_t *p_end;
    ncsi_tlv_t *p_tlv = NULL;
    uint32_t first_mcast_seq_number = 0;
    uint32_t first_mcast_recv_time = 0;

    if (!data || data_len==0) {
        return (NULL);
    }

    ptr = data;
    p_end = data + data_len;
    while (ptr - data < data_len) {
        type = *ptr;
        ptr++;

        if (type == PADDING) {
            /* This type has no length */
            continue;
        }

        if (ptr + LENGTH_FIELD_LEN > p_end) {
            return (NULL);
        }
        memcpy(&len, ptr, sizeof(len));
        len = ntohs(len);
        ptr += LENGTH_FIELD_LEN;  /* 2-byte length field */
        if (ptr + len > p_end) {
            return (NULL);
        }

        /* Skip over unknown types */
        if (type >= MAX_RCC_TLV_TYPE) {
            ptr += len;
            continue;
        }

        /* Validate length field to protect against 
         * attacks/corrupted packets 
         */
        if (len > rcc_tl[type].len) { 
            return (NULL);
        }

        switch(type) {
        case FIRST_MCAST_PKT_SEQ_NUM:
            valid_type_seen = TRUE;
            memcpy(&first_mcast_seq_number, ptr, len);
            break;
        case FIRST_MCAST_PKT_RECV_TIME:
            valid_type_seen = TRUE;
            memcpy(&first_mcast_recv_time, ptr, len);
            break;
        default:
            RTP_LOG_ERROR("Found an invalid type in a NCSI APP message\n");
            break;
        }

        ptr += len;
    }

    if (valid_type_seen) {
        if ((p_tlv = malloc(sizeof(ncsi_tlv_t))) == NULL) {
            RTP_LOG_ERROR("Failed to malloc a ncsi_tlv_t struct\n");
            return NULL;
        }
        memset(p_tlv, 0, sizeof(ncsi_tlv_t));

        p_tlv->first_mcast_seq_number = ntohl(first_mcast_seq_number);
        p_tlv->first_mcast_recv_time = ntohl(first_mcast_recv_time);
    }

    return p_tlv;
}

/**
 * erri_tlv_decode_allocate
 * @note
 *  Refer to header file rcc_tlv.h for complete function description.
 */
erri_tlv_t *erri_tlv_decode_allocate (uint8_t *data, uint16_t data_len)
{
    boolean valid_type_seen = FALSE;
    uint8_t *ptr = NULL, type;
    uint16_t len;
    uint8_t *p_end;
    erri_tlv_t *p_tlv = NULL;
    uint32_t maximum_recv_bw = 0;

    if (!data || data_len==0) {
        return (NULL);
    }

    ptr = data;
    p_end = data + data_len;
    while (ptr - data < data_len) {
        type = *ptr;
        ptr++;

        if (type == PADDING) {
            /* This type has no length */
            continue;
        }

        if (ptr + LENGTH_FIELD_LEN > p_end) {
            return (NULL);
        }
        memcpy(&len, ptr, sizeof(len));
        len = ntohs(len);
        ptr += LENGTH_FIELD_LEN;  /* 2-byte length field */
        if (ptr + len > p_end) {
            return (NULL);
        }

        /* Skip over unknown types */
        if (type >= MAX_RCC_TLV_TYPE) {
            ptr += len;
            continue;
        }

        /* Validate length field to protect against 
         * attacks/corrupted packets 
         */
        if (len > rcc_tl[type].len) { 
            return (NULL);
        }

        switch(type) {
        case MAXIMUM_RECV_BW:
            valid_type_seen = TRUE;
            memcpy(&maximum_recv_bw, ptr, len);
            break;
        default:
            RTP_LOG_ERROR("Found an invalid type in a NCSI APP message\n");
            break;
        }

        ptr += len;
    }

    if (valid_type_seen) {
        if ((p_tlv = malloc(sizeof(erri_tlv_t))) == NULL) {
            RTP_LOG_ERROR("Failed to malloc a erri_tlv_t struct\n");
            return NULL;
        }
        memset(p_tlv, 0, sizeof(erri_tlv_t));

        p_tlv->maximum_recv_bw = ntohl(maximum_recv_bw);
    }

    return p_tlv;
}

/**
 * rcc_tlv_decode_destroy
 * @note
 *  Refer to header file rcc_tlv.h for complete function description.
 */
void rcc_tlv_decode_destroy (void *p_tlv)
{
    if (p_tlv) {
        free(p_tlv);
    }
}
