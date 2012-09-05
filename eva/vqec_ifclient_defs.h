//------------------------------------------------------------------
// VQEC. Definitions and types for client interface API.
//
//
// Copyright (c) 2007-2010, 2012 by cisco Systems, Inc.
// All rights reserved.
//------------------------------------------------------------------

#ifndef __VQEC_IFCLIENT_DEFS_H__
#define __VQEC_IFCLIENT_DEFS_H__

#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include "vqec_ifclient_read.h"
#ifdef HAVE_IPLM
#include "iplm_ifclient_defs.h"
#endif
#include "vqec_tr135_defs.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

//----------------------------------------------------------------------------
/// @defgroup gdefs Global constants.
/// @{
/// The maxima's are bounds enforced by the implementation. Function calls
/// that use arguments outside these upper bounds will truncate inputs to 
/// these values. Outputs will also be truncated accordingly.
//----------------------------------------------------------------------------
#define VQEC_MSG_MAX_RECV_TIMEOUT       (100) 
                                        //!< Max timeout for blocking reads (msec)
#define VQEC_MSG_MAX_IOBUF_CNT          (32)	
                                        //!< Max limit on the msg iobuf array
#define VQEC_MSG_MAX_DATAGRAM_LEN	(1512)	
                                        //!< Maximum supported datagram size
#define VQEC_MSG_FLAGS_APP              (0x02)	
                                        //!< Message contents is in APP packet


#define VQEC_MAX_TUNER_NAMESTR_LEN      (16)    //!< Max limit on tuner name


/*
 * Defines the longest valid CNAME string that may be assigned to VQE-C.
 * Includes the terminating NULL character.
 */
#define VQEC_MAX_CNAME_LEN    33

/*
 * Identifies histograms supported by VQE-C.
 *
 * VQEC_HIST_JOIN_DELAY:
 * Delay intervals between join request and receipt of
 * first primary packet of all channels (past and present).
 *
 * VQEC_HIST_OUTPUTSCHED:
 * Delay intervals between instances of output interval scheduling. 
 *
 * VQEC_HIST_MAX:
 * Unused, must be last.
 */
typedef enum vqec_hist_t_ {
    VQEC_HIST_JOIN_DELAY = 0,
    VQEC_HIST_OUTPUTSCHED,
    VQEC_HIST_MAX
} vqec_hist_t;


//----------------------------------------------------------------------------
/// @}
/// @defgroup structs Data Structures.  
/// @{
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
/// Tuner iterator structure.
//----------------------------------------------------------------------------
typedef struct vqec_tuner_iter_t_
{
    vqec_tunerid_t i_cur;               //!< Current position
    vqec_tunerid_t (*getnext)(struct vqec_tuner_iter_t_ *);
                                        //!< Returns tuner-id for current pos
} vqec_tuner_iter_t;

/*----------------------------------------------------------------------------
 * Decapsulated RTP packet header information.
 *
 * Note:  all fields are in host byte-order.
 *--------------------------------------------------------------------------*/
typedef struct vqec_rtphdr_t_
{
    uint16_t  combined_bits;     /* RTP V,P,X,CC,M & PT */
    uint16_t  sequence;          /* RTP sequence */
    uint32_t  timestamp;         /* RTP timestamp */
    uint32_t  ssrc;              /* Syncronization source identifier */
    uint32_t  header_bytes;      /* Size of RTP header in bytes - decapsulated
                                    packet buffer starts after this offset */
    uint8_t  *rtp_payload;       /* Start of the decapsulated buffer within
                                    the RTP packet */
} vqec_rtphdr_t;

/*
 * Fast path masks for combined_bits field
 */
#define VQEC_RTP_VERSION_SHIFT       14
#define VQEC_RTP_PADDING_SHIFT       13
#define VQEC_RTP_EXT_SHIFT           12
#define VQEC_RTP_CSRCC_SHIFT          8
#define VQEC_RTP_MARKER_SHIFT         7
#define VQEC_RTP_PAYLOAD_SHIFT        0

#define VQEC_RTP_VERSION(a)     ((((a)->combined_bits) & 0xc000)        \
                            >> VQEC_RTP_VERSION_SHIFT)         /* Version  */
#define VQEC_RTP_PADDING(a)     ((((a)->combined_bits) & 0x2000)        \
                            >> VQEC_RTP_PADDING_SHIFT)         /* Padding  */
#define VQEC_RTP_EXT(a)         ((((a)->combined_bits) & 0x1000)        \
                            >> VQEC_RTP_EXT_SHIFT)             /* Extension */
#define VQEC_RTP_CSRCC(a)       (((((a)->combined_bits) & 0x0f00)       \
                            >> VQEC_RTP_CSRCC_SHIFT) & 0x000f) /* CSRC count */
#define VQEC_RTP_MARKER(a)      ((((a)->combined_bits) & 0x0080)        \
                            >> VQEC_RTP_MARKER_SHIFT)          /* Marker */
#define VQEC_RTP_PAYLOAD(a)     ((((a)->combined_bits) & 0x007f)        \
                            >> VQEC_RTP_PAYLOAD_SHIFT)         /* Payload */


/*
 * Reasons that a tuner's rapid channel change operation may have aborted
 */
