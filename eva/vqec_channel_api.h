/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2007-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: VQEC channel public interface.
 *
 * Documents:
 *
 *****************************************************************************/
#ifndef __VQEC_CHANNEL_API_H__
#define __VQEC_CHANNEL_API_H__

#include <cfg/cfgapi.h>
#include "vqec_ifclient_defs.h"
#include "vqec-dp/vqec_dp_api.h"
#include "vqec_nat_interface.h"

/**
 * @defgroup vqechan  VQEC control-plane Channel API.
 * @{
 */

/*
 * Channel IDs types/definitions
 */
typedef int32_t vqec_chanid_t;  /*!< Channel ID */
#define VQEC_CHANID_INVALID 0   /*!< Invalid ID for a channel */

/**
 * Channel module API error codes.
 */
typedef enum vqec_chan_err_
{
    VQEC_CHAN_ERR_OK,              /*!< Success  */
    VQEC_CHAN_ERR_INVALIDARGS,     /*!< Invalid arguments given to function */
    VQEC_CHAN_ERR_NOMORECHANNELS,  /*<! Channel limit reached */
    VQEC_CHAN_ERR_NOTINITIALIZED,  /*!< Channel module not initialized */
    VQEC_CHAN_ERR_NOSUCHCHAN,      /*!< Invalid channel ID supplied */
    VQEC_CHAN_ERR_NOTFOUND,        /*!< Channel not found */
    VQEC_CHAN_ERR_NOSUCHBINDING,   /*!< Channel not bound to given tuner */
    VQEC_CHAN_ERR_NOMEM,           /*!< Memory allocation failed */
    VQEC_CHAN_ERR_NOROOM,          /*!< Insufficient buffer size for info */
    VQEC_CHAN_ERR_INTERNAL,        /*!< Internal error */
} vqec_chan_err_t;

/**
 * Channel RTP session types.
 */
typedef 
enum vqec_chan_rtp_session_type_
{
    VQEC_CHAN_RTP_SESSION_PRIMARY,  /* primary session */
    VQEC_CHAN_RTP_SESSION_REPAIR,   /* repair session */
} vqec_chan_rtp_session_type_t;

/**---------------------------------------------------------------------------
 * Basic API.
 *---------------------------------------------------------------------------*/ 

/**
 * Returns a descriptive error string corresponding to an vqec_chan_err_t
 * error code.
 *
 * @param[in] err           Error code
 * @param[out] const char * Descriptive string corresponding to error code
 */ 
const char *
vqec_chan_err2str(vqec_chan_err_t err);

/**
 * Initialize the channel module, and allocate any static resources.
 *
 * @param[out] vqec_chan_err_t Retruns VQEC_CHAN_ERR_OK on success. If 
 * resources cannot be allocated returns an error code.
 */
vqec_chan_err_t
vqec_chan_module_init(void);

/**
 * Deallocate all resources allocated for the channel manager. All
 * channels that are currently cached are destroyed.  Also, the
 * flowgraphs for these channels are also destroyed in the dataplane.
 */
void
vqec_chan_module_deinit(void);

/**
 * Retrieves a channel manager ID for the supplied active channel,
 * as specified by its destination (ip addr, port) pair.  
 * If no active channel exists with the given (ip addr, port) pair,
 * then VQEC_ERR_CHANNOTACTIVE will be returned.
 *
 * @param[out] chanid   Retrieved channel's ID upon success, or
 *                      VQEC_CHANID_INVALID upon failure.
 * @param[in] ip        requested channel's destination IP address
 * @param[in] port      requested channel's destination port
 * @param[out] vqec_chan_err_t 
 *                      Returns VQEC_CHAN_ERR_OK upon success,
 *                      or a failure code indicating why a channel ID 
 *                      could not be retrieved. 
 */
vqec_chan_err_t
vqec_chan_find_chanid(vqec_chanid_t *chanid,
                      in_addr_t ip, uint16_t port);

/**
 * Converts a channel_cfg_t structure (based on SDP)
 * into a vqec_chan_cfg_t structure (internal VQE-C
 * representation of a channel configuration).
 *
 * @param[in]  sdp        sdp channel configuration
 * @param[in]  chan_type  channe type (linear or vod)
 * @param[out] cfg        target channel cfg
 */
void
vqec_chan_convert_sdp_to_cfg(channel_cfg_t *sdp,
                             vqec_chan_type_t chan_type,
                             vqec_chan_cfg_t *cfg);

