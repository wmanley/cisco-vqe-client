/*
 *------------------------------------------------------------------
 * API for configuration module
 *
 * May 2006, Dong Hsu
 *
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef _CFGAPI_H_
#define _CFGAPI_H_

/*! \file cfgapi.h
    \brief Configuration Module API declarations.

    Top level API for configuration module.
*/

#include <stdio.h>
#include <netinet/ip.h>
#include <utils/vam_types.h>
#include <utils/id_manager.h>

#define MAX_CHANNELS       1000
#define MAX_NAME_LENGTH    100
#define SDP_MAX_STRING_LEN 81
#define SDP_MAX_TYPE_LEN   10
#define MAX_KEY_LENGTH     400

/*! \enum cfg_ret_e
    \brief Return value from cfg_ api calls.
*/
typedef enum {
    CFG_SUCCESS,        /**< success */
    CFG_FAILURE,        /**< generic failure */
    CFG_OPEN_FAILED,    /**< failed to open data base */
    CFG_INVALID_HANDLE, /**< use an invalid channel handle */
    CFG_MALLOC_REQ,     /**< memory must be allocated */
    CFG_MALLOC_ERR,     /**< memory allocation error */
    CFG_GET_FAILED,     /**< failed to get channel configuration */
    CFG_ALREADY_INIT,   /**< already initialized */
    CFG_UNINITIALIZED,  /**< uninitialized */
    CFG_VALIDATION_ERR  /**< channel validation failure */
} cfg_ret_e;

/*! \struct cfg_stats_t
    \brief Stats derived from parsing a channel configuration in SDP
 */
typedef struct cfg_stats_ {
    uint32_t num_syntax_errors;  /**< number of channels w/syntax errors */
} cfg_stats_t;

/*! \enum cfg_chan_type_e
    \brief SDP semantic type (VoD or Linear).
*/
typedef enum {
    CFG_LINEAR = 0,
    CFG_VOD
} cfg_chan_type_e;

/*! \enum stream_proto_e
    \brief Media stream protocol

    Currently, two types of transport are used for media stream:
    udp and rtp.
*/
typedef enum {
    UDP_STREAM,
    RTP_STREAM
} stream_proto_e;


/*! \enum mode_e
    \brief VAM mode

    Currently, two modes are supported:
    Source mode and lookaside mode
*/
typedef enum {
    DISTRIBUTION_MODE,
    SOURCE_MODE,
    LOOKASIDE_MODE,
    RECV_ONLY_MODE,
    UNSPECIFIED_MODE
} mode_e;


/*! \enum role_e
    \brief Role for the configuration

    SSM Distribution Source, VAM, or STB
*/
typedef enum {
    SSM_DS,
    VAM,
    STB,
    UNSPECIFIED_ROLE
} role_e;


typedef enum {
    REFLECTION,
    RSI
} rtcp_fb_mode_e;


typedef enum {
    FEC_1D_MODE,
    FEC_2D_MODE
} fec_mode_e;

/*! \struct fec_stream_t
    \brief FEC media stream configuration
 */
typedef struct fec_stream_cfg_ {
    struct in_addr      multicast_addr;
    struct in_addr      src_addr;
    in_port_t           rtp_port;
    in_port_t           rtcp_port;
    uint8_t             payload_type;
    uint32_t            rtcp_sndr_bw;
    uint32_t            rtcp_rcvr_bw;
} fec_stream_cfg_t;


/* Current supported RTCP XR stat-summary bit mask */
#define RTCP_XR_LOSS_BIT_MASK           0x00000001
#define RTCP_XR_DUP_BIT_MASK            0x00000002
#define RTCP_XR_JITT_BIT_MASK           0x00000004
#define RTCP_XR_SUPPORTED_OPT_MASK      0x00000007
#define RTCP_XR_UNSUPPORTED_OPT_MASK    0xFFFFFFF8

/* define FEC sending order */