typedef enum vqec_ifclient_rcc_abort_t_ {
    VQEC_IFCLIENT_RCC_ABORT_SERVER_REJECT,
                                 /*!< RCC request rejected by VQE-S */
    VQEC_IFCLIENT_RCC_ABORT_STUN_TIMEOUT,
                                 /*!< STUN response not rcvd from VQE-S */
    VQEC_IFCLIENT_RCC_ABORT_RESPONSE_TIMEOUT,
                                 /*!< APP packet not received from VQE-S */
    VQEC_IFCLIENT_RCC_ABORT_RESPONSE_INVALID,
                                 /*!< invalid contents within rcvd APP pkt */
    VQEC_IFCLIENT_RCC_ABORT_BURST_START,
                                 /*!< first repair pkt not received in time */ 
    VQEC_IFCLIENT_RCC_ABORT_BURST_ACTIVITY,
                                 /*!< repair burst activity timed out */
    VQEC_IFCLIENT_RCC_ABORT_OTHER,
                                 /*!< internal errors, etc. */
    VQEC_IFCLIENT_RCC_ABORT_MAX
                                 /*!< unused - MUST BE LAST */
} vqec_ifclient_rcc_abort_t;

#define VQE_STAT_COUNTERS                                                      \
    /**/                                                                       \
    /* Stream Packet Counters */                                               \
    /**/                                                                       \
                                                                               \
    /* primary rtp */                                                          \
    uint64_t primary_udp_inputs;         /*!< primary udp mpeg pkts received */\
    uint64_t primary_udp_drops;          /*!<                                */\
                                         /*!< primary udp mpeg pkts dropped, */\
                                         /*!< due to a failure to have a     */\
                                         /*!< valid MPEG sync byte as the    */\
                                         /*!< first byte in the pkt payload  */\
                                                                               \
    uint64_t primary_rtp_inputs;         /*!< primary rtp pkts received      */\
    uint64_t primary_rtp_drops;          /*!<                                */\
                                         /*!< primary rtp pkts dropped, due  */\
                                         /*!< to reasons such as:            */\
                                         /*!<  o RTP parse failure           */\
                                         /*!<  o too early (before join)     */\
                                         /*!<  o too late for playout        */\
                                         /*!<  o drop simulator tool         */\
                                         /*!< This counter EXCLUDES drops due*/\
                                         /*!< to duplicate pkts being recvd  */\
                                         /*                                  */\
    uint64_t primary_rtp_drops_late;     /*!<                                */\
                                         /*!< primary rtp pkts dropped due to*/\
                                         /*!< arriving too late (after time  */\
                                         /*!< needed by output scheduler)    */\
                                         /*                                  */\
                                                                               \
    /* primary rtcp */                                                         \
    uint64_t primary_rtcp_inputs;        /*!< primary rtcp pkts received     */\
    uint64_t primary_rtcp_outputs;       /*!<                                */\
                                         /*!< primary rtcp packets sent      */\
                                         /*                                  */\
    /* repair rtp */                                                           \
    uint64_t repair_rtp_inputs;          /*!< repair/rcc rtp pkts received   */\
    uint64_t repair_rtp_drops;           /*!<                                */\
                                         /*!< repair/rcc rtp pkts dropped,   */\
                                         /*!< due to reasons such as:        */\
                                         /*!<  o RTP parse failure           */\
                                         /*!<  o too early (before join)     */\
                                         /*!<  o too late for playout        */\
                                         /*!<  o repairs preceed first       */\
                                         /*!<    sequence number from RCC    */\
                                         /*!(    APP                         */\
                                         /*!<  o drop simulator tool         */\
                                         /*!< This counter EXCLUDES drops due*/\
                                         /*!< to duplicate pkts being        */\
                                         /*!( received.                      */\
                                         /*                                  */\
    uint64_t repair_rtp_drops_late;      /*!                                 */\
                                         /*!< repair/rcc rtp pkts dropped due*/\
                                         /*!< to arriving too late (after    */\
                                         /*!< time needed by output          */\
                                         /*!( scheduler)                     */\
                                         /*                                  */\
                                         /*                                  */\
    /* repair rtcp */                                                          \
    uint64_t repair_rtcp_inputs;         /*!< repair/rcc rtcp pkts received  */\
                                                                               \
    /* fec */                                                                  \
    uint64_t fec_inputs;                 /*!< fec pkts received              */\
    uint64_t fec_drops;                  /*!<                                */\
                                         /*!< fec pkts dropped, due to       */\
                                         /*!< reasons such as:               */\
                                         /*!<    o invalid RTP header        */\
                                         /*!<    o invalid FEC header        */\
                                         /*!<    o packet arrived too late   */\
                                         /*!<    o memory allocation error   */\
                                         /*!<      while processing FEC pkt  */\
                                         /*!<    o etc.                      */\
                                                                               \
    uint64_t fec_drops_late;             /*!<                                */\
                                         /*!< fec pkts which arrived too late*/\
                                         /*!<  (a primary packet to which it */\
                                         /*!<   refers has already been      */\
                                         /*!<   scheduled for output)        */\
                                         /*                                  */\
    /* repair rtp stun */                                                      \
    uint64_t repair_rtp_stun_inputs;     /*!<                                */\
                                         /*!< STUN pts rcvd on repair rtp    */\
                                         /*!  port                           */\
                                         /*!                                 */\
    uint64_t repair_rtp_stun_outputs;    /*!<                                */\
                                         /*!< STUN pkts sent on repair rtp   */\
                                         /*!( port                           */\
                                         /*!                                 */\
    /* repair rtcp stun */                                                     \
    uint64_t repair_rtcp_stun_inputs;    /*!<                                */\
                                         /*!< STUN pkts rcvd on repair rtcp  */\
                                         /*!< port                           */\
                                         /*!                                 */\
    uint64_t repair_rtcp_stun_outputs;   /*!<                                */\
                                         /*!< STUN pkts sent on repair rtcp  */\
                                         /*!< port                           */\
                                         /*!                                 */\
                                                                               \
    /* merged stream information */                                            \
    uint64_t post_repair_outputs;        /*!<                                */\
                                         /*!< post repair stream packets     */\
                                         /*!< (common to all tuners which    */\
                                         /*!<  are tuned to the same channel)*/\
                                                                               \
    uint64_t tuner_queue_drops;          /*!<                                */\
                                         /*!< drops during pak enqueue on    */\
                                         /*!< tuner/sink (e.g. due to queue  */\
                                         /*!< limit reached)                 */\
                                         /*!                                 */\
    uint64_t underruns;                  /*!<                                */\
                                         /*!< underruns upon inserting       */\
                                         /*!< packets of the input streams   */\
                                         /*!(                                */\
                                                                               \
    /* Repair Packet Counters */                                               \
    uint64_t pre_repair_losses;          /*!<                                */\
                                         /*!< number of primary rtp pkts not */\
                                         /*!< arriving within the stream     */\
                                         /*!<                                */\
                                         /*!< E.g. an arriving pkt stream of */\
                                         /*!<      sequence numbers          */\
                                         /*!<      1,4,5,7,8                 */\
                                         /*!<      will bump this            */\
                                         /*!<      counter 3 times           */\
                                         /*!(                                */\
                                                                               \
    uint64_t post_repair_losses;         /*!<                                */\
                                         /*!< number of rtp packets which    */\
                                         /*!< were missing (not repaired)    */\
                                         /*!< upon output to the tuner       */\
                                         /*!(                                */\
                                                                               \
    uint64_t post_repair_losses_rcc;     /*!<                                */\
                                         /*!< number of rcc rtp pkts which   */\
                                         /*!< were missing (not repaired)    */\
                                         /*!< upon output to the tuner       */\
                                         /*!<                                */\
                                         /*!< I.e. any packets missing from  */\
                                         /*!<      an RCC burst and not      */\
                                         /*!<      repaired would be         */\
                                         /*!<      counted by this counter   */\
                                         /*!(                                */\
                                                                               \
    /* er */                                                                   \
    uint64_t repairs_requested;          /*!<                                */\
                                         /*!< number of repair packets       */\
                                         /*!<  requested by VQE-C            */\
                                         /*!(                                */\
    uint64_t repairs_policed;            /*!<                                */\
                                         /*!< number of repair requests      */\
                                         /*!<  not sent due to rate limiting */\
                                         /*!(                                */\
    /* fec */                                                                  \
    uint64_t fec_recovered_paks;         /*!<                                */\
                                         /*!< packets successfully           */\
                                         /*!<  regenerated/repaired by FEC   */\
                                         /*!(                                */\
    /* Channel Change Counters */                                              \
    uint32_t channel_change_requests;    /*!<                                */\
                                         /*!< number of channel changes      */\
                                         /*!< attempted                      */\
                                         /*!(                                */\
    uint32_t rcc_requests;               /*!<                                */\
                                         /*!< number of rapid channel changes*/\
                                         /*!< attempted                      */\
                                         /*!(                                */\
    uint32_t concurrent_rccs_limited;    /*!<                                */ \
                                         /*!< number of rapid channel changes*/\
                                         /*!< limited due to too many active */\
                                         /*!< RCCs already existing          */\
                                         /*!(                                */\
    uint32_t rcc_with_loss;              /*!<                                */\
                                         /*!< number of rapid channel changes*/\
                                         /*!< with rcc loss                  */\
                                         /*!(                                */\
    uint32_t rcc_aborts_total;           /*!<                                */\
                                         /*!< number of rapid channel changes*/\
                                         /*!< which aborted (see reasons     */\
                                         /*!( below)                         */\
                                         /*!(                                */\
    uint32_t rcc_abort_stats[VQEC_IFCLIENT_RCC_ABORT_MAX + 1];                 \
                                         /*!<                                */\
                                         /*!< counts for each abort reason   */\
                                         /*!*                                */

