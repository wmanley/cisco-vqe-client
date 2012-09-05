/**-----------------------------------------------------------------
 * Vqec. Client Interface API.
 *
 *
 * Copyright (c) 2007-2010 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __VQEC_IFCLIENT_H__
#define __VQEC_IFCLIENT_H__

#include "vqec_ifclient_defs.h"
#include "vqec_error.h"
#include "vqec_ifclient_read.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define VQEC_PUBLIC extern
#define VQEC_SYNCHRONIZED

/**---------------------------------------------------------------------------
 * Get the string representation corresponding to a VQEC error code.
 *----------------------------------------------------------------------------
 */
const char *vqec_err2str(vqec_error_t err);

/**
 * Maximum string length of an RCC abort string representation
 */
#define VQEC_IFCLIENT_TUNER_RCCABORT_STR_MAX  40
/**---------------------------------------------------------------------------
 * Get the string representation corresponding to a VQEC RCC abort code.
 *
 * @param[out]  str         string into which RCC abort status is written
 * @param[in]   len         length of "str"
 * @param[in]   abort_code  RCC abort code
 * @param[out]  char *      "str" upon success, or NULL if failure
 *----------------------------------------------------------------------------
 */
const char *
vqec_ifclient_tuner_rccabort2str (char *str,
                                  uint32_t len,
                                  vqec_ifclient_rcc_abort_t abort_code);

/**---------------------------------------------------------------------------
 * All VQEC_SYNCHRONIZED public methods acquire and release a library global lock,  
 * and simply wrap the "inner" private methods. Before any blocking
 * operation the lock is released, and then reacquired after return.
 *----------------------------------------------------------------------------
 */

/**---------------------------------------------------------------------------
 * Create a tuner object. A tuner object can then "tune" or "listen"
 * to a particular video input stream, referred to as a "channel". The
 * maximum number of tuner objects that can exist in a system is a 
 * global configuration parameter. Tuner id's are opaque values. 
 *
 * A string identifier which is unique across all existing tuner objects will
 * serve as the name of the tuner.
 *
 * @param[out]  id The opaque integral identifier for the tuner object 
 * created by invoking the method is placed at *id. If errors are
 * encountered the identifier is set to VQEC_TUNERID_INVALID.
 * @param[in]  namestr String representing the tuner's name. The length of
 * namestr will be truncated to VQEC_MAX_TUNER_NAMESTR_LEN. If namestr is an
 * empty string, or another tuner with the given name exists, the call will
 * fail. If namestr is NULL, the lowest unused tuner index "i" will be used
 * to create a new tuner - the name of the tuner is then the index "i". If
 * the maximum configured tuner limit is reached the call will fail.
 * @param[out] vqec_err_t If tuner creation is successful, returns
 * VQEC_OK. The following failures may occur: 
 *
 * 	<I>VQEC_ERR_EXISTTUNER</I><BR>
 *	<I>VQEC_ERR_MAXLIMITTUNER</I><BR>
 *	<I>VQEC_ERR_CREATETUNER</I> <BR>
 *     <I>VQEC_ERR_INVALIDARGS</I><BR>
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED   
vqec_error_t vqec_ifclient_tuner_create(vqec_tunerid_t *id, 
                                        const char *namestr);

/**---------------------------------------------------------------------------
 * Delete an existing tuner object. Any channel currently open on the 
 * tuner is closed. 
 *
 * @param[in]  id Existing tuner object's identifier.
 * @param[out] vqec_err_t Returns VQEC_OK on success. Otherwise,
 * the following failures may occur:
 *
 *     <I>VQEC_ERR_NOSUCHTUNER</I><BR>
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED 
vqec_error_t vqec_ifclient_tuner_destroy(const vqec_tunerid_t id);

/**---------------------------------------------------------------------------
 * Create a bind parameters handle. This struct handle is necessary for
 * calls to bind a tuner and channel. The actual parameters may be
 * set through the vqec_ifclient_bind_params_set_... functions.
 * 
 * @param[out] vqec_bind_params_t* the created bind paramters handle 
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
vqec_bind_params_t* vqec_ifclient_bind_params_create(void);

/**---------------------------------------------------------------------------
 * Destroy a bind parameters handle. Should only be called once a bind
 * has completed.
 *
 * @param[in]  bp bind parameters handle to be destroyed
 * @param[out] uint8_t TRUE for success, FALSE for failure
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
uint8_t vqec_ifclient_bind_params_destroy(vqec_bind_params_t *bp);

/**---------------------------------------------------------------------------
 * Set the context/tuner id in the bind parameters.
 *
 * @param[in]  bp bind parameters handle
 * @param[in]  context_id the id to associated with this bind
 * @param[out] uint8_t TRUE for success, FALSE for failure
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
uint8_t vqec_ifclient_bind_params_set_context_id(vqec_bind_params_t *bp,
                                                int32_t context_id);
/**---------------------------------------------------------------------------
 * Get the context/tuner id in the bind parameters.
 *
 * @param[in]  bp bind parameters handle
 * @param[out] int the context id associated with the bind
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
int32_t vqec_ifclient_bind_params_get_context_id(const vqec_bind_params_t *bp);

/**---------------------------------------------------------------------------
 * Enable or Disable Rapid Channel Change in bind parameters used when
 * binding a tuner to a channel.
 *
 * @param[in]  bp bind parameters structure handle from create function.
 * @param[out] uint8_t TRUE for success, FALSE for failure
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
uint8_t vqec_ifclient_bind_params_enable_rcc(vqec_bind_params_t *bp);
VQEC_PUBLIC
uint8_t vqec_ifclient_bind_params_disable_rcc(vqec_bind_params_t *bp);

/**---------------------------------------------------------------------------
 * Retrieve value of RCC enable from bind parameters handle.
 *
 * @param[in]  bp bind parameters structure handle
 * @param[out] uint8_t TRUE for RCC enabled, FALSE otherwise
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
uint8_t vqec_ifclient_bind_params_is_rcc_enabled(const vqec_bind_params_t *bp);

/**---------------------------------------------------------------------------
 * Enable or disable fastfill in a bind parameters structure used when binding 
 * a tuner to a channel.
 *
 * @param[in]  bp bind parameters structure handle from create function.
 * @param[out] uint8_t TRUE for success, FALSE for failure
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
uint8_t vqec_ifclient_bind_params_enable_fastfill(vqec_bind_params_t *bp);
VQEC_PUBLIC
uint8_t vqec_ifclient_bind_params_disable_fastfill(vqec_bind_params_t *bp);

/**---------------------------------------------------------------------------
 * Retrieve value of fastfill enable from a bind parameters handle.
 *
 * @param[in]  bp bind parameters structure handle
 * @param[out] uint8_t TRUE for fast fill enabled, FALSE otherwise
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC uint8_t
vqec_ifclient_bind_params_is_fastfill_enabled(const vqec_bind_params_t *bp);

/**---------------------------------------------------------------------------
 * Set the function pointer which vqec will use to retrieve stats 
 * information from the decoder.
 *
 * @param[in] bp bind parameters structure handle
 * @param[in] dcr_info_ops function pointer for retrieving stats
 * @param[out] uint8_t TRUE if success, FALSE if failure
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
uint8_t vqec_ifclient_bind_params_set_dcr_info_ops(
                        vqec_bind_params_t *bp,
                        vqec_ifclient_get_dcr_info_ops_t *dcr_info_ops);

/**---------------------------------------------------------------------------
 * Get the function pointer set for retrieving decoder info.
 *
 * @param[in] bp bind parameters structure handle
 * @param[out] vqec_ifclient_get_dcr_info_ops_t* the function pointer
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
vqec_ifclient_get_dcr_info_ops_t *
vqec_ifclient_bind_params_get_dcr_info_ops(const vqec_bind_params_t *bp);


/**---------------------------------------------------------------------------
 * Fastfill operation function vector modifier.
 *
 * @param[in] bp bind parameters structure handle
 * @param[in] fastfill_ops Fastfill function vector.
 * @param[out] uint8_t TRUE if success, FALSE if failure
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
uint8_t vqec_ifclient_bind_params_set_fastfill_ops(
                        vqec_bind_params_t *bp,
                        vqec_ifclient_fastfill_cb_ops_t *fastfill_ops);

/**---------------------------------------------------------------------------
 * Fastfill operation function vector accessor.
 *
 * @param[in] bp bind parameters structure handle
 * @param[out] vqec_ifclient_fastfill_cb_ops_t* the vector pointer
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
vqec_ifclient_fastfill_cb_ops_t *
vqec_ifclient_bind_params_get_fastfill_ops(const vqec_bind_params_t *bp);

/**---------------------------------------------------------------------------
 * Maximum fast fill setter.
 *
 * @param[in] bp bind parameters structure handle
 * @param[in] max_fastfill new value to set
 * @param[out] uint8_t TRUE if success, FALSE if failure
 *----------------------------------------------------------------------------
 */