/**
 * Retrieves a channel manager ID for the supplied channel.
 *
 * If the supplied channel exists in the channel manager database
 * (implying that a tuner is already associated with it), then its ID
 * will be returned.  If the supplied channel does not exist, then it
 * will be created and inserted into the channel database, and its ID
 * returned.
 *
 * Channels are uniquely identified by their destination IP and port
 * pair (cfg->primary_dest_addr.s_addr, cfg->primary_dest_port).
 * The other fields of the cfg structure are used only when the channel
 * does not already exist in the channel database.  Note that this
 * channel configuration is currently NOT validated within this
 * function--the supplied configuration is assumed to be valid.
 *
 * @param[out] chanid Retrieved channel's ID upon success, or
 * VQEC_CHANID_INVALID upon failure.
 * @param[in] cfg Channel configuration. A copy of this configuration
 * will be cached, and used to construct various dynamic components
 * of the channel.  The configuration is assumed valid.
 * @param[out] vqec_chan_err_t Returns VQEC_CHAN_ERR_OK upon success,
 * or a failure code indicating why a channel ID could not be retrieved. 
 */
vqec_chan_err_t
vqec_chan_get_chanid(vqec_chanid_t *chanid, vqec_chan_cfg_t *cfg);

/**
 * Updates the configuration of an active channel.  Only updates to fields
 * documented in the header for vqec_ifclient_chan_cfg_update() are updated;
 * other fields are silently ignored.
 *
 * @param[in] chanid  Identifies channel to update.
 * @param[in] cfg     Contains configuration updates
 * @param[out] vqec_chan_err_t VQEC_CHAN_ERR_OK upon success,
 * or a failure code indicating why the channel could not be updated.
 */
vqec_chan_err_t
vqec_chan_update_source(vqec_chanid_t chanid, const vqec_chan_cfg_t *cfg);

/**
 * Update the nat binding of a channel. This function close 
 * the previous nat binding of the channel and restart a 
 * new nat binding process. It is mainly use for VOD failover
 * support.
 * 
 * @param[in] chanid  Identifies channel to update.
 * @param[out] vqec_chan_err_t VQEC_CHAN_ERR_OK upon success,
 * or a failure code indicating why the channel could not be updated.
 */
vqec_chan_err_t 
vqec_chan_update_nat_binding(vqec_chanid_t chanid);

/**
 * Negates a vqec_chan_get_chanid call. 
 *
 * The effects of a vqec_chan_get_chanid call can be undone
 * by calling vqec_chan_put_chanid. If no tuners are bound to the
 * channel referenced by the supplied chanid, the channel is
 * destroyed. 
 *
 * For example, if a vqec_chan_get_chanid is called before binding
 * a tuner but the tuner bind call fails, this function may be used
 * to ensure that no memory is leaked.
 *
 * @param[in]  chanid  channel ID of an existing channel
 * @param[out] vqec_chan_err_t  Returns VQEC_CHAN_ERR_OK on success
 */
vqec_chan_err_t
vqec_chan_put_chanid(vqec_chanid_t chanid);

/**
 * Binds the given channel to a tuner.
 *
 * Note that channels may be bound to more than one tuner.
 *
 * Upon a successful binding, the following actions are taken:
 *   o The tuner ID is stored with the channel object
 *   o A flowgraph is built (if the channel is not already bound to a tuner)
 *     or augmented (if the channel is already bound) in the dataplane for 
 *     the channel
 *   o A PLI-NACK is sent to initiate RCC (if the channel is not already 
 *     bound to a tuner)
 *
 * @param[in] chanid Channel to be bound
 * @param[in] tid Tuner to which the channel should be bound (specified
 * by its dataplane tuner ID).
 * @param[in] bp Bind parameters
 * @param[in] chan_event_cb: chan event cb function
 * @param[out] vqec_chan_err_t Returns VQEC_CHAN_ERR_OK on success,
 * and an error code on failure.
 */
vqec_chan_err_t
vqec_chan_bind(vqec_chanid_t chanid,
               vqec_dp_tunerid_t tid,
               const vqec_bind_params_t *bp,
               const vqec_ifclient_chan_event_cb_t *chan_event_cb);