//----------------------------------------------------------------------------
// Global statistics for channel/tuner data
//----------------------------------------------------------------------------
typedef 
struct vqec_ifclient_stats_t_
{
   VQE_STAT_COUNTERS
} vqec_ifclient_stats_t;

//----------------------------------------------------------------------------
// Per-channel statistics
//----------------------------------------------------------------------------
typedef
struct vqec_ifclient_stats_channel_
{
    /* 
     * We maintain the same counters on a per-channel basis as that are
     * maintained as global counters. The counters here
     * will retain the statistics pertinent for this particular channel, and
     * are reset every time the channel is unbound
     */
    VQE_STAT_COUNTERS

    /* 
     * Note that VQE_STAT_COUNTERS *MUST* be the at the beginning of this
     * structure because modules expect this to be there and typecast
     * accordingly
     */
 
    uint64_t primary_rtp_expected;
                            /*
                             * Number of packets expected for this
                             * channel's primary session.
                             *
                             * From RFC 3550:
                             * The number of packets expected can be computed
                             * by the receiver as the difference between the
                             * highest sequence number received [...] and the
                             * first sequence number received [...].
                             */
    int64_t  primary_rtp_lost;
                            /*
                             * Number of packets lost for this channel's
                             * primary session
                             *
                             * From RFC 3550:
                             * The number of packets lost is defined to be 
                             * the number of packets expected less the
                             * number of packets actually received.
                             * The number of packets received is simply
                             * the count of packets as they arrive, including
                             * any late or duplicate packets.
                             */