uint8_t
vqec_ifclient_bind_params_set_max_fastfill(vqec_bind_params_t *bp,
                                           const uint32_t max_fastfill);

/**---------------------------------------------------------------------------
 * Maximum fast fill accessor.
 *
 * @param[in] bp bind parameters structure handle
 * @param[out] uint32_t the value of max_fastfill
 *----------------------------------------------------------------------------
 */
uint32_t
vqec_ifclient_bind_params_get_max_fastfill(const vqec_bind_params_t *bp);

/**---------------------------------------------------------------------------
 * Maximum receive bandwidth (RCC) setter.
 *
 * @param[in] bp bind parameters structure handle
 * @param[in] max_recv_bw_rcc new value to set
 * @param[out] uint8_t TRUE if success, FALSE if failure
 *----------------------------------------------------------------------------
 */
uint8_t
vqec_ifclient_bind_params_set_max_recv_bw_rcc(vqec_bind_params_t *bp,
                                              const uint32_t max_recv_bw_rcc);

/**---------------------------------------------------------------------------
 * Maximum receive bandwidth (RCC) accessor.
 *
 * @param[in] bp bind parameters structure handle
 * @param[out] uint32_t the value of max_recv_bw_rcc
 *----------------------------------------------------------------------------
 */
uint32_t
vqec_ifclient_bind_params_get_max_recv_bw_rcc(const vqec_bind_params_t *bp);

/**---------------------------------------------------------------------------
 * Maximum receive bandwidth (ER) setter.
 *
 * @param[in] bp bind parameters structure handle
 * @param[in] max_recv_bw_er new value to set
 * @param[out] uint8_t TRUE if success, FALSE if failure
 *----------------------------------------------------------------------------
 */
uint8_t
vqec_ifclient_bind_params_set_max_recv_bw_er(vqec_bind_params_t *bp,
                                             const uint32_t max_recv_bw_er);

/**---------------------------------------------------------------------------
 * Maximum receive bandwidth (ER) accessor.
 *
 * @param[in] bp bind parameters structure handle
 * @param[out] uint32_t the value of max_recv_bw_er
 *----------------------------------------------------------------------------
 */
uint32_t
vqec_ifclient_bind_params_get_max_recv_bw_er(const vqec_bind_params_t *bp);

/**---------------------------------------------------------------------------
 * Maximum receive bandwidth transition callback setter.
 *
 * @param[in] bp bind parameters structure handle
 * @param[in] transition_cb function pointer to callback function
 * @param[out] uint8_t TRUE if success, FALSE if failure
 *----------------------------------------------------------------------------
 */
uint8_t
vqec_ifclient_bind_params_set_mrb_transition_cb(vqec_bind_params_t *bp,
                                                void *transition_cb);

/**---------------------------------------------------------------------------
 * Maximum receive bandwidth transition callback accessor.
 *
 * @param[in] bp bind parameters structure handle
 * @param[out] void function pointer to callback function
 *----------------------------------------------------------------------------
 */