typedef enum fec_sending_order_ {
    FEC_SENDING_ORDER_NOT_DECIDED = 0,
    FEC_SENDING_ORDER_ANNEXA,
    FEC_SENDING_ORDER_ANNEXB,
    FEC_SENDING_ORDER_OTHER,
} fec_sending_order_t;

typedef struct fec_info_ {
    uint8_t             fec_l_value;
    uint8_t             fec_d_value;
    fec_sending_order_t fec_order;
    uint64_t            rtp_ts_pkt_time; /* in usec */
} fec_info_t;


/*! \struct channel_cfg_t
    \brief Channel configuration

    channel_cfg_t is an internal structure to conatin all the information
    needed for configuring a channel for both VAM and STB.
 */
typedef struct channel_cfg_ {
    idmgr_id_t          handle;
    char                name[SDP_MAX_STRING_LEN];
    char                session_key[MAX_KEY_LENGTH];
    uint64_t            version;
    boolean             complete;
    mode_e              mode;
    role_e              role;
    boolean             active;
    boolean             used;

    /* Primary Source Stream Information */
    stream_proto_e      source_proto;
    struct in_addr      original_source_addr;
    struct in_addr      src_addr_for_original_source;
    in_port_t           original_source_port;
    in_port_t           original_source_rtcp_port;
    uint8_t             original_source_payload_type;
    uint32_t            bit_rate;
    uint32_t            original_rtcp_sndr_bw;
    uint32_t            original_rtcp_rcvr_bw;
    uint32_t            original_rtcp_per_rcvr_bw;
    uint16_t            original_rtcp_xr_loss_rle;
    uint16_t            original_rtcp_xr_per_loss_rle;
    uint32_t            original_rtcp_xr_stat_flags;
    boolean             original_rtcp_xr_multicast_acq;
    boolean             original_rtcp_rsize;
    boolean             original_rtcp_xr_diagnostic_counters;

    /* Re-sourced Stream Information */
    struct in_addr      re_sourced_addr;
    struct in_addr      src_addr_for_re_sourced;
    in_port_t           re_sourced_rtp_port;
    in_port_t           re_sourced_rtcp_port;
    uint8_t             re_sourced_payload_type;
    uint32_t            re_sourced_rtcp_sndr_bw;
    uint32_t            re_sourced_rtcp_rcvr_bw;

    /* FBT address */
    struct in_addr      fbt_address;

    /* Unicast Retransimission Stream Information */
    struct in_addr      rtx_addr;
    in_port_t           rtx_rtp_port;
    in_port_t           rtx_rtcp_port;
    uint8_t             rtx_payload_type;
    uint8_t             rtx_apt;
    uint32_t            rtx_time;
    boolean             er_enable;
    boolean             fcc_enable;
    rtcp_fb_mode_e      rtcp_fb_mode;
    uint32_t            repair_rtcp_sndr_bw;
    uint32_t            repair_rtcp_rcvr_bw;
    uint16_t            repair_rtcp_xr_loss_rle;
    uint32_t            repair_rtcp_xr_stat_flags;

    /* FEC Stream Information */
    boolean             fec_enable;
    fec_mode_e          fec_mode;
    fec_stream_cfg_t    fec_stream1;
    fec_stream_cfg_t    fec_stream2;
    fec_info_t          fec_info;

    /* MD5 checksum value for SDP content */
    uint32_t            chksum;

    /* VAM specific parameters: 
     * gop_size is obsolete now; overrate_percentage is also known as 
     * e-factor.
     */
    uint32_t            gop_size;
    uint32_t            max_packet_size;
    float               overrate_percentage;
    uint8_t             rtp_er_dscp;
    uint8_t             rtp_rcc_dscp;

} channel_cfg_t;


/*! \fn void cfg_set_dbname(const char *cfg_db_name)
    \brief Set db name in the configuration module.

    \param cfg_db_name The name of configuration database.
*/
extern void cfg_set_db_name(const char *cfg_db_name);