    /* Counters for TR-135 support */ 
    uint64_t tr135_overruns;
    uint64_t tr135_underruns;
    uint64_t tr135_packets_expected;
    uint64_t tr135_packets_received;
    uint64_t tr135_packets_lost;
    uint64_t tr135_packets_lost_before_ec;
    uint64_t tr135_loss_events;
    uint64_t tr135_loss_events_before_ec;
    uint64_t tr135_severe_loss_index_count;
    uint64_t tr135_minimum_loss_distance;
    uint64_t tr135_maximum_loss_period;
    uint64_t tr135_buffer_size;
    uint32_t tr135_gmin;
    uint32_t tr135_severe_loss_min_distance;
} vqec_ifclient_stats_channel_t;

/*----------------------------------------------------------------------------
 * Statistics on updates.
 *--------------------------------------------------------------------------*/
#define VQEC_UPDATER_VERSION_STRLEN 33
typedef 
struct vqec_ifclient_updater_stats_t_
{
    int32_t updater_update_in_progress;        /*!< indicates whether or not
                                                  a updater_update is 
                                                  currently in progress */
    struct timeval updater_last_update_time;   /*!< the system time of the
                                                  most recent successful
                                                  updater_udpate */
    uint16_t chan_cfg_total_channels;           /*!< total number of active
                                                  channels in the current
                                                  channel configuration */
    char chan_cfg_version[VQEC_UPDATER_VERSION_STRLEN];
                                                /*!<
                                                 *!< version identifier of
                                                 *!< channel config file
                                                 */
} vqec_ifclient_updater_stats_t;


/**
 * Decoder statistics imported by the client.
 */
typedef struct vqec_ifclient_dcr_stats_
{
    uint32_t dec_picture_cnt;           /* decoded picture count */
    uint64_t fst_decode_time;           /* first frame decode time*/
    uint64_t last_decoded_pts;          /* last decoded pts */
    uint64_t ir_time;                   /* remote control CC time*/
    uint64_t pat_time;                  /* pat found time */
    uint64_t pmt_time;                  /* pmt found time */
    uint64_t display_time;              /* first frame display time */
    uint64_t display_pts;               /* first displayed PTS value */
} vqec_ifclient_dcr_stats_t;

/**
 * Methods which if registered will allow the client to control and report-on
 * on the progress of various CC optimizations.
 * We use a structure here for easy extension in the future.  
 */
typedef struct vqec_ifclient_get_dcr_info_ops_
{
    int32_t (*get_dcr_stats)(int32_t context_id, 
                             vqec_ifclient_dcr_stats_t *stats);
} vqec_ifclient_get_dcr_info_ops_t;

/*
 * Reasons that a fastfill operation may have aborted
 */
typedef enum vqec_ifclient_fastfill_abort_t_ {
    VQEC_IFCLIENT_FASTFILL_ABORT_SERVER,
                                 /*!<fastfill request rejected by VQE-S */
    VQEC_IFCLIENT_FASTFILL_ABORT_CLIENT,
                                 /*!< RCC aborted */
    VQEC_IFCLIENT_FASTFILL_ABORT_MAX
                                 /*!< unused - MUST BE LAST */
} vqec_ifclient_fastfill_abort_t;

/*---------------------------------------------------------------------------
 * Fastfill status at the time that a callback is invoked.
 *--------------------------------------------------------------------------*/ 
typedef struct vqec_ifclient_fastfill_status_
{
    uint32_t bytes_filled;      /* Bytes sent till now in the fast-fill burst */
    uint32_t elapsed_time;      /* Fill-time (stream time) that has elapsed */
    vqec_ifclient_fastfill_abort_t abort_reason;   /* fastfill abort reason */
} vqec_ifclient_fastfill_status_t;

/*---------------------------------------------------------------------------
 * Fastfill parameters used in the (*fastfill_start) callback.
 *--------------------------------------------------------------------------*/ 
typedef struct vqec_ifclient_fastfill_params_
{
    uint64_t first_pcr;         /* First pcr value from APP (PCR units) */
    uint32_t fill_total_time;   /* Total fill time from APP (PCR units) */
} vqec_ifclient_fastfill_params_t;

/*---------------------------------------------------------------------------
 * Fastfill callback operations.  Applications can register callbacks at bind
 * time using bind parameters to the methods below. 
 *
 * The fastfill feature accelerates channel changes by sending data more 
 * quickly to the decoder.  Integrations supporting fastfill are required 
 * to enable fastfill during the bind and implement the following fastfill_ops 
 * vectors (also passed at bind time inside the bind_parames).
 * 
 * The integrator of VQE fastfill is responsible for making sure that the 
 * decoder is ready to receive packets at a rate higher than the stream rate.
 * This may include making sure that either the decoder's tracking clock or 
 * tracking loop is disabled.
 * 
 *   (*fastfill_start) is called if fastfill is enabled, and also the server 
 *                     acknowledged that the burst will come in a higher rate.
 *   (*fastfill_abort) is called if fastfill is aborted after the initial bind.
 *   (*fastfill_done) is called when the fill duration is complete. 
 * 
 * Structures are used as input parameters for extensibility in the future.
 *--------------------------------------------------------------------------*/ 
