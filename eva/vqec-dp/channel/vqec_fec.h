/*------------------------------------------------------------------
 *
 * Aug. 2007
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *------------------------------------------------------------------
 */

/**
 * vqec_fec.h - defines FEC functions for VQE-C.
 *
 * This FEC module mainly implements Pro-MPEG COP#3 release 2 and RFC 2733.
 * Please note the Pro-MPEG COP#3 was taken over by SMPTE under specifications
 * SMPTE 2022-1-2007 and SMPTE 2022-2-2007. 
 * 
 * Forward Error Correction (FEC) is a class of methods for controlling 
 * errors in an one-way communication system. FEC sender sends extra 
 * information (FEC packets) along with the data, which can be used by 
 * the receiver to detect and correct the lost data.
 *
 * In VQE-C, the received FEC packets are input to fec module, 
 * if there are data packet losses, fec module should try to recover them.
 * In this version, we implemented XOR fec.
 */

#ifndef __VQEC_FEC_H__
#define __VQEC_FEC_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <utils/vam_types.h>
#include "vqec_pak_seq.h"
#include <vqec_dp_api_types.h>
#include <rtp/fec_fast_type.h>

/**
 * Forward declaration
 */
struct vqec_pcm_;

#define VQEC_DP_FEC0_IS_HANDLE (1)
#define VQEC_DP_FEC1_IS_HANDLE (2)

/* need to align with VQEC_MAX_NAME_LEN */
#define VQEC_FEC_MAX_NAME_LEN    255

/* define size of FEC pak_seq */
#define VQEC_FEC_STREAM_RING_BUFFER_BUCKET_BITS 8
#define VQEC_FEC_STREAM_RING_BUFFER_SIZE   \
            (1 << VQEC_FEC_STREAM_RING_BUFFER_BUCKET_BITS)

/**
 * If the sequence number of a received FEC packet is 50 apart
 * from previous packet, we discard this new received packet. 
 * 50 is chosen based on MAX_D and MAX_L which are both 20.
 * we only consider 2 blocks delay which is 40, plus some 
 * redundancy 10. 
 */
#define VQEC_FEC_MAX_GAP_SIZE 50

/**
 * define L and D limitations in CoP#3r2 
 * L is number of columns, D is for rows
 * We extend the MAX_LD value to 256 from 
 * 100 in CoP#3r2 to support more stream
 * types. Some vendors are encoding with 
 * LxD larger than 100
 */
#define MAX_L       20
#define MAX_D       20
#define MAX_LD      256
#define MIN_L       1
#define MIN_D       4
#define MIN_L_IN_2D 4

#define MAX_XOR_CORRECTION 1

/**
 * define the available fec streams, 1D, 2D, none 
 */
typedef enum vqec_fec_stream_avail_ {
    VQEC_FEC_STREAM_AVAIL_NONE = 0,
    VQEC_FEC_STREAM_AVAIL_1D = 1,
    VQEC_FEC_STREAM_AVAIL_2D = 2,
} vqec_fec_stream_avail_t;

/**
 * define the fec types, CoP#3 only does XOR,
 * define this enum for future easy extension
 */ 
typedef enum vqec_fec_type_ {
    VQEC_FEC_TYPE_XOR =  0,
    VQEC_FEC_TYPE_UNKNOWN,
} vqec_fec_type_t;


/**
 * fec header defination, according to CoP#3r2
 */
typedef struct vqec_fec_hdr_ {
    uint16_t snbase_low_bits;     /* min seq number with this FEC pkt */
    uint16_t length_recovery;     /* number of protected media packets */
    uint32_t e_bit:1;             /* if 1 means header extension */  
    uint32_t pt_recovery: 7;      /* payload type */
    uint32_t mask: 24;            /* mask, set to 0 in COP#3r2 */
    uint32_t ts_recovery;         /* timestamp recovery */ 
    uint32_t x_bit:1;             /* resv. for future, set to 0 now */
    uint32_t d_bit:1;             /* 0 means column, 1 means row */
    uint32_t type:3;              /* 0: XOR; 1: Hamming; 2: RS */
    uint32_t index:3;             /* set to 0 in cop#3 */
    uint8_t offset:8;             /* L: column; 1: row */
    uint8_t na_bits:8;                 /* D: column; L: row */
    uint8_t snbase_ext_bits: 8;   /* set to 0 in COP#3r2 */
} vqec_fec_hdr_t;

/**
 * FEC set up information through sysconfig and SDP. The FEC module
 * is initialized from src at system bootup and channel change. 
 */
typedef struct vqec_fec_param_ {
    /**
     * this parameter is controlled by both sysconfig and SDP
     */
    boolean is_fec_enabled;              /* 1: enabled, 0: disabled */

    /**
     * these paprameters are from SDP 
     */
    vqec_fec_stream_avail_t avail_streams; /* available streams */
    vqec_fec_type_t fec_type_stream1;      /* type of fec used, stream1 */
    vqec_fec_type_t fec_type_stream2;      /* type of fec used, stream2 */
} vqec_fec_params_t;

typedef struct vqec_fec_stats_ {        /*!< stats for FEC module */
    uint64_t fec_late_paks;             /*!< late fec paks*/
    uint64_t fec_recovered_paks  ;      /*!< fec recovered pks*/
    uint64_t fec_dec_not_needed;        /*!< no need to decode paks*/
    uint64_t fec_duplicate_paks;        /*!< fec duplicate packet count */
    uint64_t fec_total_paks;            /*!< total fec paks received */
    uint64_t fec_rtp_hdr_invalid;       /*!< # paks with invalid rtp hdr */
    uint64_t fec_hdr_invalid;           /*!< # paks with invalid fec hdr */
    uint64_t fec_unrecoverable_paks;    /*!< # fec paks can not recover video*/
    uint64_t fec_drops_other;           /*!< fec pak drops, other reason*/ 
    uint64_t fec_gap_detected;          /*!< fec gap detected */
} vqec_fec_stats_t;