/*! \fn cfg_ret_e cfg_init(const char *cfg_db_name)
    \brief Initialize the configuration module.

    \param cfg_db_name The name of configuration database.
*/
extern cfg_ret_e cfg_init(const char *cfg_db_name);


/*! \fn cfg_ret_e cfg_shutdown()
    \brief Clean up the configuration module.
*/
extern cfg_ret_e cfg_shutdown(void);

/*! \fn cfg_ret_e cfg_save()
    \brief Save the configuration data to the database.
*/
extern cfg_ret_e cfg_save(const char *cfg_db_name);

/*! \fn cfg_ret_e cfg_save_from()
    \brief Save the configuration data from a given file
*/
extern cfg_ret_e cfg_save_from(const char *cfg_db_name);


/*! \fn cfg_ret_e cfg_update()
    \brief Update the configuration data to the database.
*/
extern cfg_ret_e cfg_update(const char *cfg_db_name);


/*! \fn boolean cfg_is_savable()
    \brief Check whether the configuration file is savable or not
*/
extern boolean cfg_is_savable(void);


/*! \fn const char* cfg_get_timestamp()
    \brief Get the time stamp of the configuration data
*/
extern const char* cfg_get_timestamp(char *buffer, uint32_t length);


/*! \fn cfg_ret_e cfg_get_all_channels(idmgr_id_t *channels_p, 
                                       uint16_t *total_channels)
    \brief Get the list of channel handles.

    \param handles_p      An array of channel handles
    \param total_channels The total number of channel handles in the array
*/
extern cfg_ret_e cfg_get_all_channels(idmgr_id_t **handles_p,
                                      uint16_t *total_channels);

/*! \fn cfg_ret_e cfg_get_all_channel_data(char *data_buffer, uint32_t length)
    \brief Put all channel configuration data in SDP syntax in one big buffer.

*/
extern cfg_ret_e cfg_get_all_channel_data(char *data_buffer,
                                          uint32_t buffer_length);

/*! \fn cfg_ret_e cfg_parse_all_channel_data(char *data_buffer)
    \brief Parse all channel configuration data in SDP syntax and store it internally.

*/
extern cfg_ret_e cfg_parse_all_channel_data(char *data_buffer);

/*! \fn cfg_ret_e cfg_parse_single_channel_data(char *data_buffer,
                                                channel_cfg_t *chan_cfg,
                                                cfg_chan_type_e chan_type)
    \brief Parse channel configuration data in SDP syntax and store it in chan_cfg.

*/
extern cfg_ret_e cfg_parse_single_channel_data(char *data_buffer,
                                               channel_cfg_t *chan_cfg,
                                               cfg_chan_type_e chan_type);

/*! \fn uint16_t cfg_get_total_num_channels()
    \brief Get the total number of channels.

*/
extern uint16_t cfg_get_total_num_channels(void);

/*! \fn uint16_t cfg_get_total_num_active_channels()
    \brief Get the total number of active channels.

*/
extern uint16_t cfg_get_total_num_active_channels(void);

/*! \fn uint16_t cfg_get_total_num_input_channels()
    \brief Get the total number of input channels.

*/
extern uint16_t cfg_get_total_num_input_channels(void);


/*! \fn channel_cfg_t *cfg_get_channel_cfg_from_idx(uint16 index)
    \brief Get the channel configuration data based on an index

*/
extern channel_cfg_t *cfg_get_channel_cfg_from_idx(uint16_t index);

/*! \fn channel_cfg_t *cfg_get_channel_cfg_from_hdl(idmgr_id_t handle)
    \brief Get the channel configuration data based on a handle

*/
extern channel_cfg_t *cfg_get_channel_cfg_from_hdl(idmgr_id_t handle);

/*! \fn channel_cfg_t *cfg_get_channel_cfg_from_orig_src_addr(
                                        char* mcast_ip_addr, uint16_t port)
    \brief Retrieve the channel configuration data based on original multicast address and port

    \param mcast_ip_addr multicast address of original source stream
    \param port          port of original source stream
*/
extern channel_cfg_t *cfg_get_channel_cfg_from_orig_src_addr(
    struct in_addr mcast_ip_addr,
    in_port_t port);

