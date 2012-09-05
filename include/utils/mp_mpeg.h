/*------------------------------------------------------------------
 * Mpeg Parser.  
 * This file contains MPEG structures and definitions from  
 * the various MPEG RFCs. H.222,  H.262,  etc
 * April 2006, Jim Sullivan
 *
 * Copyright (c) 2006, 2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#ifndef __MP_MPEG_H__
#define __MP_MPEG_H__

#include <utils/vam_types.h>

/**
 * Definitions from MPEG Compression Spec H.262
 */
#define MP_MPEG_PES_STARTCODE_MASK 0xFFFFF1F0
typedef enum mp_startcode_ {
    /**
     * Picture Startcode 
     */
    MP_MPEG_STARTCODE_PICTURE       =0x0100,

    /**
     * Sequence Header Startcode 
     */
    MP_MPEG_STARTCODE_SEQHDR        =0x01B3,

    /**
     * Sequence Header Extension Startcode
     */
    MP_MPEG_STARTCODE_SEQEXT        =0x01B5,

    /**
     * Group of Pictures Startcode
     */
    MP_MPEG_STARTCODE_GROUP         =0x01B8,

    /**
     * PES Header reserved startcode value
     */
    MP_MPEG_STARTCODE_PES_RESERVED  =0x01E0,

    /**
     * PES PKT containing MPEG1 VIDEO payload
     */
    MP_MPEG_STARTCODE_PES_MPEG1VIDEO=0x01E1,

    /**
     * PES PKT containing MPEG2 VIDEO payload 
     */
    MP_MPEG_STARTCODE_PES_MPEG2VIDEO=0x01E2,

    /**
     * PES PKT containing H.264 VIDEO payload 
     */
    MP_MPEG_STARTCODE_PES_H264VIDEO =0x011B,
}mp_startcode_t;

/* See Picture Header in H.262 */
/**
 * Picture Type of MPEG-2 I-Frame
 */
#define MP_MPEG_PICTURE_TYPE_I 0x01

/**
 * Picture Type of MPEG-2 P-Frame
 */
#define MP_MPEG_PICTURE_TYPE_P 0x02

/**
 * Picture Type of MPEG-2 B-Frame
 */
#define MP_MPEG_PICTURE_TYPE_B 0x03
typedef struct VAM_PACKED mp_picthdr_ {
    uint32_t startcode;
    uint32_t w2;
}mp_picthdr_t;
#define MP_GET_PICTHDR_TYPE(picthdr_ptr)         \
        ((ntohl(picthdr_ptr->w2)>>19) & 0x03)

/* NAL Types (see H.264 ) 
 * Note: NAL types 1-5 are VCL NALs and the rest are non-VCL NALs */
typedef enum mp_mpeg_nal_type_ {
    MP_MPEG_NAL_TYPE_0=0, 
    MP_MPEG_NAL_TYPE_1,       /* coded slice of non-IDR pict */
    MP_MPEG_NAL_TYPE_2,       /* coded slice data petition A */
    MP_MPEG_NAL_TYPE_3,       /* coded slice data petition B */
    MP_MPEG_NAL_TYPE_4,       /* coded slice data petition C */
    MP_MPEG_NAL_TYPE_5,       /* coded slice of IDR pict     */
    MP_MPEG_NAL_TYPE_6,       /* SEI */
    MP_MPEG_NAL_TYPE_7,       /* SPS */
    MP_MPEG_NAL_TYPE_8,       /* PPS */
    MP_MPEG_NAL_TYPE_9,       /* access unit(AU) delimeter(AUD) */
    MP_MPEG_NAL_TYPE_10,      /* end of sequence */
    MP_MPEG_NAL_TYPE_11,      /* end of stream */
    MP_MPEG_NAL_TYPE_12       /* filler data */
}mp_mpeg_nal_type_t;
static inline boolean MP_IS_VCL_NAL(mp_mpeg_nal_type_t nal_type)
{
    boolean vcl_nal=FALSE; 
    if (( nal_type>=MP_MPEG_NAL_TYPE_1) && (nal_type<=MP_MPEG_NAL_TYPE_5)) {
        vcl_nal = TRUE; 
    }
    return (vcl_nal);
}