/**
 * Unbinds the given channel from the specified tuner.
 *
 * Upon a successful unbinding, the following actions are taken:
 *   o The tuner ID is removed from the channel object
 *   o The channel's flowgraph is modified to exclude the tuner (if other
 *     tuners remain bound to the channel) or destroyed (if no other tuners
 *     remain bound to the channel)
 *   o All its other control and dataplane resources are destroyed
 *
 * @param[in] chanid channel to be unbound
 * @param[in] tid tuner to be unbound from the given channel
 * @param[out] vqec_chan_err_t Returns VQEC_CHAN_ERR_OK if the unbind
 * was successful, and an error code on failure.
 */
vqec_chan_err_t
vqec_chan_unbind(vqec_chanid_t chanid, vqec_dp_tunerid_t tid);

/**
 * Get counters for all channels
 *
 * This API accepts a vqec_ifclient_stats_t structure, which is the
 * public data structure for exporting aggregated data across ALL
 * channels.
 *
 * @param[in] stats Pointer to the statistics structure.
 * @param[in] cumulative Indicates whether cumulative statistics are 
 *            desired.
 * @param[out] vqec_chan_err_t Indicates success or reason for failure.
 * 
 */
vqec_chan_err_t
vqec_chan_get_counters(vqec_ifclient_stats_t *stats, boolean cumulative);

/**
 * Get counters for an individual channel.
 *
 * This API accepts a vqec_ifclient_stats_channel_t structure, which is
 * the public data structure for exporting data on an individual channel.
 *
 * Note that
 *   1) Additional stats/fields may be added to vqec_ifclient_stats_channel_t
 *      in the future, including ones which match the names of those defined
 *      in vqec_ifclient_stats_t, as requirements for retrieving these stats 
 *      appear.
 *   2) The stats retrieved by this API start accumulating when the channel
 *      becomes active, and are NOT cleared by the "clear counters"
 *      command or vqec_ifclient_clear_stats() API since there is no current
 *      requirement for this behavior.
 * 
 * @param[in] chanid           ID of channel whose counters are requested
 * @param[in] cumulative Indicates whether cumulative statistics are 
 *            desired.
 * @param[out] stats           Pointer to the statistics structure.
 * @param[out] vqec_chan_err_t Indicates success or reason for failure.
 */
vqec_chan_err_t
vqec_chan_get_counters_channel(vqec_chanid_t chanid, 
                               vqec_ifclient_stats_channel_t *stats,
                               boolean cumulative);

/**
 * Get TR-135 sample counters for an individual channel.
 *
 * @param[in] chanid           ID of channel whose counters are requested
 * @param[out] stats           Pointer to the tr135 sample statistics structure.
 * @param[out] vqec_chan_err_t Indicates success or reason for failure.
 */
vqec_chan_err_t
vqec_chan_get_counters_channel_tr135_sample(
                        vqec_chanid_t chanid, 
                        vqec_ifclient_stats_channel_tr135_sample_t *stats);

/**
 * API to set TR-135 parameters for a channel
 *
 * @param[in] chanid ID of channel whose counters are requested
 * @param[in] params TR-135 writable parameters for this channel
 * @param[out] vqec_chan_err_t Indicates success or reason for failure.
 */
vqec_chan_err_t
vqec_chan_set_tr135_params (vqec_chanid_t chanid, 
                            vqec_ifclient_tr135_params_t *params);

/*
 * Get the tuner Q drops for all the tuners bound to a channel
 *
 * @param[in]  chanid              Channel identifier.
 * @param[out] tuner_queue_drops   Pointer to the uint64_t variable
 *                                that will be populated by this function
 * @param[in]  cumulative          Boolean specifying if cumulative stats
 *                                are desired
 * @returns vqec_chan_err_t
 */
vqec_chan_err_t vqec_chan_get_tuner_q_drops(vqec_chanid_t chanid,
                                            uint64_t *tuner_queue_drops,
                                            boolean cumulative);

/**
 * Retrieve a channel's input shim stream identifiers.
 *
 * @param[in]  chanid          - specifies channel of interest
 * @param[out] prim_rtp_id     - primary rtp stream ID
 * @param[out] repair_rtp_id   - repair rtp stream ID
 * @param[out] fec0_rtp_id     - fec0 rtp stream ID
 * @param[out] fec1_rtp_id     - fec1 rtp stream ID
 * @param[out] vqec_chan_err_t - VQEC_CHAN_ERR_OK:         success
 *                               VQEC_CHAN_ERR_NOSUCHCHAN: channel not found
 */