void *
vqec_ifclient_bind_params_get_mrb_transition_cb(const vqec_bind_params_t *bp);

/**---------------------------------------------------------------------------
 * STB IR keypress time setter.
 *
 * @param[in] bp bind parameters structure handle
 * @param[in] ir_time timestamp the IR keypress was registered
 * @param[out] uint8_t TRUE if success, FALSE if failure
 *----------------------------------------------------------------------------
 */
uint8_t
vqec_ifclient_bind_params_set_ir_time(vqec_bind_params_t *bp,
                                      uint64_t ir_time);
/**---------------------------------------------------------------------------
 * STB IR keypress time accessor.
 *
 * @param[in] bp bind parameters structure handle
 * @param[out] uint64_t timestamp the IR keypress was registered
 * @param[out] boolean TRUE if the user set the ir_time
 *----------------------------------------------------------------------------
 */
uint64_t
vqec_ifclient_bind_params_get_ir_time(const vqec_bind_params_t *bp,
                                      uint8_t *ir_time_valid);

/**---------------------------------------------------------------------------
 * Open or "bind" a channel to an existing tuner.
 *
 * @param[in]  id Existing tuner object's identifier.
 * @param[in]  sdp_handle handle to an sdp description of a channel.
 *             Note the sdp_handle is really the "o = " line of a config.
 * @param[in]  bp Bind parameters to apply when binding the channel.
 * @param[out] vqec_err_t Returns VQEC_OK on success. Otherwise, the
 * following failures may occur:
 *
 *     <I>VQEC_ERR_NOSUCHTUNER</I><BR>
 *     <I>VQEC_ERR_CHANNELLOOKUP</I><BR>
 *     <I>VQEC_ERR_SESSIONCREATE</I><BR>
 *     <I>VQEC_ERR_MALLOC</I><BR>
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t vqec_ifclient_tuner_bind_chan(const vqec_tunerid_t id,  
                                           const vqec_sdp_handle_t sdp_handle,
                                           const vqec_bind_params_t *bp);

/**---------------------------------------------------------------------------
 * Open or "bind" a channel (specified by chan cfg) to an existing tuner.
 *
 * @param[in]  id  Existing tuner object's identifier.
 * @param[in]  cfg Pointer to a vqec_chan_cfg_t struct which describes
 *                 the channel to be tuned.
 * @param[in]  bp  Parameters for this particular bind
 * @param[out] vqec_err_t Returns VQEC_OK on success. Otherwise, the
 * following failures may occur:
 *
 *     <I>VQEC_ERR_NOSUCHTUNER</I><BR>
 *     <I>VQEC_ERR_SESSIONCREATE</I><BR>
 *     <I>VQEC_ERR_MALLOC</I><BR>
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t vqec_ifclient_tuner_bind_chan_cfg(const vqec_tunerid_t id,  
                                               vqec_chan_cfg_t *cfg,
                                               const vqec_bind_params_t *bp);

/**---------------------------------------------------------------------------
 * Updates the source-related configuration of an active channel.  The
 * update persists as long as the channel remains active.  All tuners 
 * bound to this active channel will use the updated configuration.
 *
 * The channel to update is identified by the (primary_dest_addr, 
 * primary_dest_port) fields within "cfg".  Its primary_dest_addr
 * MUST be unicast.
 *
 * Only the source-related fields of the "cfg" structure are relevant, 
 * and the rest are ignored (not updated).   Fields related to the repair
 * stream are only updated if error repair is enabled on the channel.
 *
 * Relevant fields are listed below:
 *  - primary_dest_addr
 *  - primary_dest_port
 *  - primary_source_addr
 *  - primary_source_port
 *  - fbt_addr
 *  - primary_source_rtcp_port
 *  - rtx_source_addr
 *  - rtx_source_port
 *  - rtx_source_rtcp_port
 *
 * @param[in] cfg            - identifies active channel to update
 *                             via (primary_dest_addr, primary_dest_port) 
 *                             fields, and supplies updated channel values
 *                             (see above)
 * @param[in] up             - update parameters (not used,
 *                             NULL should be passed)
 * @param[out] vqec_error_t  - result of update operation:
 *   VQEC_ERR_CHANNOTACTIVE       supplied channel not currently active
 *   VQEC_ERR_INVALIDARGS         invalid channel parameters
 *   VQEC_ERR_INTERNAL            internal error, see logs
 *   VQEC_OK                      update succeeded
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t vqec_ifclient_chan_update(const vqec_chan_cfg_t *cfg,
                                       const vqec_update_params_t *up);

/**---------------------------------------------------------------------------
 *
 * Update the nat binding of a channel. This function close 
 * the previous nat binding of the channel and restart a 
 * new nat binding process. It is mainly used for VOD failover
 * support.
 * 
 * @param[in] chanid  Identifies channel to update.
 * @param[out] vqec_error_t  - result of update operation:
 *   VQEC_ERR_CHANNOTACTIVE       supplied channel not currently active
 *   VQEC_ERR_INVALIDARGS         invalid channel parameters
 *   VQEC_ERR_INTERNAL            internal error, see logs
 *   VQEC_OK                      update succeeded
 *----------------------------------------------------------------------------
 */

VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t vqec_ifclient_nat_binding_update(in_addr_t prim_dest_ip, 
                                              in_port_t prim_dest_port);