/* Primary Picture Types ( see H.264 Table 7.2 "Meaning of primary_pic_type") */
typedef enum mp_mpeg_ppt_ {
    MP_MPEG_PPT_TYPE_I=0,     /* I slices        */ 
    MP_MPEG_PPT_TYPE_P,       /* I,P slices      */
    MP_MPEG_PPT_TYPE_B,       /* I,P,B slices    */
    MP_MPEG_PPT_TYPE_SI,      /* SI slices       */
    MP_MPEG_PPT_TYPE_SP,      /* SI, SP slices   */
    MP_MPEG_PPT_TYPE_I_SI,    /* I, SI slices    */
    MP_MPEG_PPT_TYPE_P_SP,    /* I, SI, P, SP    */
    MP_MPEG_PPT_TYPE_B_SP,    /* I, SI, P, SP, B */
}mp_mpeg_ppt_t;
#define MP_GET_RBSP_AUDT_PPT(byte_ptr)               \
        ((*byte_ptr>>5)&0x07)

/**
 * SEI Message Types (H.264 Annex D) 
 */
typedef enum mp_h264_sei_ {
    MP_H264_SEI_BUFFERING=0,        /**< Buffering Period */
    MP_H264_SEI_PIC_TIMING,         /**< Picture Timing */
    MP_H264_SEI_PAN_SCAN,           /**< Pan-Scan Rectangle */
    MP_H264_SEI_FILLER_PAYLOAD,     /**< Filler Payload */
    MP_H264_SEI_USER_DATA_REG,      /**< User Data Registered to ITU T.35 */
    MP_H264_SEI_USER_DATA_UNREG,    /**< User Data Unregistered */
    MP_H264_SEI_RECOVERY_PT,        /**< Recovery Point */
    MP_H264_SEI_REF_PIC_REPEAT,     /**< Decoded Reference Picture Marking 
                                         Repetition */
    MP_H264_SEI_SPARE_PIC,          /**< Spare Picture */
    MP_H264_SEI_SCENE_INFO,         /**< Scene Info */
    MP_H264_SEI_SUB_SEQ_INFO,       /**< Sub-sequence Info */
    MP_H264_SEI_SUB_SEQ_LAYER_CHAR, /**< Sub-sequence Layer Characteristics */
    MP_H264_SEI_FF_FREEZE,          /**< Full Frame Freeze */
    MP_H264_SEI_FF_RELEASE,         /**< Full Frame Freeze Release */
    MP_H264_SEI_FF_SNAPSHOT,        /**< Full Frame Freeze Snapshot */
    MP_H264_SEI_PROG_REFINE_START,  /**< Progressive Refinement Segment Start*/
    MP_H264_SEI_PROG_REFINE_END,    /**< Progressive Refinement Segment End */
    MP_H264_SEI_MC_SLICE_GROUP,     /**< Motion Constrained Slice Group Set */
} mp_h264_sei_t;

/**
 * Slice Types (H.264 Table 7-3) 
 * 
 * @note (From H.264 7.4.3) 
 * slice_type values in the range 5..9 specify, in addition to the coding 
 * type of the current slice, that all other slices of the current coded 
 * picture shall have a value of slice_type equal to the current value of 
 * slice_type or equal to the current value of slice_type-5.
 */
typedef enum mp_h264_slice_type_ {
    MP_H264_P_SLICE=0,      /**< P Slice */
    MP_H264_B_SLICE,        /**< B Slice */
    MP_H264_I_SLICE,        /**< I Slice */
    MP_H264_SP_SLICE,       /**< SP Slice */
    MP_H264_SI_SLICE,       /**< SI Slice */
    MP_H264_ALL_P_SLICE,    /**< Only P Slices */
    MP_H264_ALL_B_SLICE,    /**< Only B Slices */
    MP_H264_ALL_I_SLICE,    /**< Only I Slices */
    MP_H264_ALL_SP_SLICE,   /**< Only SP Slices */
    MP_H264_ALL_SI_SLICE,   /**< Only SI Slices */
    MP_H264_LAST_SLICE
} mp_h264_slice_type_t;
        
/* Emulation byte sequence (H.264 Sect. 7.4.1) */
#define MP_MPEG_EMULATION_BYTES 0x000003

/* Profile Codes (H.264) */
#define MP_MPEG_H264_PROF_BASELINE 66
#define MP_MPEG_H264_PROF_MAIN 77
#define MP_MPEG_H264_PROF_HIGH 100

/* Sequence Parameter Set defines */
#define MP_MPEG_H264_SPS_MAX_SEQ_SCALE_LIST 8
#define MP_MPEG_H264_SPS_SMALL_SCALE_LIST_SIZE 16
#define MP_MPEG_H264_SPS_LARGE_SCALE_LIST_SIZE 64