vqec_chan_err_t
vqec_chan_get_inputshim_streams(vqec_chanid_t chanid,
                                vqec_dp_streamid_t *prim_rtp_id,
                                vqec_dp_streamid_t *repair_rtp_id,
                                vqec_dp_streamid_t *fec0_rtp_id,
                                vqec_dp_streamid_t *fec1_rtp_id);

/*
 * Retrieve a channel's output shim stream identifier.
 *
 * @param[in]  chanid          - specifies channel of interest
 * @param[out] postrepair_id   - post-repair stream ID
 * @param[out] vqec_chan_err_t - VQEC_CHAN_ERR_OK:         success
 *                               VQEC_CHAN_ERR_NOSUCHCHAN: channel not found
 */
vqec_chan_err_t
vqec_chan_get_outputshim_stream(vqec_chanid_t chanid,
                                vqec_dp_streamid_t *postrepair_id);

/**
 * Clear the counters associated with a channel.
 *
 * @param[out] vqec_chan_err_t Indicates success or reason for failure.
 */
vqec_chan_err_t
vqec_chan_clear_counters(void);

/**
 * Display the state of a channel (equivalent to src_dump sans
 * the display of channel configuration from the configuration
 * manager) to the given file stream.
 *
 * @param[in] chanid Identifier of the channel.
 * @param[in] options_flag Which attributes of the channel are to be
 * displayed.
 */
void
vqec_chan_display_state(vqec_chanid_t chanid, uint32_t options_flag);

/**
 * Function:    vqec_channel_cfg_printf()
 * Description: Print out the channel configuration data
 * Parameters:  channel cfg to print
 * Returns:     void
 */
void vqec_channel_cfg_printf(channel_cfg_t *channel_p);


/**---------------------------------------------------------------------------
 * NAT-related API.
 *---------------------------------------------------------------------------*/ 

/**
 * Interface to deliver a NAT binding update for a binding the channel has
 * currently open.
 *
 * @param[in] chanid Identifier of the channel.
 * @param[in] bindid Identifier of the binding which is being updated.
 * @param[in] data Attributes that describe the current state of the binding.
 */
void
vqec_chan_nat_bind_update(vqec_chanid_t chanid, vqec_nat_bindid_t bindid, 
                          vqec_nat_bind_data_t *data);

/**
 * Inject a NAT packet through a socket which is owned by the channel.
 *
 * @param[in]chanid Identifier of the channel.
 * @param[in] bindid Identifier of the binding which is being updated.
 * @param[in] desc Description of the binding associated with the identifier.
 * @param[in] buf NAT packet contents.
 * @param[in] len Length of the NAT packet.
 * @param[out] boolean Returns true if the packet is successfully en-queued
 * to the socket or to an intermediate IPC queue, false otherwise.
 */
boolean 
vqec_chan_nat_inject_pak(vqec_chanid_t chanid, vqec_nat_bindid_t bindid, 
                         vqec_nat_bind_desc_t *desc, char *buf, uint16_t len);

/**
 * Handle NAT packets that are received on a RTCP session. 
 *
 * @param[in] chanid Identifier of the channel.
 * @param[in] buf Pointer the NAT packet.
 * @param[in] len Length of the NAT packet.
 * @param[in] session_t Type of session on which the packet is received.
 * @param[in] source_ip: source ip of the packet (ip of the pak sender) 
 * @param[in] source_port: source port of the packet
 * [only possible / allowed for repair session in current implementation].
 * @param[out] boolean Returns true if the packet was successfully sent
 * to the NAT protocol, false otherwise.
 */
boolean
vqec_chan_eject_rtcp_nat_pak(vqec_chanid_t chanid, 
                             char *buf, uint16_t len, 
                             vqec_chan_rtp_session_type_t session_t,
                             in_addr_t source_ip, 
                             in_port_t source_port);

/**
 * Handle NAT packets that are received on a RTP session from the dataplane.
 *
 * @param[in] chanid Identifier of the channel.
 * @param[in] dp_chanid Identifier of the dataplane channel.
 * @param[in] buf Pointer the NAT packet.
 * @param[in] len Length of the NAT packet.
 * @param[in] session_t Type of session on which the packet is received.
 * @param[in] source_ip: source ip of the packet (ip of the pak sender) 
 * @param[in] source_port: source port of the packet
 * [only possible / allowed for repair session in current implementation].
 * @param[out] boolean Returns true if the packet was successfully sent
 * to the NAT protocol, false otherwise.
 */