typedef struct vqec_ifclient_fastfill_cb_ops_
{
    void (*fastfill_start)(int32_t context_id, 
                          vqec_ifclient_fastfill_params_t *params);
    void (*fastfill_abort)(int32_t context_id, 
                           vqec_ifclient_fastfill_status_t *status);
    void (*fastfill_done)(int32_t context_id, 
                           vqec_ifclient_fastfill_status_t *status);

} vqec_ifclient_fastfill_cb_ops_t;


/**
 * chan event defined for chan event call back function.
 * INACTIVE  - triggered when incoming data for a channel 
 * has been active, but then stops 
 * NEW_SOURCE - triggered when a channel's existing source changes 
 * (i.e. not when the channel's first source appears)
 *
 */
typedef enum vqec_ifclient_chan_event_
{
    VQEC_IFCLIENT_CHAN_EVENT_UNKNOWN,
    VQEC_IFCLIENT_CHAN_EVENT_INACTIVE,     /* chan is inactive */
    VQEC_IFCLIENT_CHAN_EVENT_NEW_SOURCE,   /* chan has now source*/
} vqec_ifclient_chan_event_t;

/**
 * chan event call back function argument
 */
typedef struct vqec_ifclient_chan_event_args_ {
    vqec_ifclient_chan_event_t event;
} vqec_ifclient_chan_event_args_t;

/**
 * Methods which if registered will allow the client to signal a channel
 * event.
 * The callback is per channel basis.
 * We use a structure here for easy extension in the future.  
 */
typedef struct vqec_ifclient_chan_event_cb_
{
    uint32_t chan_event_context;
    void (*chan_event_cb)(int32_t chan_event_context, 
                          vqec_ifclient_chan_event_args_t *cb_args);
} vqec_ifclient_chan_event_cb_t;

/**
 * Parameters when add a tmp chan
 */
typedef struct vqec_ifclient_tmp_chan_params_
{
    vqec_ifclient_chan_event_cb_t event_cb;
} vqec_ifclient_tmp_chan_params_t;

/*---------------------------------------------------------------------------
 * Get the dcr operations structure which will allow the client
 * to control various CC optimizations.
 *
 * retun the pointer of the dcr operations
 *--------------------------------------------------------------------------*/ 
vqec_ifclient_get_dcr_info_ops_t * get_dcr_ops(void);

typedef struct vqec_bind_params_ vqec_bind_params_t;
typedef struct vqec_update_params_ vqec_update_params_t;

/**
 * vqec_sdp_handle_t - defines the per tv channel globally unique identifier
 *                   - this field is really a NULL terminated text string 
 *                   - in the form of a valid "o=" SDP description.
 *                   - See RFC 4456 for a description of the "o=" syntax.
 */
typedef char* vqec_sdp_handle_t;

/*----------------------------------------------------------------------------
 * SDP content block
 *--------------------------------------------------------------------------*/
    typedef char sdp_block_t;

/// @}

/**
 * The enum values defined in vqec_show_tuner_options_mask and used by 
 * vqec_set_options_flag() return info derived from the parsing of a 
 * "show tuner..." CLI command, including:
 * 
 * 1. the keyword that followed "show tuner", which is one of:
 *       a.) "all" (ALL_TUNERS_MASK) 
 *       b.) "name" (SPECIFIC_TUNER_MASK)
 *       c.) "join-delay" (JOINDELAY_MASK)
 * 2. in the case of a "all" or "name" keyword, the set of flags given
 *    to qualify the "show tuner" output.  The ALL_OPTIONS_MASK defines 
 *    the complete set of flags for this purpose.
 * 3. whether an error was encountered in parsing the keywords, flags, 
 *    or tuner names (ERROR_MASK or TUNER_NAME_ERROR_MASK)
 */
enum vqec_show_tuner_options_mask {
    ALL_TUNERS_MASK        = 0x00000001,
    SPECIFIC_TUNER_MASK    = 0x00000002,
    BRIEF_MASK             = 0x00000004,
    PCM_MASK               = 0x00000008,
    COUNTERS_MASK          = 0x00000010,
    CHANNEL_MASK           = 0x00000020,
    LOG_MASK               = 0x00000040,
    DETAIL_MASK            = 0x00000080,
    FEC_MASK               = 0x00000100,
    NAT_MASK               = 0x00000200,
    RCC_MASK               = 0x00000400,    
    IPC_MASK               = 0x00000800,    
    ALL_OPTIONS_MASK       = ((IPC_MASK << 1) - 1) & 
                             ~(ALL_TUNERS_MASK | SPECIFIC_TUNER_MASK),
    JOINDELAY_MASK         = 0x08000000,
    ERROR_MASK             = 0x10000000,
    TUNER_NAME_ERROR_MASK  = 0x30000000
};