/**
 * Definitions from MPEG Systems Spec H.222
 */
/**
 *@{
 */
/*************************************************
 * Structure definition and macros for the TS hdr
 *************************************************/
#define MP_MPEG_PID_PAT  0
#define MP_MPEG_PID_CAT  1
#define MP_MPEG_PID_TSDT 2
#define MP_MPEG_PID_NULL 0x1fff
#define MP_MPEG_PAD_BYTE 0xff
#define MP_TSHDR_SYNC_BYTE 0x47
#define MP_TSHDR_SCRAM_NONE 0x00
#define MP_TSHDR_SCRAM_ODD 0x01
#define MP_TSHDR_SCRAM_EVEN 0x02
#define MP_TSHDR_SCRAM_INVALID 0x03
#define MP_TSHDR_AF_PAYLOAD_PRESENT 0x01
#define MP_TSHDR_AF_PRESENT 0x02
#define MP_AF_MAXLEN 183
#define MP_MPEG_TSPKT_LEN 188
typedef struct VAM_PACKED  mp_tsheader_ {
    uint8_t   ts_b1; 
    uint16_t  ts_b23;
    uint8_t   ts_b4;
}mp_tsheader_t;

#define MP_GET_TSHDR_SYNC(thhdr_ptr)         \
        (thhdr_ptr)->ts_b1;  
#define MP_GET_TSHDR_PID(thhdr_ptr)         \
        (ntohs((thhdr_ptr)->ts_b23)&0x1fff )
#define MP_GET_TSHDR_PRI(thhdr_ptr)         \
        ((ntohs((thhdr_ptr)->ts_b23)>>13)&0x01)
#define MP_GET_TSHDR_PUSI(thhdr_ptr)         \
        ((ntohs((thhdr_ptr)->ts_b23)>>14)&0x01)
#define MP_GET_TSHDR_ERR(thhdr_ptr)         \
        ((ntohs((thhdr_ptr)->ts_b23)>>15)&0x01)
#define MP_GET_TSHDR_CONTCNT(thhdr_ptr)         \
        (((thhdr_ptr)->ts_b4 ) & 0x0f)
#define MP_GET_TSHDR_AF(thhdr_ptr)         \
        (((thhdr_ptr)->ts_b4 >>4) & 0x03)
#define MP_GET_TSHDR_SCRAM(thhdr_ptr)         \
        ((thhdr_ptr)->ts_b4>>6 & 0x03)

/* Macro to get the next continuity count given the current */ 
#define MP_GET_NEXT_MPEGCC(cc) (((cc)+1)&0x0f) 
#define MP_GET_PREV_MPEGCC(cc) (((cc)-1)&0x0f) 
#define MP_INVALID_CC 16

/*************************************************
 * Macros to access fields in the Adaptation
 *************************************************/
#define MP_GET_AF_LEN(afhdr_ptr)    \
     (*afhdr_ptr)
#define MP_GET_AF_DISCONT(afhdr_ptr) \
     (*afhdr_ptr>>7&0x01)
#define MP_GET_AF_RAPIND(afhdr_ptr) \
     (*afhdr_ptr>>6&0x01)
#define MP_GET_AF_PCRFLAG(afhdr_ptr) \
     (*afhdr_ptr>>4&0x01)
#define MP_GET_AF_PCRBASE(pcrval_ptr) \
     (ntohll(*pcrval_ptr)>>31)
#define MP_GET_AF_PCREXT(pcrval_ptr)  \
     ((ntohll(*pcrval_ptr)>>16)&0x1ff)
/**************************************************
 * Structure definition and macros for the PES hdr 
 **************************************************/
typedef struct VAM_PACKED  mp_peshdr_ {
    uint32_t  startcode;  
    uint16_t  pktlen;
    uint8_t   ctrl1;
    uint8_t   ctrl2;
    uint8_t   hdrlen; 
}mp_peshdr_t;
#define MP_GET_PESHDR_STARTCODE(peshdr_ptr)         \
        (ntohl((peshdr_ptr)->startcode))
#define MP_GET_PESHDR_PKTLEN(peshdr_ptr)         \
        (ntohs((peshdr_ptr)->pktlen))
#define MP_GET_PESHDR_SCRAM(peshdr_ptr)         \
        (((peshdr_ptr)->ctrl1>>4)&0x03)