/**---------------------------------------------------------------------------
 * Close or unbind any channel that is currently bound to a tuner.     
 * @param[in]  id Existing tuner object's identifier.
 * @param[out] vqec_err_t Returns VQEC_OK on success. Otherwise, the
 * following errors may occur:
 *
 *     <I>VQEC_ERR_NOSUCHTUNER</I><BR>
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t vqec_ifclient_tuner_unbind_chan(const vqec_tunerid_t id);

/**---------------------------------------------------------------------------
 * Obtain an sdp_handle based on the url.  The address and port
 * contained in the URL must describe the original stream address and port.
 * The protocols RTP and UDP
 * must be valid identifiers in all implementations of this    
 * method, with the following BNF grammar: 
 *
 *		url: rtpurl | udpurl <BR>
 *		rtpurl: rtp://IpAddress:Port <BR>
 * 		udpurl: udp://IpAddress:Port <BR>
 *		IpAddress: digits.digits.digits.digits <BR>
 *		Port: digits <BR>
 *		digits: digit [digits] <BR>
 *		digit: 0 1 2 3 4 5 6 7 8 9<BR>
 *
 * Only RTP and UDP url types are allowed. However, the implementation
 * currently ignores the RTP/UDP designation. The channel handle will
 * be identified even if the protocol field does not properly match
 * the protocol of the original stream source.
 *
 * Returns a valid sdp_handle or NULL.  Note the caller
 *           is responsible for freeing sdp handles.
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED vqec_sdp_handle_t
vqec_ifclient_alloc_sdp_handle_from_url(char *url);

/**---------------------------------------------------------------------------
 * Free an sdp_handle 
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED void
vqec_ifclient_free_sdp_handle(vqec_sdp_handle_t sdp_handle);

/**---------------------------------------------------------------------------
 * Parses an SDP from the buffer argument into individual parameters and
 * fills the provided vqec_chan_cfg_t structure accordingly. The parser
 * validation type is determined by the vqec_chan_type_t chan_type argument.
 * Calling with VQEC_CHAN_TYPE_LINEAR as the chan_type will cause the SDP
 * to be validated as a linear channel (with up to five section expected:
 * original, re-sourced, retransmission, fec1, fec2). Calling with 
 * VQEC_CHAN_TYPE_VOD as the chan_type will cause the SDP to be validated as
 * a VoD channel (with up to two sections expected: primary, retransmission).
 *
 * @param[in]  cfg       channel cfg structure to be filled
 * @param[in]  buffer    character buffer containing SDP
 * @param[in]  chan_type VQEC_CHAN_TYPE_LINEAR or VQEC_CHAN_TYPE_VOD
 * @param[out] uint8_t   TRUE (non-zero) if success, FALSE otherwise
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED 
uint8_t vqec_ifclient_chan_cfg_parse_sdp(vqec_chan_cfg_t *cfg,
                                         char *buffer,
                                         vqec_chan_type_t chan_type);

/**---------------------------------------------------------------------------
 * [Thread-safe]
 * Initialize an iterator which can iterate over all tuners. The current
 * implementation of this iterator is quite modest. It is assumed that 
 * a linear-map of tuners is maintained such that the position of tuners
 * in that map does not change, once they are allocated. The initializer 
 * sets two indices in the structure: a beginning position, which corresponds
 * to some "first" tuner, and an end position which corresponds to some
 * "last" tuner. The meaning of these positions is opaque to a user, and it 
 * suggested that users do not use them directly. Instead they should
 * use the FOR_ALL macro to iterate over all of the tuners.
 * 
 * If additional tuners are added while the walk is in progress, they
 * will be returned in the walk if the addition completed before the
 * walk reaches the tuner's position in the linear map. The FOR_ALL
 * macro initializes the iterator before use. 
 *
 * Example: <BR>
 * vqec_tuner_iter_t m_iter; <BR>
 * vqec_tuner_id_t m_id; <BR>
 * FOR_ALL_TUNERS_IN_LIST(m_iter, m_id) { <BR>
 * ... printf("%s", vqec_ifclient_tuner_get_name_by_id(m_id, ..)); <BR>
 * } <BR>
 * ** - Do not acquire the global lock when using the loop! **
 * 
 * @param[in]	iter Iterator structure which will be initialized.
 *---------------------------------------------------------------------------- 
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_tunerid_t vqec_ifclient_tuner_iter_init_first(vqec_tuner_iter_t *iter);
    
#define FOR_ALL_TUNERS_IN_LIST(iter, id)                                \
    for ((id) = vqec_ifclient_tuner_iter_init_first(&(iter));           \
         (iter).i_cur != VQEC_TUNERID_INVALID;                          \
         (id) = (iter).getnext(&(iter)))

/*----------------------------------------------------------------------------
 * Validate and decapsulate an arbitrary RTP packet.
 *
 * Note:  all fields in returned rtp_hdr are in host byte-order.
 *
 * @param[in]   buf     Pointer to the buffer containing the RTP packet
 * @param[in]   len     Length of the input RTP packet buffer
 * @param[out]  rtp_hdr On return, contains the RTP header fields of the
 *                      given and validated RTP packet.
 * @param[out]  vqec_err_t Returns VQEC_OK on success.  Otherwise, the
 *              following failures may occur:
 *
 *                   <I>VQEC_ERR_BADRTPHDR</I><BR>
 *--------------------------------------------------------------------------*/
VQEC_PUBLIC
vqec_error_t vqec_ifclient_rtp_hdr_parse (char *buf,
                                          uint16_t len,
                                          vqec_rtphdr_t *rtp_hdr);
                         
/*----------------------------------------------------------------------------
 * Trigger a VQE-C updater update.  This request will be handled
 * asynchronously by the updater thread, which will wait
 * a random time interval bound by "update_window" in the system config, and
 * then update all necessary components.
 *
 *--------------------------------------------------------------------------*/
VQEC_PUBLIC
void vqec_ifclient_trigger_update(void);