enum vqec_show_tuner_cli_name {
    STCLIARG_NAME = 0,
    STCLIARG_ALL,
    STCLIARG_JOINDELAY
};

enum vqec_show_tuner_cli_opts {
    STCLIARG_BRIEF = 0,
    STCLIARG_PCM,
    STCLIARG_CHANNEL,
    STCLIARG_COUNTERS, 
    STCLIARG_LOG, 
    STCLIARG_FEC, 
    STCLIARG_NAT, 
    STCLIARG_RCC,
    STCLIARG_IPC, 
    STCLIARG_DETAIL 
};

#define SHOW_T_CLI_ARG_NAME "name"
#define SHOW_T_CLI_ARG_ALL "all"
#define SHOW_T_CLI_ARG_BRIEF "brief"
#define SHOW_T_CLI_ARG_PCM "pcm"
#define SHOW_T_CLI_ARG_CHANNEL "channel"
#define SHOW_T_CLI_ARG_COUNTERS "counters"
#define SHOW_T_CLI_ARG_LOG "log"
#define SHOW_T_CLI_ARG_FEC "fec"
#define SHOW_T_CLI_ARG_NAT "nat"
#define SHOW_T_CLI_ARG_RCC "rcc"
#define SHOW_T_CLI_ARG_IPC "ipc"
#define SHOW_T_CLI_ARG_DETAIL "detail"    
#define SHOW_T_CLI_ARG_JOINDELAY "join-delay"

enum vqec_show_channel_options_mask {
    SC_LIST_MASK                  = 0x00000001,
    SC_COUNTERS_MASK              = 0x00000002,
    SC_COUNTERS_ALL_MASK          = (0x00000004 | SC_COUNTERS_MASK),
    SC_COUNTERS_URL_MASK          = (0x00000008 | SC_COUNTERS_MASK),
    SC_COUNTERS_CUMULATIVE_MASK   = (0x00000010 | SC_COUNTERS_MASK),
    SC_CONFIG_MASK                = 0x00000020,
    SC_CONFIG_ALL_MASK            = (0x00000040 | SC_CONFIG_MASK),
    SC_CONFIG_URL_MASK            = (0x00000080 | SC_CONFIG_MASK),
    SC_ERROR_MASK                 = 0x00000100,
};

enum vqec_show_channel_cli_opts_params1 {
    SCCLIARG_COUNTERS = 0,
    SCCLIARG_CONFIG,
};

enum vqec_show_channel_cli_opts_params2 {
    SCCLIARG_ALL = 0,
    SCCLIARG_URL,
};

enum vqec_show_channel_cli_opts_params3 {
    SCCLIARG_CUMULATIVE = 0,
};

#define SHOW_C_CLI_ARG_LIST "list"
#define SHOW_C_CLI_ARG_COUNTERS "counters"
#define SHOW_C_CLI_ARG_CONFIG "config"
#define SHOW_C_CLI_ARG_ALL "all"
#define SHOW_C_CLI_ARG_URL "url"
#define SHOW_C_CLI_ARG_CUMULATIVE "cumulative"

#define VQEC_CLIENT_MODE_USER 0x0 /* must be 0 */
#define VQEC_CLIENT_MODE_KERN 0x1
extern int32_t g_vqec_client_mode;  

/*----------------------------------------------------------------------------
 * OS-specific Definitions
 *--------------------------------------------------------------------------*/
#include <netinet/ip.h>
typedef struct in_addr vqec_in_addr_t; /* network byte order */
typedef in_port_t vqec_in_port_t;      /* network byte order */

/*----------------------------------------------------------------------------
 * VQE-C Channel Configuration Structure 
 *--------------------------------------------------------------------------*/

#define VQEC_MAX_CHANNEL_NAME_LENGTH 80

typedef enum vqec_chan_type_ {
    VQEC_CHAN_TYPE_LINEAR = 0,
    VQEC_CHAN_TYPE_VOD
} vqec_chan_type_t;

typedef enum vqec_fec_mode_ {
    VQEC_FEC_1D_MODE,
    VQEC_FEC_2D_MODE
} vqec_fec_mode_t;

typedef enum vqec_fec_order_ {
    VQEC_FEC_SENDING_ORDER_NOT_DECIDED = 0,
    VQEC_FEC_SENDING_ORDER_ANNEXA,
    VQEC_FEC_SENDING_ORDER_ANNEXB,
    VQEC_FEC_SENDING_ORDER_OTHER
} vqec_fec_order_t;

/* unspecified bandwidth value */
#define VQEC_RTCP_BW_UNSPECIFIED 0xffffffff

/* values for loss rle and per loss rle */
/* represent maximum xr block size in bytes */
#define VQEC_RTCP_XR_RLE_ENABLE 0xfff2
#define VQEC_RTCP_XR_RLE_UNSPECIFIED 0xffff
 
/* bit masks for RTCP XR stat flags */
#define VQEC_RTCP_XR_STAT_LOSS  0x00000001
#define VQEC_RTCP_XR_STAT_DUP   0x00000002
#define VQEC_RTCP_XR_STAT_JITT  0x00000004

#define VQEC_MAX_ICE_UFRAG_LENGTH 257
/*
 * vqec_chan_cfg_t
 *
 * This type describes a VQE-C channel.
 *
 * Structure field names and associated comments are intended to document the
 * purpose of each field.
 *
 * NOTE:  Addr and ports within this structure MUST be in network byte order!
 */