#define MP_GET_PESHDR_PTSDTS(peshdr_ptr)         \
        (((peshdr_ptr)->ctrl2>>6)&0x03)
#define MP_GET_PESHDR_HDRLEN(peshdr_ptr)         \
        ((peshdr_ptr)->hdrlen)
#define MP_PESHDR_NO_PTSDTS_PRESENT 0x00
#define MP_PESHDR_DTS_PRESENT 0x01
#define MP_PESHDR_PTS_PRESENT 0x02

typedef struct VAM_PACKED  mp_pespts_ {
    uint8_t   pts_b1; 
    uint16_t  pts_b23;
    uint16_t  pts_b45;
}mp_pespts_t;
static inline 
unsigned long long MP_GET_PES_PTS(mp_pespts_t *pts_ptr) {
    unsigned long long pts=0; 
    uint16_t b45 = ntohs(pts_ptr->pts_b45)>>1;
    uint16_t b23 = ntohs(pts_ptr->pts_b23)>>1;
    uint8_t  b1  = (pts_ptr->pts_b1>>1)&0x07;
    pts = pts | b45 | (b23<<15) | ((unsigned long long)b1<<30);
    return (pts);
}

/**********************************************************
 * Structure definition and macros for the PSI section hdr 
 **********************************************************/
#define MP_PSI_TABLEID_PAT 0
#define MP_PSI_TABLEID_CAT 1
#define MP_PSI_TABLEID_PMT 2
#define MP_PSISECTION_LEN   1024
#define MP_PRIVSECTION_LEN  4096
#define MP_CA_PRIVSECTION_LEN  512 /* ETR 289 says 256, but Pkey says 512 */
#define MP_PSISECTHDR_BYTESP1 3  /* from start to/and including sect_len */ 
#define MP_PSISECTHDR_BYTESP2 5  /* from byte after sect_len to data_start */ 
typedef struct VAM_PACKED mp_secthdr_ {
    uint8_t table_id; 
    uint16_t sect_len;
    uint16_t program_number; 
    uint8_t  version; 
    uint8_t sect_num; 
    uint8_t sect_last; 
}mp_secthdr_t;
#define MP_GET_PSISECTHDR_TABLEID(psisect_ptr)   \
  (psisect_ptr)->table_id
#define MP_GET_PSISECTHDR_SSI(psisect_ptr)  \
  (ntohs((psisect_ptr)->sect_len)&0x8000)>>15
#define MP_GET_PSISECTHDR_LEN(psisect_ptr)   \
  (ntohs((psisect_ptr)->sect_len)&0x0fff)
#define MP_GET_PSISECTHDR_PROGNUM(psisect_ptr)   \
  (ntohs((psisect_ptr)->program_number))
#define MP_GET_PSISECTHDR_VERSION(psisect_ptr)   \
  ((psisect_ptr)->version>>1&0x01f)
#define MP_GET_PSISECTHDR_CURNEXT(psisect_ptr)   \
  ((psisect_ptr)->version&0x01)
#define MP_GET_PSISECTHDR_NUM(psisect_ptr)     \
  (psisect_ptr)->sect_num
#define MP_GET_PSISECTHDR_LAST(psisect_ptr)     \
  (psisect_ptr)->sect_last
#define MP_GET_PSISECT_CRC(patdata_ptr) \
  (ntohl(*(patdata_ptr)))

#define MP_GET_PID(patdata_ptr) \
  (ntohs(*(uint16_t*)patdata_ptr)&0x01FFF)
#define MP_GET_PATSECT_PROGRAM(patdata_ptr) \
  (ntohs(*(uint16_t*)patdata_ptr))
#define MP_GET_PATSECT_PID(patdata_ptr) \
  (ntohs(*(uint16_t*)patdata_ptr)&0x01FFF)
#define MP_GET_PMTSECT_PID(patdata_ptr) \
  (ntohs(*(uint16_t*)patdata_ptr)&0x01FFF)
#define MP_GET_PMTSECT_INFOLEN(patdata_ptr) \
  (ntohs(*(uint16_t*)patdata_ptr)&0x0FFF)
#define MP_GET_PMTSECT_STREAMTYPE(patdata_ptr) \
  *patdata_ptr