/**---------------------------------------------------------------------------
 * Initialize the client interface.  Initialization may block while
 * updates to network and channel configuration are requested over
 * the network.  VQE-C initialization requires that network services
 * already be initialized (such that VQE-C may retrieve MAC addresses 
 * associated with its interfaces and communicate over network sockets).
 *
 * @param[in]	filename VQEC's configuration file. This file may be NULL,
 * in which case default configuration parameters shall be used.
 * @param[out] vqec_err_t Returns VQEC_OK on success. On failure, the 
 * following error codes are returned:
 *
 * When VQEC services are no longer needed, the vqec_ifclient_deinit() API
 * may be invoked in order to free allocated resources.
 *
 * NOTE:  If VQE-C services have been initialized with this function and
 *        the caller wishes to re-initialize VQE-C with a new configuration
 *        file (or if the configuration file's contents have changed),
 *        then vqec_ifclient_deinit() MUST be called prior to calling
 *        vqec_ifclient_init() again. 
 *
 *     <I>VQEC_ERR_MALLOC</I><BR>
 *     <I>VQEC_ERR_SYSCALL</I><BR>
 *     <I>VQEC_ERR_CONFIG_NOT_FOUND</I><BR>
 *     <I>VQEC_ERR_PARAMRANGEINVALID</I><BR>
 *----------------------------------------------------------------------------  
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t vqec_ifclient_init(const char *filename);

/**---------------------------------------------------------------------------
 * De-initialize the client interface. All previously resources allocated
 * within VQEC are freed. Any other control/data thread activity should
 * preferably be stopped prior to calling de-init.
 *----------------------------------------------------------------------------    
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
void vqec_ifclient_deinit(void);


/**---------------------------------------------------------------------------
 * Start the client interface.
 *
 * vqec_ifclient_init() must have been invoked prior to this call. This
 * function runs the libevent event-polling thread, and will *not* return, 
 * until vqec_ifclient_stop() is executed asynchronously from another thread.
 * @param[out] vqec_err_t Returns VQEC_OK on success. On failure, the 
 * following error codes are returned:
 *     <I>VQEC_ERR_INVCLIENTSTATE</I><BR>
 *     <I>VQEC_ERR_MALLOC</I><BR>
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t vqec_ifclient_start(void); 

/**---------------------------------------------------------------------------
 * Stop the client interface. 
 *
 * The client interface is stopped, i.e., the thread that invoked
 * vqec_ifclient_init() will suspend event polling and return, given that
 * it has been previously started. 
 *----------------------------------------------------------------------------    
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
void vqec_ifclient_stop(void); 

/**---------------------------------------------------------------------------
 * Clear global/historical (non cumulative) statistics. 
 *---------------------------------------------------------------------------- 
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
void vqec_ifclient_clear_stats(void);

/**---------------------------------------------------------------------------
 * Display all state variables for a tuner including statistics (primarily
 * used for debugging). The display is either to the CLI session, if one is
 * active or to stderr if there is no CLI session.
 * 
 * @param[out] vqec_err_t Returns VQEC_OK on success. On failure, the 
 * following error codes are returned:
 *     <I>VQEC_ERR_NOSUCHTUNER</I><BR>
 *---------------------------------------------------------------------------- 
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t vqec_ifclient_tuner_dump_state(const vqec_tunerid_t id, 
                                            unsigned int options_flag);

/**---------------------------------------------------------------------------
 * Retrieves the global/historical tuner statistics.
 * 
 * These statistics track events which occur across all tuners,
 * since VQE-C initialization or the last time at which they were cleared, i.e.
 * the statistics are relative to the last time "clear counters" was issued 
 * from the CLI.
 * 
 * @param[out] ptr to vqec_ifclient_stats_t structure that will be populated
 * @param[out] vqec_err_t Returns VQEC_OK on success. On failure, the 
 * following error codes are returned:
 *     <I>VQEC_ERR_INVALIDARGS</I><BR>
 *---------------------------------------------------------------------------- 
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t 
vqec_ifclient_get_stats(vqec_ifclient_stats_t *s);

/**---------------------------------------------------------------------------
 * Retrieves the cumulative global/historical tuner statistics.
 * 
 * These statistics track events which occur across all tuners,
 * since VQE-C initialization.
 *
 * @param[out] ptr to vqec_ifclient_stats_t structure that will be populated
 * @param[out] vqec_err_t Returns VQEC_OK on success. On failure, the 
 * following error codes are returned:
 *     <I>VQEC_ERR_INVALIDARGS</I><BR>
 *---------------------------------------------------------------------------- 
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t 
vqec_ifclient_get_stats_cumulative(vqec_ifclient_stats_t *s);

/**---------------------------------------------------------------------------
 * Retrieves stats for an active channel (a channel to which one or more
 * tuners are currently bound).
 * 
 * These statistics accumulate with the binding of the first tuner to the
 * channel, and are cleared whenever the channel becomes inactive (no
 * longer bound to any tuner).
 *
 * @param[in]  url          - URL specifying the channel of interest.
 *                            This URL should match the one specified in the
 *                            call to vqec_ifclient_alloc_sdp_handle_from_url()
 *                            prior to the vqec_ifclient_tuner_bind_chan()
 *                             call.  (See the header for
 *                            vqec_ifclient_alloc_sdp_handle_from_url() for
 *                            details.)
 * @param[out] stats        - Stats which have occurred for the active channel
 * @param[out] vqec_error_t - Returns VQEC_OK on success. On failure, the 
 *                             following error codes may be returned:
 *                             <I>VQEC_ERR_INVALIDARGS</I><BR>
 *                             <I>VQEC_ERR_CHANNELPARSE</I><BR>
 *                             <I>VQEC_ERR_CHANNOTACTIVE</I><BR>
 *---------------------------------------------------------------------------- 
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t 
vqec_ifclient_get_stats_channel(const char *url,
				vqec_ifclient_stats_channel_t *stats);

/**---------------------------------------------------------------------------
 * This is a cumulative version of the vqec_ifclient_get_stats_channel API
 * and retrieves cumulative stats for an active channel.
 * For more details refer to vqec_ifclient_get_stats_channel API.
 *
 * @param[in]  url          - URL specifying the channel of interest.
 *                            This URL should match the one specified in the
 *                            call to vqec_ifclient_alloc_sdp_handle_from_url()
 *                            prior to the vqec_ifclient_tuner_bind_chan()
 *                             call.  (See the header for
 *                            vqec_ifclient_alloc_sdp_handle_from_url() for
 *                            details.)
 * @param[out] stats        - Stats which have occurred for the active channel
 * @param[out] vqec_error_t - Returns VQEC_OK on success. On failure, the 
 *                             following error codes may be returned:
 *                             <I>VQEC_ERR_INVALIDARGS</I><BR>
 *                             <I>VQEC_ERR_CHANNELPARSE</I><BR>
 *                             <I>VQEC_ERR_CHANNOTACTIVE</I><BR>
 *---------------------------------------------------------------------------- 
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t 
vqec_ifclient_get_stats_channel_cumulative(const char *url,
				          vqec_ifclient_stats_channel_t *stats);

/**---------------------------------------------------------------------------
 * Display a histogram maintained internally by VQE-C (primarily used for
 * monitoring or troubleshooting).  The display is either to the CLI 
 * session, if one is active or to stderr if there is no CLI session.
 * 
 * @param[in]  hist       Histogram to be displayed
 * @param[out] vqec_err_t Returns VQEC_OK on success. On failure, the 
 * following error codes are returned:
 *     <I>VQEC_ERR_INVALIDARGS</I><BR>
 *     <I>VQEC_ERR_MALLOC</I><BR>
 *     <I>VQEC_ERR_NOTINITIALIZED</I><BR>
 *---------------------------------------------------------------------------- 
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t vqec_ifclient_histogram_display(const vqec_hist_t hist);

/**---------------------------------------------------------------------------
 * Clears a histogram maintained internally by VQE-C (primarily used for
 * monitoring or troubleshooting).
 * 
 * @param[out] vqec_err_t Returns VQEC_OK on success. On failure, the 
 * following error codes are returned:
 *     <I>VQEC_ERR_INVALIDARGS</I><BR>
 *---------------------------------------------------------------------------- 
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t vqec_ifclient_histogram_clear(const vqec_hist_t hist);

/*----------------------------------------------------------------------------
 * Get statistics on updates.
 * 
 * @param[out] vqec_err_t Returns VQEC_OK on success. On failure, the 
 * following error codes are returned:
 *     <I>VQEC_ERR_INVALIDARGS</I><BR>
 *--------------------------------------------------------------------------*/ 
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t 
vqec_ifclient_updater_get_stats(vqec_ifclient_updater_stats_t *s);