/*! \fn channel_cfg_t *cfg_get_channel_cfg_from_origin(char *username,
                                                       char *session_id,
                                                       char *nettype,
                                                       char *addrtype, 
                                                       char *creator_addr)
    \brief Retrieve the channel configuration data based on three fields in originator of SDP session: username, session-id, and creator's address

    \param username     user's login in the originating host
    \param session_id   numeric string such as NTP timestamp
    \param creator_addr IP address or FQDN of session creator
*/
extern channel_cfg_t *cfg_get_channel_cfg_from_origin(const char *username,
                                                      const char *session_id,
                                                      const char *nettype,
                                                      const char *addrtype,
                                                      const char *creator_addr);

/**
 * channel_cfg_sdp_handle_t - defines the per tv channel globally unique 
 * identifier
 *                      - this field is really a NULL terminated text string 
 *                      - in the form of a valid "o=" SDP description.
 *                      - See RFC 4456 for a description of the "o=" syntax.
 */
typedef char* channel_cfg_sdp_handle_t;

/*! \fn channel_cfg_t *cfg_get_channel_cfg_from_SDP_hdl(
                                channel_cfg_sdp_handle_t sdp_handle)

   \brief Get the channel configuration data based on an sdp_handle (the "o=" line).
   Note the version portion of the "o=" line will not be used in the lookup / match.
*/
extern channel_cfg_t *cfg_get_channel_cfg_from_SDP_hdl(
    channel_cfg_sdp_handle_t handle);

/*! \fn channel_cfg_sdp_handle_t  cfg_alloc_SDP_handle_from_orig_src_addr(
        stream_proto_e protocol, char* mcast_ip_addr, uint16_t port);
   \brief Retrieve the channel configuration data based on original multicast address and port,
          returns a valid configuration handle wich the caller is responsible to free or NULL.
   \param protocol protocol of original source stream (UDP/RTP)
   \param mcast_ip_addr multicast address of original source stream
   \param port          port of original source stream
*/
extern channel_cfg_sdp_handle_t cfg_alloc_SDP_handle_from_orig_src_addr(
    stream_proto_e protocol, struct in_addr mcast_ip_addr, in_port_t port);

/*! \fn channel_cfg_sdp_handle_t
        cfg_clone_SDP_handle(const channel_cfg_sdp_handle_t sdp_handle);
    \brief Clone, i.e. allocate a copy of, an sdp_handle.
        returns a valid sdp configuration handle or NULL. Note the caller
        is responsible for freeing non-NULL handles.
*/
extern channel_cfg_sdp_handle_t cfg_clone_SDP_handle(
    const channel_cfg_sdp_handle_t sdp_handle);

/*! \fn channel_cfg_sdp_handle_t  cfg_free_SDP_handle(sdp_handle)
   \brief Free an sdp_handle allocated by cfg_freeSDPHandle
*/
extern void cfg_free_SDP_handle(channel_cfg_sdp_handle_t sdp_handle);

/*! \fn boolean
        cfg_SDP_handle_equivalent(const channel_cfg_sdp_handle_t sdp_handle1,
                                  const channel_cfg_sdp_handle_t sdp_handle2);

    \brief Determine if true sdp_handles are equivalent.  Two handles
           are equivelent if they map to the same channel.  Note that
           the version sub-field of the sdp_handle ("o=..." line).
           is ignored in the comparison.
           two handles are equivalent if they
           map to the same channel, regardless of channel version.
*/
extern boolean cfg_SDP_handle_equivalent(
    const channel_cfg_sdp_handle_t sdp_handle1,
    const channel_cfg_sdp_handle_t sdp_handle2);

/*! \fn cfg_ret_e cfg_get_system_params(uint32_t *gop_size,
                                        uint8_t *burst_rate)
    \brief Get the sysetm wide parameters
*/
extern cfg_ret_e cfg_get_system_params(uint32_t *gop_size,
                                       uint8_t *burst_rate);