/* Elementary Stream Types(see table 2-29 in MPEG Systems Spec ) */
typedef enum mp_mpeg_streamtypes_ {
    MP_MPEG_STREAMTYPE_MPEG1VIDEO         =0x01,  /* ISO 11172 MPEG-1 video */ 
    MP_MPEG_STREAMTYPE_MPEG2VIDEO         =0x02,  /* ITU H.262 video */ 
    MP_MPEG_STREAMTYPE_MPEG1AUDIO         =0x03,  /* ISO 11172 MPEG-1 audio */ 
    MP_MPEG_STREAMTYPE_MPEG2AUDIO         =0x04,  /* ISO 13818-3 audio */
    MP_MPEG_STREAMTYPE_PRIVSECT           =0x05,  /* ITU H.222 private PSI sections */
    MP_MPEG_STREAMTYPE_PRIVDATA           =0x06,  /* ITU H.222 PES pkts containing private data */
    MP_MPEG_STREAMTYPE_MHEG               =0x07,  /* ISO 13522 language for describing multimedia */
    MP_MPEG_STREAMTYPE_DSMCC              =0x08,  /* ITU H.222 digital storage (DSMCC) */
    MP_MPEG_STREAMTYPE_H222_1             =0x09,  /* MPEG2(systems) over ATM */
    MP_MPEG_STREAMTYPE_DSMCC_A            =0x0a,  
    MP_MPEG_STREAMTYPE_DSMCC_B            =0x0b,
    MP_MPEG_STREAMTYPE_DSMCC_C            =0x0c,
    MP_MPEG_STREAMTYPE_DSMCC_D            =0x0d,
    MP_MPEG_STREAMTYPE_MPEG2AUX           =0x0e,  /* ISO 13818-1 auxiliary */
    MP_MPEG_STREAMTYPE_AACAUDIO           =0x0f,  /* ISO 13818-7 ADTS audio */ 
    MP_MPEG_STREAMTYPE_MPEG4VIDEO         =0x10,  /* ISO 14496-2 MPEG4 video */
    MP_MPEG_STREAMTYPE_MPEG4AUDIOAMD1     =0x11,  /* ISO 14496-3 MPEG4 audio */
    MP_MPEG_STREAMTYPE_MPEG4_14496_1_PES  =0x12,  /* ISO 14496-1 carried in PES pkts */
    MP_MPEG_STREAMTYPE_MPEG4_14496_1_SECT =0x13,  /* ISO 14496-1 carried in 14496 defined sections */
    MP_MPEG_STREAMTYPE_DSMCC_DL           =0x14,  /* ISO 13818-6 Synchronized Download Protocol */
    MP_MPEG_STREAMTYPE_METADATAPES        =0x15,  /* meta data carried in PES */
    MP_MPEG_STREAMTYPE_METADATASEC        =0x16,  /* meta data carried in metadata sections */
    MP_MPEG_STREAMTYPE_METADATADC         =0x17,  /* meta data in ISO 13818-6 DSMCC Data Carousel */
    MP_MPEG_STREAMTYPE_METADATAOC         =0x18,  /* meta data in ISO 13818-6 DSMCC Obj Carousel */
    MP_MPEG_STREAMTYPE_METADATADL         =0x19,  
    MP_MPEG_STREAMTYPE_MPEG2IPMP          =0x1a,  /* ISO 13818-11 MPEG-2 IPMP stream */
    MP_MPEG_STREAMTYPE_H264VIDEO          =0x1b,  /* ITU H.264 AVC */
    MP_MPEG_STREAMTYPE_TBD                =0x7f,  
    MP_MPEG_STREAMTYPE_OPENCBLVIDEO       =0x80,  /* User Private 0x80-0xff */
    MP_MPEG_STREAMTYPE_AC3AUDIO           =0x81,
    MP_MPEG_STREAMTYPE_DTSAUDIO           =0x8a
}mp_mpeg_streamtypes_t;
/**********************************************************
 * Structure definition and macros for MPEG descriptors
 **********************************************************/
   typedef enum mp_mpeg_descriptors_ {
       MP_MPEG_DESC_RES1=0, 
       MP_MPEG_DESC_RES2, 
       MP_MPEG_DESC_VIDEO_STREAM, 
       MP_MPEG_DESC_AUDIO_STREAM, 
       MP_MPEG_DESC_HIERARCHY, 
       MP_MPEG_DESC_REGISTRATION,
       MP_MPEG_DESC_DATA_ALIGN,
       MP_MPEG_DESC_TB_GRID, 
       MP_MPEG_DESC_VIDEO_WINDOW,
       MP_MPEG_DESC_CA,
       MP_MPEG_DESC_ISO639,
       MP_MPEG_DESC_SYSTEM_CLOCK,
       MP_MPEG_DESC_BUFFER_UTIL,
       MP_MPEG_DESC_COPYRIGHT,
       MP_MPEG_DESC_MAX_BITRATE,
       MP_MPEG_DESC_PRIV_DAT_IND,
       MP_MPEG_DESC_SMOOTHING_BUFFER,
       MP_MPEG_DESC_STD, 
       MP_MPEG_DESC_IBP, 
       MP_MPEG_DESC_RESERVED19_26, 
       MP_MPEG_DESC_MPEG4_VIDEO=27, 
       MP_MPEG_DESC_MPEG4_AUDIO, 
       MP_MPEG_DESC_IOD, 
       MP_MPEG_DESC_SL, 
       MP_MPEG_DESC_FMC, 
       MP_MPEG_DESC_EXT_ES_ID, 
       MP_MPEG_DESC_MUXCODE, 
       MP_MPEG_DESC_FMX_BUFSIZE,
       MP_MPEG_DESC_MULTIPLEX_BUFFER, 
        MP_MPEG_DESC_MAX, 
   }mp_mpeg_descriptors_t;