/**---------------------------------------------------------------------------
 * Update the channel VQE-C channel lineup.  The channel lineup is a block
 * of SDP descriptions of VQE accelerated channels.  Note this API is
 * used to update the entire VQE-C IPTV channel lineup.  It does not merge
 * any channel descriptions, but rather replaces the channel descriptions with
 * those pointed to by the sdp_block.
 * Note this API should not be used when using VQE-C's CDI (channel delivery
 * infrastructure).  This API is intended for integrations where the
 * VQE-C channel lineup (SDP description) delivery is performed by the
 * STB middleware.  
 *
 * @param[in] sdp block of sdp descriptions of channels.  Must represent
 *            valid sdp channels.
 * following error codes are returned:
 *     <I>VQEC_ERR_INVALIDARGS</I><BR>
 *     <I>VQEC_ERR_INVCLIENTSTATE</I><BR>
 *     <I>VQEC_ERR_SETCHANCFG</I><BR>
 *     <I>VQEC_ERR_NOCHANNELLINEUP</I><BR>
 *--------------------------------------------------------------------------
 */
VQEC_PUBLIC VQEC_SYNCHRONIZED
vqec_error_t
vqec_ifclient_cfg_channel_lineup_set(sdp_block_t *sdp_block);

/*
 * Configuration delivery APIs:
 * Used for updating VQE-C's Network, Override, and Channel Configurations.
 *
 * vqec_ifclient_config_register()
 *     - supports callback functions for events that occur for a configuration
 *       (e.g. config is now missing/corrupt)
 * vqec_ifclient_config_update()
 *     - accepts/validates/persists/applies a new configuration
 * vqec_ifclient_config_status()
 *     - provides information about an existing configuration
 */

/*
 * Registers a function which VQE-C will call when events of interest occur 
 * related to one of its configurations (Network Configuration, Override 
 * Configuration, and Channel Configuration).
 *
 * For example, if cfg_index_pathname is configured, then VQE-C will issue 
 * a notification if it detects that a persistent configuration is 
 * missing/corrupt.
 *
 * Only one (the first) function registered with this API will be accepted 
 * per configuration type; subsequent registrations will be rejected.
 *
 * @param[in]  params       - register parameters
 * @param[out] vqec_error_t - result of registration:
 *        VQEC_ERR_INVALIDARGS
 *          Invalid parameters supplied
 *        VQEC_ERR_ALREADYREGISTERED
 *          Callback already registered for the supplied configuration type.
 *        VQEC_OK
 *          Registration succeeded
 */
vqec_error_t
vqec_ifclient_config_register(
    const vqec_ifclient_config_register_params_t *params);

/*
 * Validates, persists (if so configured) and applies (if VQE-C is enabled)
 * a new/updated configuration.
 * 
 * The supplied configuration is subject to a validity check whose definition 
 * varies based on resource type:
 *
 * VQEC_IFCLIENT_CONFIG_NETWORK:
 *   - If configuration syntax (libconfig) is invalid, the update is rejected
 *   - If configuration has valid syntax but one or more individual 
 *     parameters are assigned invalid values, then the update will be 
 *     accepted but invalid settings will be ignored.
 * VQEC_IFCLIENT_CONFIG_CHANNEL:
 *   - If configuration syntax is invalid or required SDP fields are missing,
 *     the update is rejected
 *
 * If VQE-C Core Services are enabled, configuration changes are applied
 * at next channel change or at next reboot, depending on the configuration
 * which has changed.
 *
 * The update operation is performed synchronously, and thus the caller may 
 * free() or reuse the "buffer" upon return.
 *
 * A return code is supplied which indicates the outcome of the configuration 
 * update, as described below.  In the event of a failure, additional 
 * failure details will be written to the system log.
 *
 * @param[in]  params       - update parameters
 * @param[out] vqec_error_t - 
 *     Result of update:
 *       VQEC_ERR_INVALIDARGS -
 *          Invalid arguments (e.g. NULL buffer pointer)
 *       VQEC_ERR_CONFIG_INVALID -
 *          Buffer rejected, does not contain a valid configuration.
 *       VQEC_ERR_UPDATE_FAILED_EXISTING_INTACT -
 *          Error encountered in processing update.  The existing
 *          configuration has been left intact.
 *       VQEC_ERR_UPDATE_FAILED_EXISTING_REMOVED -
 *          Error encountered in processing update (e.g. failure in writing 
 *          to file).  The existing configuration has been removed.
 *       VQEC_OK -
 *          Configuration accepted.
 *          "persisted" output parameter indicates whether VQE-C persisted 
 *          the configuration.
 */