/**
 * fec module main structure defination
 */
typedef struct vqec_fec_ {
    vqec_pak_seq_t *pak_seq;             /* real buffer to store data packet */
    vqec_seq_num_t head;                 /* the smallest data  seq num */
    vqec_seq_num_t tail;                 /* the largest data seq num */

    vqec_pak_seq_t *fec_pak_seq_column;  /* store column  packet */
    vqec_seq_num_t fec_column_head;      /* the smallest column fec seq num */
    vqec_seq_num_t fec_column_tail;      /* the largest column fec seq num */
 
    vqec_pak_seq_t *fec_pak_seq_row;     /* real buffer to store row packet */
    vqec_seq_num_t fec_row_head;         /* the smallest row fec seq num */
    vqec_seq_num_t fec_row_tail;         /* the largest row fec  seq num */

    vqec_fec_info_t fec_info;            /* cache the learned information here */
 
    /**
     * The two are used for streams indicator, for example, SDP says
     * 2-D FEC, but only 1 stream is available. The init value of these
     * two are FALSE.
     */
    boolean column_avail;                /* true: if col fec stream avail */
    boolean row_avail;                   /* true: if row fec stream avail */

    /**
     * store FEC information from sysconfig and SDP
     */
    vqec_fec_params_t fec_params;
    vqec_fec_stats_t fec_stats;  /* fec stats */
    vqec_fec_stats_t fec_stats_snapshot;  /* fec stats snapshot*/
 
    vqec_seq_num_t last_rx_seq_num_column;
    vqec_seq_num_t last_rx_seq_num_row;  /* use to change 16 bits
                                          * rtp seq number into 32
                                          * bits vqec seq number.
                                          */
    vqec_seq_num_t largest_seq_num_recved_column;
    vqec_seq_num_t largest_seq_num_recved_row;
    struct vqec_pcm_ *pcm;               /* pcm to get the real packet */
    uint32_t chanid;                     /* ID of the associated dpchan */

    boolean first_pak_processed;         /* used for associate streams */
    boolean is_stream1_column;           /* with column or row         */

   /* used to determine the sending order Annex A or B*/
    boolean sending_order_detected;      /* is sending order detected? */
    boolean num_paks_saved;
    vqec_seq_num_t fst_fec_seq32;
    vqec_seq_num_t fst_sn_base32;
    vqec_seq_num_t snd_fec_seq32;
    vqec_seq_num_t snd_sn_base32;
    uint8_t saved_L;
    uint8_t saved_D;

} vqec_fec_t;

extern boolean g_fec_enabled;

/**
 * Gets the current global FEC enable/disable setting for demonstration
 * purposes.  Returns TRUE if the FEC demo knob is set to TRUE (or is unset),
 * and FALSE otherwise.  NOTE:  This does NOT incorporate the system
 * configuration setting.
 *
 * @param[out] boolean - TRUE if enabled, FALSE if disabled.
 */
static inline boolean
vqec_dp_get_fec_inline (void)
{
    return (g_fec_enabled);
}

/**
 * Initialize the fec module using the the
 * data plane initialization parameters. Responsible for
 * setting aside the appropriate module resources.
 *
 * @param[in] max_fecs Maximum number of simultaneous FEC objects
 * @param[out] vqec_dp_error_t Return VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_fec_module_init (uint32_t max_fecs);

/**
 * Deinitialize the output scheduler module. Relinquish any
 * allocations made by the vqec_fec_module_init function.
 *
 * @params[out] vqec_dp_error_t Returns VQEC_DP_ERR_OK on success.
 */
vqec_dp_error_t
vqec_fec_module_deinit (void);


/**
 * This function create fec module.
 * The caller first fills the fec_param stucture with
 * parameters from SDP and sysconfig. During creation, fec 
 * structure is dynamic allocated, initialized and then returned
 * 
 * @param[in] fec_param  pointer to fec parameter structure
 * @param[in] pcm Associated pcm pointer.
 *
 * @return    initialized fec module, otherwise NULL
 */
vqec_fec_t *vqec_fec_create(vqec_fec_params_t *vqec_fec_param, 
                            struct vqec_pcm_ *pcm,
                            uint32_t dpchan_id);

/**
 * destroy fec module, clean the whole module, and free the fec buffer
 *
 * @param[in] fec   pointer to fec module
 *
 */
void vqec_fec_destroy(vqec_fec_t *fec);

/**
 * get fec statistics counters
 *
 * @param[in] fec_stats     pointer to fec module
 * @param[in] stats         tuner stats pointer
 * @param[in] cumulative  flag to retrieve cumulative status
 */
void vqec_fec_get_status(vqec_fec_t *fec, vqec_dp_fec_status_t *s, 
                         boolean cumulative);

/** 
 * clear fec counters at fec module initilization
 * @param[in] fec_stats     
 *
 * @retrun an error code
 */
void vqec_fec_counter_clear(vqec_fec_t *fec);

/**
 * flush fec module
 * flush FEC pak_sequence for column or row FEC

 * @param[in] fec               pointer to fec module
 * @param[in] is_column_fec     if column TRUE, if row FALSE
 *
 * @return                      TRUE if success, otherwise FALSE
 */
boolean vqec_fec_flush(vqec_fec_t *fec, boolean is_column_fec);


boolean vqec_fec_receive_packet (vqec_fec_t *fec,
                                 vqec_pak_t *pak,
                                 uint32_t handle);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VQEC_FEC_H */