typedef struct VAM_PACKED mp_ca_desc_pac_ {
    uint8_t type; 
    uint8_t len;
    uint16_t sys_id; 
    uint16_t pid;
    uint8_t priv_data[256];
}mp_ca_desc_pac_t;
#define MP_GET_CADESC_SYSID(capac_ptr) \
  (ntohs(capac_ptr->sys_id))
#define MP_GET_CADESC_PID(capac_ptr) \
  (ntohs(capac_ptr->pid)&0x1fff)

/* PSI Table ID Section Types (See H.222 2.4.4.4, pg 44) */ 
typedef enum mp_mpeg_tableid_{
    MP_MPEG_TABLEID_PAS         = 0x00, /* Program Association Section */
    MP_MPEG_TABLEID_CAS         = 0x01, /* Conditional Access Section */
    MP_MPEG_TABLEID_PMS         = 0x02, /* Program Map Section */
    MP_MPEG_TABLEID_DESC        = 0x03, /* TS Description Section */
    MP_MPEG_TABLEID_SCENE_DESC  = 0x04, /* ISO 14496 Scene Description */
    MP_MPEG_TABLEID_OBJ_DESC    = 0x05, /* ISO 14496 Object Description */
    /* H.222 Reserved           = 0x06 - 0x37
       ISO 13818-6              = 0x38 - 0x3F 
       User Private             = 0x40 - 0xFE
    */
    MP_MPEG_TABLEID_FORBIDDEN   = 0xFF, /* Forbidden */
}mp_mpeg_tableid_t;

#define MP_MPEG_NIT_PROG_NUM 0
/**
 *@}
 */

/*************************************************************************************
 * PCR related functions
 *************************************************************************************/
#define MP_PCR_BASE_UNIT        90000     /* PCR base in units of 90000 */
#define MP_DEFAULT_TSRATE     3750000
#define MP_MINPCR_UNITS             1   

/***********************************************************************
 * Function: mp_get_pcrunit_per_byte
 * Description: This function calculates the time to send the specified
 * number of bytes at the specified rate. The time is expressed in 
 * PCR base units of 90000. 
 * Inputs: 
 * rate - TS rate in bit/sec
 * byte_count - Number of bytes 
 * Returns: Number of PCR units corresponding to the time to send 
 *          byte_count at rate 
 ***********************************************************************/
static inline
uint32_t mp_get_pcrunit_per_byte(uint32_t bit_rate, uint32_t byte_cnt)
{
    int units;
    uint32_t byte_rate;
    if (bit_rate == 0) {
        bit_rate = MP_DEFAULT_TSRATE;
    }
    byte_rate = bit_rate/8;
    units =  (MP_PCR_BASE_UNIT * byte_cnt)/byte_rate;
    units = (units > MP_MINPCR_UNITS) ? units : MP_MINPCR_UNITS; 
    return (units);
}

static inline
uint32_t mp_get_pcrunit_per_tspkt(uint32_t rate, uint32_t pkt_cnt)
{
    return(mp_get_pcrunit_per_byte(rate, pkt_cnt*188));
}
#endif