vqec_error_t
vqec_ifclient_config_update(
    vqec_ifclient_config_update_params_t *params);

/*
 * Provides status information about a persistent VQE-C configuration.
 *
 * @param[in/out] params    - parameters (see structure definition)
 * @param[out] vqec_error_t -
 *                    VQEC_ERR_INVALIDARGS
 *                     Invalid parameters supplied
 *                    VQEC_ERR_NOTINITIALIZED
 *                     Config update services not initialized
 *                    VQEC_OK
 *                     Status information returned
 */
vqec_error_t
vqec_ifclient_config_status(vqec_ifclient_config_status_params_t *params);

/*
 * Returns whether or not VQE-C Core Services are enabled.
 * "enabled" means that VQE-C Core Services initialization
 * was attempted and completed successfully.
 *
 * @param[out] boolean     - TRUE:  VQE-C is enabled/initialized
 *                           FALSE: otherwise
 */
uint8_t
vqec_ifclient_is_vqec_enabled(void);

/*
 * Updates the VQE-C Override Configuration, given override parameter 
 * settings in the form of (tag, value) pairs.  
 *
 * @params[in]  tags:
 *    - An array of character strings, each representing an override 
 *      configuration parameter.
 *    - If tags[i] does not match a VQE-C parameter supported as an override,
 *      then a warning message is logged, and tags[i] is excluded from the 
 *      update.
 *    - A tags[i] entry of NULL terminates the array.  Note that an array
 *      of no parameters (i.e. tags[0] == NULL) or no valid parameters
 *      effectively removes the Override Configuration.
 *
 * @params[in]  values:
 *    - An array of character strings, each representing an override value to 
 *      set.  This list is in a one-to-one mapping with the tags parameter.
 *      For example, tags[5] is the override parameter name and values[5] is
 *      the associated override value to be set.
 *    - If a supplied parameter value is invalid, then the override parameter
 *      setting is ignored.
 *
 * @param[out] vqec_error_t
 *       VQEC_ERR_INVALIDARGS -
 *          Invalid arguments (e.g. NULL params pointer)
 *       VQEC_ERR_UPDATE_FAILED_EXISTING_INTACT -
 *          Error encountered in processing update.  The existing
 *          configuration has been left intact.
 *       VQEC_ERR_UPDATE_FAILED_EXISTING_REMOVED -
 *          Error encountered in processing update (e.g. failure in writing 
 *          to file).  The existing configuration has been removed.
 *       VQEC_OK -
 *          Override update completed.
 */
vqec_error_t
vqec_ifclient_config_override_update(
                     vqec_ifclient_config_override_params_t *params);

/**----------------------------------------------------------------------------
 * Check if the tuner specified by the name, exists.
 * 
 * @param[in] namestr Unique name of the tuner. *namestr cannot be an empty string.
 * @param[out] Boolean value specifying the existence of the tuner
 *--------------------------------------------------------------------------
 */ 
VQEC_PUBLIC VQEC_SYNCHRONIZED 
uint8_t vqec_ifclient_check_tuner_validity_by_name(const char *namestr);

/*---------------------------------------------------------------------------
 * Method to register the new desired RTP CNAME
 *
 * @param[in]: Pointer to a character array that carries the RTP CNAME 
 *             to register. It MUST uniquely identify the client.  If   
 *             cname is NULL (or this function is never called), the 
 *             default value (mac address) will be used.
 *
 *             Note that if this CNAME length is greater than VQEC_MAX_CNAME_LEN
 *             it is rejected. CNAME length is inclusive of the NULL character
 *             used to terminate the CNAME string.
 *
 *             Valid CNAME strings must only contain the following characters:
 *             o letters:       a-z, A-Z
 *             o numbers:       0-9
 *             o special chars: $ - _ . + ! * ' ( ) ,
 *             as documented in RFC 1738, section 2.2 as unreserved and not 
 *             requiring special encoding within an URL.
 *
 *             Also note that incase a new CNAME is not registered, the default
 *             behavior is to use the MAC address of the first found ethernet 
 *             interface.
 * 
 * @param[out] Boolean value that returns TRUE on success and false on failure.  
 *             On failure to register a CNAME, the default cname (mac address) 
 *             will be used.
 *---------------------------------------------------------------------------    */         
VQEC_PUBLIC VQEC_SYNCHRONIZED 
uint8_t vqec_ifclient_register_cname(char *cname);