typedef struct vqec_chan_cfg_ {

    /* Top Level Parameters */
    char                name[VQEC_MAX_CHANNEL_NAME_LENGTH];
    uint8_t             protocol;

    /* Primary Stream Destination Transport */
    vqec_in_addr_t      primary_dest_addr;          /* network byte order IP4*/
    vqec_in_port_t      primary_dest_port;          /* network byte order */
    vqec_in_port_t      primary_dest_rtcp_port;

    /* Primary Stream Source Transport */
    vqec_in_addr_t      primary_source_addr;        /* network byte order */
    vqec_in_port_t      primary_source_port;        /*
						     * unicast only
						     * (network byte order)
						     */
    vqec_in_port_t      primary_source_rtcp_port;   /*
						     * unicast only
						     * (network byte order)
						     */

    /* Primary Stream Parameters */
    uint8_t             primary_payload_type;       /* RTP payload type */
    uint32_t            primary_bit_rate;           /* bps */
    uint32_t            primary_rtcp_sndr_bw;       /* bps */
    uint32_t            primary_rtcp_rcvr_bw;       /* bps */
    uint32_t            primary_rtcp_per_rcvr_bw;   /* multicast only */

    /* Primary Extended Reports */
    uint16_t            primary_rtcp_xr_loss_rle;   /* size in bytes */
    uint16_t            primary_rtcp_xr_per_loss_rle; 
                                                    /* size in bytes */
    uint32_t            primary_rtcp_xr_stat_flags; /* bit mask flags */
    uint8_t             primary_rtcp_xr_multicast_acq;  
   
    /* Primary support for Reduced Size RTCP reports */
    uint8_t             primary_rtcp_rsize;
    uint8_t             primary_rtcp_xr_diagnostic_counters; 

    /* Primary Stream Feedback Target */
    vqec_in_addr_t      fbt_addr;                   /* network byte order */

    /* Retransmission Enable Flags */
    uint8_t             er_enable;
    uint8_t             rcc_enable;

    /* Retransmission Stream Source Transport */
    vqec_in_addr_t      rtx_source_addr;            /* network byte order */
    vqec_in_port_t      rtx_source_port;            /* network byte order */
    vqec_in_port_t      rtx_source_rtcp_port;       /* network byte order */

    /*
     * Retransmission Stream Destination Transport
     *
     * Note:  supplied values for rtx_dest_port and rtx_rtcp_dest_port are
     *        used only for unicast channels or when NAT mode is configured.
     *        Otherwise, values for rtx_source_port and rtx_source_rtcp_port
     *        will be also be used as local retransmission stream destination
     *        ports.
     */
    vqec_in_addr_t      rtx_dest_addr;              /* network byte order */
    vqec_in_port_t      rtx_dest_port;              /* network byte order */
    vqec_in_port_t      rtx_dest_rtcp_port;         /* network byte order */
    
    /* Retransmission Stream Parameters */
    uint8_t             rtx_payload_type;           /* RTP payload type */
    uint32_t            rtx_rtcp_sndr_bw;           /* bps */
    uint32_t            rtx_rtcp_rcvr_bw;           /* bps */
    
    /* Retransmission Extended Reports */
    uint16_t            rtx_rtcp_xr_loss_rle;       /* size in bytes */
    uint32_t            rtx_rtcp_xr_stat_flags;     /* bit mask flags */

    /* FEC Enable Flags */
    uint8_t             fec_enable;
    vqec_fec_mode_t     fec_mode;

    /* FEC 1 Transport */
    vqec_in_addr_t      fec1_mcast_addr;            /* network byte order */
    vqec_in_port_t      fec1_mcast_port;            /* network byte order */
    vqec_in_port_t      fec1_mcast_rtcp_port;       /* network byte order */
    vqec_in_addr_t      fec1_source_addr;           /* network byte order */

    /* FEC 1 Parameters */
    uint8_t             fec1_payload_type;          /* RTP payload type */
    uint32_t            fec1_rtcp_sndr_bw;          /* bps */
    uint32_t            fec1_rtcp_rcvr_bw;          /* bps */

    /* FEC 2 Transport */
    vqec_in_addr_t      fec2_mcast_addr;            /* network byte order */
    vqec_in_port_t      fec2_mcast_port;            /* network byte order */
    vqec_in_port_t      fec2_mcast_rtcp_port;       /* network byte order */
    vqec_in_addr_t      fec2_source_addr;           /* network byte order */

    /* FEC 2 Parameters */
    uint8_t             fec2_payload_type;          /* RTP payload type */
    uint32_t            fec2_rtcp_sndr_bw;          /* bps */
    uint32_t            fec2_rtcp_rcvr_bw;          /* bps */

    /* FEC Info Parameters */
    uint8_t             fec_l_value;            /* FEC block length */
    uint8_t             fec_d_value;            /* FEC block depth */
    vqec_fec_order_t    fec_order;          
    uint64_t            fec_ts_pkt_time;        /* RTP timestamps packet time*/ 

    /* Internally Used Parameters - No Need to Set */
    uint8_t             passthru;                       /* passthru channel */

    /* ICE ufrag for both client and server */
    char client_ufrag[VQEC_MAX_ICE_UFRAG_LENGTH];
    char server_ufrag[VQEC_MAX_ICE_UFRAG_LENGTH];
    uint8_t prim_nat_binding;  /* nat binding flag for primary session */
    uint8_t stun_optimization;  /* optimize stun signaling, 
                                   if true, no signaling after detecting 
                                   not behind nat */
} vqec_chan_cfg_t;