boolean
vqec_chan_eject_rtp_nat_pak(vqec_chanid_t chanid, 
                            vqec_dp_chanid_t dp_chanid,
                            char *buf, uint16_t len, 
                            vqec_dp_streamid_t is_id,
                            in_addr_t source_ip, in_port_t source_port);

/**
 * Display all NAT bindings that are open for the channel.
 *
 * @param[in] chanid Identifier of the channel.
 */
void
vqec_chan_show_nat_bindings(vqec_chanid_t chanid);


/**---------------------------------------------------------------------------
 * RCC-related API.
 *---------------------------------------------------------------------------*/ 
#ifdef HAVE_FCC

/**
 * Get the PAT information associated with a channel: the data is available
 * only after the APP packet has been received for a RCC burst.
 *
 * @param[in] chanid Identifier of the channel.
 * @param[out] pat_buf Buffer in which PAT is copied.
 * @param[out] pat_len Content length of PAT data.
 * @param[in] buf_len Length of pat_buf buffer.
 * @param[out] vqec_chan_err_t Indicates success or reason for failure.
 */
vqec_chan_err_t
vqec_chan_get_pat(vqec_chanid_t chanid,
                  uint8_t *pat_buf,
                  uint32_t *pat_len,
                  uint32_t buf_len);

/**
 * Get the PMT information associated with a channel: the data is available
 * only after the APP packet has been received for a RCC burst.
 *
 * @param[in] chanid Identifier of the channel.
 * @params[out] pmt_pid PID of the PMT.
 * @param[out] pmt_buf Buffer in which PMT is copied.
 * @param[out] pmt_len Content length of PMT data.
 * @param[in] buf_len Length of pmt_buf buffer.
 * @param[out] vqec_chan_err_t Indicates success or reason for failure.
*/
vqec_chan_err_t
vqec_chan_get_pmt(vqec_chanid_t chanid,
                  uint16_t *pmt_pid,
                  uint8_t *pmt_buf,
                  uint32_t *pmt_len,
                  uint32_t buf_len);


/**
 * Display the RCC state of a channel.
 *
 * @param[in] chanid Identifier of the channel.
 * @param[in] brief_flag If brief output mode is desired.
 */

void
vqec_chan_display_rcc_state(vqec_chanid_t chanid, boolean brief_flag);

#endif   /* HAVE_FCC */

/**---------------------------------------------------------------------------
 * Get dataplane channel id. Used for CLI access only - *do not* use
 * for other reasons.
 *
 * @param[in] chanid Identifier of the channel.
 * @param[out] vqec_dp_chanid_t Dataplane channel id. 
 *---------------------------------------------------------------------------*/ 
vqec_dp_chanid_t
vqec_chan_id_to_dpchanid(vqec_chanid_t chanid);

/**---------------------------------------------------------------------------
 * Has fastfill been requested for this channel from the server. 
 *
 * @param[in] chanid Identifier of the channel.
 * @param[out] uint8_t Returns TRUE if fastfill is enabled, FALSE otherwise.
 * 
 * The return value of this function is uint8_t instead of boolean. The reason 
 * is that this code will be used by third party code, such as STB code, which 
 * may not have boolean defined to be the same as it is in VQE-C.
 *---------------------------------------------------------------------------*/ 
uint8_t 
vqec_chan_is_fastfill_enabled(vqec_chanid_t chanid);


/**---------------------------------------------------------------------------
 * Is RCC enabled for this channel? 
 *
 * @param[in] chanid Identifier of the channel.
 * @param[out] uint8_t Returns TRUE if rcc is enabled, FALSE otherwise.
 * 
 * The return value of this function is uint8_t instead of boolean. The reason 
 * is that this code will be used by third party code, such as STB code, which 
 * may not have boolean defined to be the same as it is in VQE-C.
 *---------------------------------------------------------------------------*/ 
uint8_t 
vqec_chan_is_rcc_enabled(vqec_chanid_t chanid);


/**
 * Get the max observed FEC block size.
 *
 * @param[out] uint16_t the max observed block size.
 */
uint16_t vqec_chan_get_max_fec_block_size_observed(void);

/**
 *@}
 */

#endif /* __VQEC_CHANNEL_API_H__ */