/**---------------------------------------------------------------------------
 * Is a fastfill operation enabled for the tuner?  Fastfill will be inactive
 * if the application requested no fastfill at bind time, or if the client or 
 * server had fastfill disabled via configuration.  If called when the tuner
 * is not bound the method will return false.
 *
 * @param[in]  id Tuner identifier.
 * @param[out] uint8_t TRUE for fastfill enabled, FALSE otherwise.
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
uint8_t vqec_ifclient_tuner_is_fastfill_enabled(vqec_tunerid_t id);

/**---------------------------------------------------------------------------
 * Is a RCC operation enabled for the tuner?  RCC will be inactive
 * if the application requested no RCC at bind time, or if the client or 
 * server had RCC disabled via configuration.  If called when the tuner
 * is not bound the method will return false.
 *
 * @param[in]  id Tuner identifier.
 * @param[out] uint8_t TRUE for RCC enabled, FALSE otherwise.
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
uint8_t vqec_ifclient_tuner_is_rcc_enabled(vqec_tunerid_t id);

/**---------------------------------------------------------------------------
 * Used to set the tr-135 control parameters gmin and severe_loss_min_distance 
 * bind parameters.
 *
 * @param[in]  bp Pointer to bind parameters data structure
 * @param[in]  params  TR-135 writable parameters 
 * @param[out] uint8_t TRUE for success, FALSE otherwise
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
uint8_t
vqec_ifclient_bind_params_set_tr135_params(vqec_bind_params_t *bp,
                                           vqec_ifclient_tr135_params_t 
                                           *params);

/**---------------------------------------------------------------------------
 * Used to retrieve the tr-135 control bind parameters.
 * 
 * @param[in]  bp      Pointer to bind parameters data structure
 * @param[out] vqec_ifclient_tr135_params_t * Pointer to TR-135 parameters
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
vqec_ifclient_tr135_params_t *
vqec_ifclient_bind_params_get_tr135_params(const vqec_bind_params_t *bp);

/**---------------------------------------------------------------------------
 * Used to set the tr-135 control parameters gmin and severe_loss_min_distance 
 * for an active channel (a channel to whom one or more tuners is currently 
 * bound).
 * These changes to channel's TR135 take effect immediately upon successful
 * return from this API. 
 *
 * @param[in]  url     URL specifying the channel of interest
 * @param[in]  params  TR-135 writable parameters 
 * @param[out] vqec_error_t - Returns VQEC_OK on success. On failure, the 
 *                             following error codes may be returned:
 *                             <I>VQEC_ERR_INVALIDARGS</I><BR>
 *                             <I>VQEC_ERR_CHANNELPARSE</I><BR>
 *                             <I>VQEC_ERR_CHANNOTACTIVE</I><BR>
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
vqec_error_t
vqec_ifclient_set_tr135_params_channel(const char *url,
                                       vqec_ifclient_tr135_params_t *params);

/**---------------------------------------------------------------------------
 * The API is useful to gather TR-135 sample statistics between TR-135
 * application-defined 'sample intervals'. Upon calling this API, VQE-C first 
 * populates the external tr135 sample-stats data structure and then, 
 * TR-135 stats internal to VQE-C, are reset. 
 * Thus, the TR-135 Application can use this function to gather TR-135 sample 
 * statistics between two consecutive invocations of this function. 
 *
 * @param[in]  url     URL specifying the channel of interest
 * @param[out] stats   TR-135 sample parameters 
 * @param[out] vqec_error_t - Returns VQEC_OK on success. On failure, the 
 *                             following error codes may be returned:
 *                             <I>VQEC_ERR_INVALIDARGS</I><BR>
 *                             <I>VQEC_ERR_CHANNELPARSE</I><BR>
 *                             <I>VQEC_ERR_CHANNOTACTIVE</I><BR>
 *----------------------------------------------------------------------------
 */
VQEC_PUBLIC
vqec_error_t
vqec_ifclient_get_stats_channel_tr135_sample(const char *url,
                             vqec_ifclient_stats_channel_tr135_sample_t *stats);


/**---------------------------------------------------------------------------
 * Reserve two ports for a subsequent vqec_ifclient_tuner_bind_chan_cfg
 * call. The reserved port may be used as the destination for the
 * primary or retransmission stream when populating a vqec_chan_cfg_t
 * structure. vqec_ifclient_socket_close must be used to free allocated
 * ports. All arguments may also be used as an input args for specifying 
 * the exact interface and ports to be opened. An address or port with
 * a null or zero value allows VQE-C to choose the appropriate interface
 * or the next available port number.
 *
 * NOTE:  addr and ports MUST be supplied in network byte order,
 *        and are returned in network byte order.
 * 
 * @param[in/out] addr      IP address of open socket (network byte order)
 * @param[in/out] rtp_port  rtp port number of open socket (network byte order)
 * @param[in/out] rtcp_port rtcp port number of open socket(network byte order)
 * @param[out]    uint8_t   TRUE if successful, FALSE otherwise
 *--------------------------------------------------------------------------*/
VQEC_PUBLIC 
uint8_t vqec_ifclient_socket_open (vqec_in_addr_t *addr,
                                   vqec_in_port_t *rtp_port,
                                   vqec_in_port_t *rtcp_port);

/**---------------------------------------------------------------------------
 * Close a port opened by the vqec_ifclient_socket_open function. 
 *
 * NOTE:  addr and ports MUST be supplied in network byte order,
 *
 * @param[in] addr      IP address of open socket (network byte order)
 * @param[in] rtp_port  rtp port number of open socket (network byte order)
 * @param[in] rtcp_port rtcp port number of open socket (network byte order)
 *--------------------------------------------------------------------------*/
VQEC_PUBLIC
void vqec_ifclient_socket_close (vqec_in_addr_t addr,
                                 vqec_in_port_t rtp_port,
                                 vqec_in_port_t rtcp_port);

/**
 * API for adding a channel to the temp channel database
 *
 * @param[in] *chan: the channel cfg that need to be added to temp chan db.
 * @param[in] *chan_params: chan parameters
 * @return TRUE if success, FALSE otherwise
 */
VQEC_PUBLIC
uint8_t vqec_ifclient_add_new_tmp_chan(
        vqec_chan_cfg_t *chan,
        vqec_ifclient_tmp_chan_params_t *chan_params);

/**
 * API for removing a channel from temp channel database
 * based on the destination IP address
 *
 * @param[in] dest_ip: dest ip address (network byte order)
 * @param[in] dest_port (network byte order)
 * @return: TRUE if success, FALSE otherwise
 */
VQEC_PUBLIC
void vqec_ifclient_remove_tmp_chan(vqec_in_addr_t dest_ip, vqec_in_port_t port);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* __VQEC_IFCLIENT_H__ */