/*----------------------------------------------------------------------------
 * VQE-C System Configuration Delivery 
 *--------------------------------------------------------------------------*/

/*
 * Defines the types of configurations used by VQE-C which may be updated.
 */
typedef enum vqec_ifclient_config_ {
    VQEC_IFCLIENT_CONFIG_NETWORK,
    VQEC_IFCLIENT_CONFIG_OVERRIDE,
    VQEC_IFCLIENT_CONFIG_CHANNEL,
    VQEC_IFCLIENT_CONFIG_MAX         /* unused, terminates config list */
} vqec_ifclient_config_t;

/*
 * Defines the events of interest which may occur for a configuration
 * file.  The configuration delivery mechanism may register a callback
 * function to learn about these events.
 */
typedef enum vqec_ifclient_config_event_ {
    VQEC_IFCLIENT_CONFIG_EVENT_CONFIG_INVALID,
                           /*
                            * Previously delivered configuration is now 
                            * missing/corrupt.  Requires cfg_index_pathname
                            * to be configured.
                            */
} vqec_ifclient_config_event_t;

/* 
 * Defines the set of information passed back to the configuration delivery
 * mechanism when an event has occurred.
 */
typedef struct vqec_ifclient_config_event_params_ {
    vqec_ifclient_config_event_t      event;      /* event type */
    vqec_ifclient_config_t            config;     /* identifies config */
} vqec_ifclient_config_event_params_t;

/*
 * Defines a callback function type.
 *
 * @param[in] params  - information about the configuration event.
 *                      The point may ONLY be dereferenced during the
 *                      execution of this callback, and NOT after its return.
 */
typedef void (*vqec_ifclient_config_event_cb_t)(
                       vqec_ifclient_config_event_params_t *params);

/*
 * Defines the parameters passed by the configuration delivery mechanism
 * upon registering a callback function for a configuration type.
 */
typedef struct vqec_ifclient_config_register_params_ {
    vqec_ifclient_config_t           config;    /*
                                                 * Identifies configuration 
                                                 * for which event handler is 
                                                 * being registered.
                                                 */
    vqec_ifclient_config_event_cb_t  event_cb;  /*
                                                 * Event callback handler.
                                                 * Invoked by VQE-C upon an 
                                                 * event occurring for
                                                 * configurations of type
                                                 * specified in "config".
                                                 */
} vqec_ifclient_config_register_params_t;

typedef struct vqec_ifclient_config_update_params_ {
    vqec_ifclient_config_t config;    /* [in] config to be updated */
    char                   *buffer;   /* 
                                       * [in] buffer containing updated config,
                                       *      MUST be NULL-terminated
                                       */
    uint8_t                persisted; /*
                                       * [out] TRUE:  config was saved to file
                                       *       FALSE: config was not saved
                                       */
} vqec_ifclient_config_update_params_t;

/*
 * Indicators for the status of a persisted VQE-C configuration file.
 */
typedef enum vqec_ifclient_config_status_ {
    VQEC_IFCLIENT_CONFIG_STATUS_VALID,    /* config is valid */
    VQEC_IFCLIENT_CONFIG_STATUS_INVALID,  /* config is corrupt */
    VQEC_IFCLIENT_CONFIG_STATUS_NA,       /*
                                           * config is not configured to
                                           *  be persisted, or 
                                           * config's validity could not 
                                           *  be determined because index
                                           *  file not configured for use
                                           */
} vqec_ifclient_config_status_t;

/* Length of a VQE-C MD5 checksum string (32 hex chars plus '\0') */
#define VQEC_MD5_CHECKSUM_STRLEN  (32 + 1)
/* Length of a VQE-C timestamp in ISO-8601 format */
#define VQEC_TIMESTAMP_STRLEN (28 + 1)

/*
 * Defines parameters passed by the caller (and returned by VQE-C)
 * for retrieving status information about a VQE-C configuration
 */
typedef struct vqec_ifclient_config_status_params_ {
    vqec_ifclient_config_t        config;      
                                  /* [in]  identifies config being queried */
    uint8_t                       persistent;  
                                  /* [out] is persisted config present(T/F)? */
    vqec_ifclient_config_status_t status;
                                  /*
                                   * [out] validity of persisted config 
                                   *       (verification of md5 checksum)
                                   */
    char                          md5[VQEC_MD5_CHECKSUM_STRLEN];
                                  /*
                                   * [out] config's md5 checksum value,
                                   *       or "<unavailable>" if config is
                                   *       not persisted
                                   */ 
    char                          last_update_timestamp[VQEC_TIMESTAMP_STRLEN];
                                  /*
                                   * [out] date and time of last 
                                   *       configuration update
                                   */
} vqec_ifclient_config_status_params_t;

/*
 * Defines parameters passed by the caller for updating the
 * Override Configuration via (tag, value) pairs.
 */
typedef struct vqec_ifclient_config_override_params_ {
    const char  **tags;        /* [in]     array of config parameter names */
    const char  **values;      /* [in]     array of config parameter values */
} vqec_ifclient_config_override_params_t;

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __VQEC_IFCLIENT_DEFS_H__