/*! \fn cfg_ret_e cfg_set_system_params(uint32_t gop_size, uint8_t burst_rate)
    \brief Set the sysetm wide parameters
*/
extern cfg_ret_e cfg_set_system_params(uint32_t gop_size,
                                       uint8_t burst_rate);

/*! \fn void cfg_printf_channel_cfg(channel_cfg_t *channel_p);
    \brief Print out the content in channel configuration
*/
extern void cfg_print_channel_cfg(channel_cfg_t *channel_p);

/*! \fn cfg_ret_e cfg_get_cfg_stats(uint32_t *parsed, uint32_t *validated)
    \brief Get the sysetm wide stats
*/
extern cfg_ret_e cfg_get_cfg_stats(uint32_t *parsed,
                                   uint32_t *validated,
                                   uint32_t *total);

/*! \fn cfg_ret_e cfg_commit_update(void)
    \brief Change the current channel manager with new one
*/
extern cfg_ret_e cfg_commit_update(void);

/*! \fn boolean cfg_get_client_source_address
    \brief Get the source address (i.e. where packets are sent from) as seen by a client.
 */
boolean
cfg_get_client_source_address(const channel_cfg_t *channel,
                              struct in_addr *src_addr);

/*! \fn void cfg_remove_redundant_FBTs(void);
    \brief Remove all the channels with redundant FBT address
*/
extern void cfg_remove_redundant_FBTs(void);

/*! \fn channel_cfg_t *cfg_find_in_update
    \brief Get the existing channel configuration data in update file based on antoher one.
 */
extern channel_cfg_t *cfg_find_in_update(const channel_cfg_t *channel);

/*! \fn void cfg_remove_channel
    \brief Remove the existing channel configuration data.
 */
extern void cfg_remove_channel(const channel_cfg_t *channel);

/*! \fn boolean cfg_has_changed
    \brief Compare two channels based on the identifiers.
 */
extern boolean cfg_has_changed(channel_cfg_t *channel1,
                               channel_cfg_t *channel2);

/*! \fn boolean cfg_is_identical
    \brief Compare two channels based all the configuration data except the version.
 */
extern boolean cfg_is_identical(channel_cfg_t *channel1,
                                channel_cfg_t *channel2);

/*! \fn channel_cfg_t *cfg_get_next_in_update
    \brief Find the next available channel configuration data in update file
 */
extern channel_cfg_t *cfg_get_next_in_update(void);

/*! \fn channel_cfg_t *cfg_insert_channel
    \brief Insert a channel configuration data into cfg module.
 */
extern channel_cfg_t *cfg_insert_channel(channel_cfg_t *channel);

/*! \fn void cfg_cleanup_update
    \brief Clean up the memory allocated in update
 */
extern void cfg_cleanup_update(void);

/*! \fn void cfg_copy_cfg_stats
    \brief Copy the sysetm wide stats
*/
extern void cfg_copy_cfg_stats(void);

/*! \fn boolean cfg_validate_all
    \brief Validate the channels with the existing one
*/
extern boolean cfg_validate_all(void);

/*! \fn void cfg_get_update_stats
    \brief Get update stats in text format
*/
extern void cfg_get_update_stats(char *string, int length);

/*! \fn void cfg_get_update_stats
    \brief Get update stats values
*/
extern void cfg_get_update_stats_values(cfg_stats_t *stats);

/* Function:    read_line
 * Description: Return a line from the input buffer
 * Parameters:  line_buffer     Returned line buffer
 *              max_length      The length of the buffer
 *              whole_buffer    The input buffer to be parsed
 *              total_length    Length of whole_buffer
 *              start_idx       Starting index to be parsed in input buffer
 * Returns:     The index after the line being parsed
 */
int read_line(char *line_buffer, 
              int max_length, 
              char *whole_buffer, 
              int total_length,
              int start_idx);

#endif /* _CFGAPI_H_ */
