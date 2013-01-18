/*
 *------------------------------------------------------------------
 * Channel access and management functions for configuration module
 *
 * May 2006, Dong Hsu
 *
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include "cfg_channel.h"
#include <sdp.h>
#include <utils/vam_debug.h>
#include <utils/vam_util.h>
#include <utils/strl.h>

#include <log/vqe_cfg_syslog_def.h>
#include <log/vqe_cfg_debug.h>

typedef enum {
    ORIGINAL_STREAM = 0,
    RE_SOURCED_STREAM,
    UNICAST_RTX_STREAM,
    FEC_FIRST_STREAM,
    FEC_SECOND_STREAM,
    MAX_SESSIONS
} session_type_e;

#define MIN_LINEAR_SESSIONS 3
#define MIN_VOD_SESSIONS 1

#define MAX_MSG_LENGTH 256

#define RTCP_BW_UNSPECIFIED 0xffffffff
#define RTCP_RLE_UNSPECIFIED 0xffff
#define ATTR_LEN 10
#define MAX_VERSION_ALLOWED 9223372036854775806LL

#define SENDONLY_STR "sendonly"
#define RECVONLY_STR "recvonly"
#define INACTIVE_STR "inactive"

#define MIN_PORT_ALLOWED 1024
#define MAX_BW_ALLOWED INT_MAX
#define MIN_BIT_RATE_ALLOWED 1
#define MAX_BIT_RATE_ALLOWED 40000

#define MIN_PT 96
#define MAX_PT 127
#define STATIC_PT 33

#define MAX_GROUP_LINES 3
#define GROUP_SIZE 4*SDP_MAX_STRING_LEN

#define CHECK_DOCUMENT ""
#define USE_IN_ADDR_ANY "Will use INADDR_ANY (0.0.0.0) instead."

#define LOSS_RLE_HEADER 12

/* sa_ignore DISABLE_RETURNS strlcpy */

static channel_cfg_t* is_exist(void *sdp_p, channel_mgr_t *channel_mgr_p);
static boolean linear_validation_and_extraction(void *sdp_p,
                                                channel_cfg_t *channel_p);
static session_type_e figure_out_session_type(void *sdp_p,
                                              uint16_t index,
                                              channel_cfg_t *channel_p);
static int get_bit_rate(void *sdp_p, uint16_t index);
static boolean check_rtcp_port(in_port_t *rtp, in_port_t *rtcp, 
                               char *stream_name, channel_cfg_t *channel_p);
static void cp_fec(fec_stream_cfg_t *dest, fec_stream_cfg_t *src);
static boolean possible_lose_xr_data(channel_cfg_t *channel_p);

/* Function:    cfg_channel_add
 * Description: Create a channel structure to store all the configuration
 *              information for a given channel
 * Parameters:  sdp_p           pointer to the SDP description
 *              channel_mgr_p   pointer to the channel manager
 *              handle          channel handle for the new channel
 * Returns:     
 */
cfg_channel_ret_e cfg_channel_add (void *sdp_p,
                                   channel_mgr_t *channel_mgr_p,
                                   idmgr_id_t *handle_p,
                                   uint32_t checksum)
{
    channel_cfg_t *channel_p;

    /* Check whether the channel is exist. If not, add it */
    channel_p = is_exist(sdp_p, channel_mgr_p);
    if (channel_p == NULL) {
        /* Check whether we have exceed the maximum allowed */
        if (channel_mgr_p->total_num_channels >= MAX_CHANNELS) {
            syslog_print(CFG_REACH_MAX_WARN, MAX_CHANNELS);

            return CFG_CHANNEL_EXCEED_MAX;
        }

        /* Create a channel structure */
        channel_p = (channel_cfg_t *) malloc(sizeof(channel_cfg_t));
        if (channel_p == NULL) {
            syslog_print(CFG_MALLOC_ERR);
            return CFG_CHANNEL_MALLOC_ERR;
        }

        /* Clean up the memory */
        memset(channel_p, 0, sizeof(channel_cfg_t));

        /* Get a handle from the ID manager */
        *handle_p = id_get((void *)channel_p, channel_mgr_p->handle_mgr);
        channel_p->handle = *handle_p;

        channel_p->chksum = checksum;

        /* Semantic validation and content extraction */
        if (linear_validation_and_extraction(sdp_p, channel_p) == FALSE) {
            /* Remove the handle from the ID manager */
            id_delete(channel_p->handle, channel_mgr_p->handle_mgr);
            free(channel_p);
            return CFG_CHANNEL_FAILED_GET_INFO;
        }

        /* Also, add the channel to the channel map */
        if (cfg_channel_add_map(channel_mgr_p, channel_p)
            != CFG_CHANNEL_SUCCESS) {
            syslog_print(CFG_ADD_TO_MAP_WARN, channel_p->session_key);

            /* Remove the handle from the ID manager */
            id_delete(channel_p->handle, channel_mgr_p->handle_mgr);
            free(channel_p);
            return CFG_CHANNEL_FAILED_ADD_MAP;
        }

        channel_mgr_p->handles[channel_mgr_p->total_num_channels] = *handle_p;

        /* Increment the total channel count */
        channel_mgr_p->total_num_channels++;
    }
    else {
        syslog_print(CFG_ADD_ERR, channel_p->session_key);
        return CFG_CHANNEL_EXIST;
    }

    return CFG_CHANNEL_SUCCESS;
}


/* Function:    cfg_channel_delete
 * Description: 
 * Parameters:  
 * Returns:     
 */
cfg_channel_ret_e cfg_channel_delete (channel_mgr_t *channel_mgr_p,
                                      idmgr_id_t handle,
                                      boolean remove_from_map)
{
    uint16_t i;
    channel_cfg_t *channel_p;
    id_mgr_ret ret;

    /* Retrieval of channel pointer based on the handle */
    channel_p = (channel_cfg_t *)id_to_ptr(handle, &ret,
                                           channel_mgr_p->handle_mgr);

    if (channel_p == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "cfg_channel_delete:: Invalid channel 0x%lx\n",
                      handle);
        return CFG_CHANNEL_INVALID_HANDLE;
    }

    VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                  "cfg_channel_delete:: Deleting channel 0x%lx\n",
                  channel_p->handle);

    if (channel_p->handle != handle) {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "cfg_channel_delete:: non-matched channel handle "
                      "[0x%lx - 0x%lx]\n",
                      handle, channel_p->handle);
        return CFG_CHANNEL_INVALID_HANDLE;
    }

    /* Remove the handle from the ID manager */
    id_delete(handle, channel_mgr_p->handle_mgr);

    /* Remove from the channel map */
    if (remove_from_map) {
        if (cfg_channel_del_map(channel_mgr_p, channel_p)
            != CFG_CHANNEL_SUCCESS) {
            syslog_print(CFG_REMOVE_FROM_MAP_WARN, channel_p->session_key);
        }
    }

    /* Free the memory for channel structure */
    free(channel_p);

    /* Remove the handle from the configuration manager */
    for (i = 0; i < channel_mgr_p->total_num_channels; i++) {
        if (channel_mgr_p->handles[i] == handle) {
            channel_mgr_p->handles[i] = ILLEGAL_ID;
            break;
        }
    }

    return CFG_CHANNEL_SUCCESS;
}


/* Function:    cfg_channel_add_map
 * Description: Add a channel structure to the channel maps
 *
 *              There is a two-fold purpose for this function:
 *
 *              1. Add the original multicast address and port to
 *                 the channel map so the channel can be easily retrieved
 *                 from the map based on the original multicast address
 *                 and port;
 *              2. Use the hashmap to check the uniqueness of address and
 *                 port across all the channels.
 *
 *              The rules for channel address and port restriction are as
 *              follows:
 *
 *              Rule 1: No two channels allowed to have same primary multicast
 *                      address AND original_RTP_port combination. Also No two 
 *                      channels are allowed to have same primary multicast 
 *                      address AND original_RTCP_port combination
 *                      - Two channels are allowed to have same multicast 
 *                        address, as long as the the RTP and RTCP ports are 
 *                        different between two channels
 *
 *              Rule 2: No two channels are allowed to have same FBT_IP_address
 *                      AND original_RTCP_port combination as well as same
 *                      FBT_IP_address AND rtx_rtp_port combination, and same
 *                      FBT_IP_address AND rtx_rtcp_port combination.
 *                      - This is required so that VQE-S can properly 
 *                        demultiplex the primary RTCP packets for different 
 *                        channels (RTP sessions are identified using destIP,
 *                        dest port tuple).
 *                      - Combination of FBT_IP_address AND rtx_rtp_port or
 *                        rtx_rtcp_port not being allowed is due to the
 *                        conflict for receiving nack message vs regular
 *                        RTCP message for unicast retransmission stream.
 *
 *              Rule 3: FEC stream for a channel cannot use same multicast 
 *                      address AND port combination that has already been 
 *                      used by another channel primary stream OR another 
 *                      channel's FEC stream. 
 *                      - For now, we allow FEC stream's multicast address to 
 *                        overlap with another (or same) channel's primary or 
 *                        FEC multicast address, as long as RTP ports are 
 *                        different.
 *
 * Parameters:  channel_mgr_p   pointer to the channel manager
 *              channel_p       the channel
 * Returns:     
 */
cfg_channel_ret_e cfg_channel_add_map (channel_mgr_t *channel_mgr_p,
                                       channel_cfg_t *channel_p)
{
    vqe_hash_key_t hkey;
    vqe_hash_elem_t *elem;
    char tmp[INET_ADDRSTRLEN];
    in_port_t rtcp_port = 0;

    if (channel_p == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "cfg_channel_add_map:: Invalid channel pointer.");
        return CFG_CHANNEL_INVALID_CHANNEL;
    }
    else {
        if (channel_p->mode == SOURCE_MODE) {
            rtcp_port = channel_p->re_sourced_rtcp_port;
        }
        else if (channel_p->mode == LOOKASIDE_MODE) {
                rtcp_port = channel_p->original_source_rtcp_port;
        }

        /* Add the original multicast address and rtp port */
        if (channel_add_map(&channel_mgr_p->channel_map,
                            channel_p->original_source_addr,
                            channel_p->original_source_port,
                            channel_p->handle) == FALSE) {
            VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                          "combination of multicast address %s and port %d "
                          "has been used for another channel", 
                          inet_ntop(AF_INET, &(channel_p->original_source_addr),
                                    tmp, INET_ADDRSTRLEN),
                          ntohs(channel_p->original_source_port)); 
            syslog_print(CFG_DUPLICATED_SRC_ADDR_WARN, channel_p->session_key);

            return CFG_CHANNEL_FAILED_ADD_MAP;
        }

        /* Add the original multicast address and rtcp port */
        if (channel_p->original_source_rtcp_port != 0) {
            if (channel_add_map(&channel_mgr_p->channel_map,
                                channel_p->original_source_addr,
                                channel_p->original_source_rtcp_port,
                                channel_p->handle) == FALSE) {
                VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                              "combination of multicast address %s and rtcp "
                              "port %d has been used for another channel", 
                              inet_ntop(AF_INET, &(channel_p->original_source_addr),
                                        tmp, INET_ADDRSTRLEN),
                              ntohs(channel_p->original_source_rtcp_port)); 
                syslog_print(CFG_DUPLICATED_SRC_ADDR_WARN,
                             channel_p->session_key);

                /* Remove everything from the channel map */
                if (cfg_channel_del_map(channel_mgr_p, channel_p)
                    != CFG_CHANNEL_SUCCESS) {
                    syslog_print(CFG_REMOVE_FROM_MAP_WARN,
                                 channel_p->session_key);
                }

                return CFG_CHANNEL_FAILED_ADD_MAP;
            }
        }

        if (channel_p->mode == SOURCE_MODE ||
            channel_p->mode == LOOKASIDE_MODE) {
            /* Add the fbt address and rtcp port */
            if (channel_add_map(&channel_mgr_p->fbt_map,
                                channel_p->fbt_address,
                                rtcp_port,
                                channel_p->handle) == FALSE) {
                VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                              "combination of FBT address %s and port %d has "
                              "been used for another channel", 
                              inet_ntop(AF_INET, &(channel_p->fbt_address),
                                        tmp, INET_ADDRSTRLEN),
                              ntohs(rtcp_port)); 
                syslog_print(CFG_DUPLICATED_FBT_ADDR_WARN,
                             channel_p->session_key);

                /* Remove everything from the channel map */
                if (cfg_channel_del_map(channel_mgr_p, channel_p)
                    != CFG_CHANNEL_SUCCESS) {
                    syslog_print(CFG_REMOVE_FROM_MAP_WARN,
                                 channel_p->session_key);
                }

                return CFG_CHANNEL_FAILED_ADD_MAP;
            }

            /* Add the unicast rtx address and rtp port */
            if(channel_p->rtx_rtp_port != channel_p->original_source_rtcp_port &&
               channel_add_map(&channel_mgr_p->fbt_map,
                               channel_p->fbt_address,
                               channel_p->rtx_rtp_port,
                               channel_p->handle) == FALSE) {
                VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                              "combination unicast address %s and rtp port %d "
                              "has been used for another channel", 
                              inet_ntop(AF_INET, &(channel_p->fbt_address),
                                        tmp, INET_ADDRSTRLEN),
                              ntohs(channel_p->rtx_rtp_port)); 
                syslog_print(CFG_DUPLICATED_RTX_ADDR_WARN,
                             channel_p->session_key);

                /* Remove everything from the channel map */
                if (cfg_channel_del_map(channel_mgr_p, channel_p)
                    != CFG_CHANNEL_SUCCESS) {
                    syslog_print(CFG_REMOVE_FROM_MAP_WARN,
                                 channel_p->session_key);
                }
                
                return CFG_CHANNEL_FAILED_ADD_MAP;
            }

            /* Add the unicast rtx address and rtcp port */
            if (channel_add_map(&channel_mgr_p->fbt_map,
                                channel_p->fbt_address,
                                channel_p->rtx_rtcp_port,
                                channel_p->handle) == FALSE) {
                VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                              "combination unicast address %s and rtcp port "
                              "%d has been used for another channel", 
                              inet_ntop(AF_INET, &(channel_p->fbt_address),
                                        tmp, INET_ADDRSTRLEN),
                              ntohs(channel_p->rtx_rtcp_port)); 
                syslog_print(CFG_DUPLICATED_RTX_ADDR_WARN,
                             channel_p->session_key);


                /* Remove everything from the channel map */
                if (cfg_channel_del_map(channel_mgr_p, channel_p)
                    != CFG_CHANNEL_SUCCESS) {
                    syslog_print(CFG_REMOVE_FROM_MAP_WARN,
                                 channel_p->session_key);
                }
                
                return CFG_CHANNEL_FAILED_ADD_MAP;
            }
        }

        if (channel_p->fec_enable) {
            if (channel_add_map(&channel_mgr_p->channel_map,
                                channel_p->fec_stream1.multicast_addr,
                                channel_p->fec_stream1.rtp_port,
                                channel_p->handle) == FALSE) {
                VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                              "combination of multicast address %s and port "
                              "%d for first FEC stream has been used for "
                              "another channel",
                              inet_ntop(AF_INET,
                                        &(channel_p->fec_stream1.multicast_addr),
                                        tmp, INET_ADDRSTRLEN),
                              ntohs(channel_p->fec_stream1.rtp_port)); 
                syslog_print(CFG_DUPLICATED_SRC_ADDR_WARN,
                             channel_p->session_key);

                /* Remove everything from the channel map */
                if (cfg_channel_del_map(channel_mgr_p, channel_p)
                    != CFG_CHANNEL_SUCCESS) {
                    syslog_print(CFG_REMOVE_FROM_MAP_WARN,
                                 channel_p->session_key);
                }
                
                return CFG_CHANNEL_FAILED_ADD_MAP;
            }

            if (channel_p->fec_stream1.rtcp_port != 0 ) {
                if (channel_add_map(&channel_mgr_p->channel_map,
                                    channel_p->fec_stream1.multicast_addr,
                                    channel_p->fec_stream1.rtcp_port,
                                    channel_p->handle) == FALSE) {
                    VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                                  "combination of multicast address %s and "
                                  "rtcp %d for first FEC stream has been used "
                                  "for another channel",
                                  inet_ntop(AF_INET,
                                            &(channel_p->fec_stream1.multicast_addr),
                                            tmp, INET_ADDRSTRLEN),
                                  ntohs(channel_p->fec_stream1.rtcp_port)); 
                    syslog_print(CFG_DUPLICATED_SRC_ADDR_WARN,
                                 channel_p->session_key);

                    /* Remove everything from the channel map */
                    if (cfg_channel_del_map(channel_mgr_p, channel_p)
                        != CFG_CHANNEL_SUCCESS) {
                        syslog_print(CFG_REMOVE_FROM_MAP_WARN,
                                     channel_p->session_key);
                    }
                
                    return CFG_CHANNEL_FAILED_ADD_MAP;
                }
            }

            if (channel_p->fec_mode == FEC_2D_MODE) {
                if (channel_add_map(&channel_mgr_p->channel_map,
                                    channel_p->fec_stream2.multicast_addr,
                                    channel_p->fec_stream2.rtp_port,
                                    channel_p->handle) == FALSE) {
                    VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                                  "combination of multicast address %s and "
                                  "rtp %d for second FEC stream has been used "
                                  "for another channel",
                                  inet_ntop(AF_INET,
                                            &(channel_p->fec_stream2.multicast_addr),
                                            tmp, INET_ADDRSTRLEN),
                                  ntohs(channel_p->fec_stream2.rtp_port)); 
                    syslog_print(CFG_DUPLICATED_SRC_ADDR_WARN,
                                 channel_p->session_key);

                    /* Remove everything from the channel map */
                    if (cfg_channel_del_map(channel_mgr_p, channel_p)
                        != CFG_CHANNEL_SUCCESS) {
                        syslog_print(CFG_REMOVE_FROM_MAP_WARN,
                                     channel_p->session_key);
                    }
                
                    return CFG_CHANNEL_FAILED_ADD_MAP;
                }

                if (channel_p->fec_stream2.rtcp_port != 0 ) {
                    if (channel_add_map(&channel_mgr_p->channel_map,
                                        channel_p->fec_stream2.multicast_addr,
                                        channel_p->fec_stream2.rtcp_port,
                                        channel_p->handle) == FALSE) {
                        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                                      "combination of multicast address %s "
                                      "and rtcp %d for second FEC stream has "
                                      "been used for another channel",
                                      inet_ntop(AF_INET,
                                                &(channel_p->fec_stream2.multicast_addr),
                                                tmp, INET_ADDRSTRLEN),
                                      ntohs(channel_p->fec_stream2.rtcp_port)); 
                        syslog_print(CFG_DUPLICATED_SRC_ADDR_WARN,
                                     channel_p->session_key);
                        
                        /* Remove everything from the channel map */
                        if (cfg_channel_del_map(channel_mgr_p, channel_p)
                            != CFG_CHANNEL_SUCCESS) {
                            syslog_print(CFG_REMOVE_FROM_MAP_WARN,
                                         channel_p->session_key);
                        }
                
                        return CFG_CHANNEL_FAILED_ADD_MAP;
                    }
                }
            }
        }
    }

    VQE_HASH_MAKE_KEY(&hkey,
                      channel_p->session_key,
                      strlen(channel_p->session_key));
    elem = vqe_hash_elem_create(&hkey, channel_p);
    if (elem == NULL) {
        syslog_print(CFG_CREATE_KEY_ERR, channel_p->session_key);
        return CFG_CHANNEL_FAILED_ADD_MAP;
    }

    if (!MCALL(channel_mgr_p->session_keys, vqe_hash_add_elem, elem)) {
        vqe_hash_elem_destroy(elem);
        syslog_print(CFG_CREATE_KEY_ERR, channel_p->session_key);
        return CFG_CHANNEL_FAILED_ADD_MAP;
    }

    return CFG_CHANNEL_SUCCESS;
}


/* Function:    cfg_channel_del_map
 * Description: Delete a channel structure to the channel maps
 * Parameters:  channel_mgr_p   pointer to the channel manager
 *              channel_p       the channel
 * Returns:     
 */
cfg_channel_ret_e cfg_channel_del_map (channel_mgr_t *channel_mgr_p,
                                       channel_cfg_t *channel_p)
{
    vqe_hash_key_t hkey;
    vqe_hash_elem_t *elem;
    in_port_t rtcp_port = 0;

    if (channel_p == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "cfg_channel_del_map:: Invalid channel pointer.");
        return CFG_CHANNEL_INVALID_CHANNEL;
    }
    else {
        if (channel_p->mode == SOURCE_MODE) {
            rtcp_port = channel_p->re_sourced_rtcp_port;
        }
        else if (channel_p->mode == LOOKASIDE_MODE) {
                rtcp_port = channel_p->original_source_rtcp_port;
        }

        if (channel_remove_map(&channel_mgr_p->channel_map,
                               channel_p->original_source_addr,
                               channel_p->original_source_port,
                               channel_p->handle) == FALSE) {
            syslog_print(CFG_REMOVE_FROM_MAP_WARN, channel_p->session_key);
        }


        if (channel_p->original_source_rtcp_port != 0) {
            if (channel_remove_map(&channel_mgr_p->channel_map,
                                   channel_p->original_source_addr,
                                   channel_p->original_source_rtcp_port,
                                   channel_p->handle) == FALSE) {
                syslog_print(CFG_REMOVE_FROM_MAP_WARN, channel_p->session_key);
            }
        }

        if (channel_p->mode == SOURCE_MODE ||
            channel_p->mode == LOOKASIDE_MODE) {
            if (channel_remove_map(&channel_mgr_p->fbt_map,
                                   channel_p->fbt_address,
                                   rtcp_port,
                                   channel_p->handle) == FALSE) {
                syslog_print(CFG_REMOVE_FROM_MAP_WARN, channel_p->session_key);
            }

            if (channel_remove_map(&channel_mgr_p->fbt_map,
                                   channel_p->fbt_address,
                                   channel_p->rtx_rtp_port,
                                   channel_p->handle) == FALSE) {
                syslog_print(CFG_REMOVE_FROM_MAP_WARN, channel_p->session_key);
            }

            if (channel_remove_map(&channel_mgr_p->fbt_map,
                                   channel_p->fbt_address,
                                   channel_p->rtx_rtcp_port,
                                   channel_p->handle) == FALSE) {
                syslog_print(CFG_REMOVE_FROM_MAP_WARN, channel_p->session_key);
            }
        }

        if (channel_p->fec_enable) {
            if (channel_remove_map(&channel_mgr_p->channel_map,
                                   channel_p->fec_stream1.multicast_addr,
                                   channel_p->fec_stream1.rtp_port,
                                   channel_p->handle) == FALSE) {
                syslog_print(CFG_REMOVE_FROM_MAP_WARN, channel_p->session_key);
            }

            if (channel_p->fec_stream1.rtcp_port != 0) {
                if (channel_remove_map(&channel_mgr_p->channel_map,
                                       channel_p->fec_stream1.multicast_addr,
                                       channel_p->fec_stream1.rtcp_port,
                                       channel_p->handle) == FALSE) {
                    syslog_print(CFG_REMOVE_FROM_MAP_WARN, 
                                 channel_p->session_key);
                }
            }

            if (channel_p->fec_mode == FEC_2D_MODE) {
                if (channel_remove_map(&channel_mgr_p->channel_map,
                                       channel_p->fec_stream2.multicast_addr,
                                       channel_p->fec_stream2.rtp_port,
                                       channel_p->handle) == FALSE) {
                    syslog_print(CFG_REMOVE_FROM_MAP_WARN,
                                 channel_p->session_key);
                }

                if (channel_p->fec_stream2.rtcp_port != 0) {
                    if (channel_remove_map(&channel_mgr_p->channel_map,
                                           channel_p->fec_stream2.multicast_addr,
                                           channel_p->fec_stream2.rtcp_port,
                                           channel_p->handle) == FALSE) {
                        syslog_print(CFG_REMOVE_FROM_MAP_WARN, 
                                     channel_p->session_key);
                    }
                }
            }
        }
    }

    VQE_HASH_MAKE_KEY(&hkey,
                      channel_p->session_key,
                      strlen(channel_p->session_key));
    elem = MCALL(channel_mgr_p->session_keys, vqe_hash_get_elem, &hkey);
    if (elem) {
        if (!MCALL(channel_mgr_p->session_keys, vqe_hash_del_elem, elem)) {
            syslog_print(CFG_DELETE_KEY_ERR, channel_p->session_key);
            return CFG_CHANNEL_FAILURE;
        }
        vqe_hash_elem_destroy(elem);
    }

    return CFG_CHANNEL_SUCCESS;
}


/* Function:    cfg_channel_insert
 * Description: Insert a channel structure to the channel manager
 * Parameters:  channel_mgr_p   pointer to the channel manager
 *              channel_p       the channel configuration data to be inserted
 * Returns:     The new channel configuration in the channel manager
 */
channel_cfg_t *cfg_channel_insert (channel_mgr_t *channel_mgr_p,
                                   channel_cfg_t *new_channel_p)
{
    int i;
    channel_cfg_t *channel_p;

    /* Check whether the channel is exist. If not, add it */
    if (new_channel_p != NULL) {
        /* Create a channel structure */
        channel_p = (channel_cfg_t *) malloc(sizeof(channel_cfg_t));
        if (channel_p == NULL) {
            syslog_print(CFG_MALLOC_ERR);
            return NULL;
        }
        memset(channel_p, 0, sizeof(channel_cfg_t));

        /* Get a handle from the ID manager */
        channel_p->handle = id_get((void *)channel_p,
                                   channel_mgr_p->handle_mgr);
        
        /* Copy all the contents */
        if (cfg_channel_copy(channel_p, new_channel_p)
            != CFG_CHANNEL_SUCCESS) {
            free(channel_p);
            return NULL;
        }
        
        /*Also, add the channel to the channel map */
        if (cfg_channel_add_map(channel_mgr_p, channel_p)
            != CFG_CHANNEL_SUCCESS) {
            syslog_print(CFG_ADD_TO_MAP_WARN, channel_p->session_key);

            /* Remove the handle from the ID manager */
            id_delete(channel_p->handle, channel_mgr_p->handle_mgr);
            free(channel_p);
            return NULL;
        }

        /* Find an open slot, otherwise add at the end */
        for (i = 0; i < channel_mgr_p->total_num_channels; i++) {
            if (channel_mgr_p->handles[i] == ILLEGAL_ID) {
                channel_mgr_p->handles[i] = channel_p->handle;

                return channel_p;
            }
        }

        if (channel_mgr_p->total_num_channels >= MAX_CHANNELS) {
            syslog_print(CFG_REACH_MAX_WARN, MAX_CHANNELS);
            if (cfg_channel_delete(channel_mgr_p, channel_p->handle, TRUE)
                != CFG_CHANNEL_SUCCESS) {
                syslog_print(CFG_DELETE_WARN);
            }

            return NULL;
        }

        channel_mgr_p->handles[channel_mgr_p->total_num_channels]
            = channel_p->handle;
        channel_mgr_p->total_num_channels++;

        return channel_p;
    }
    else {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "cfg_channel_insert:: Null pointer for new channel.");
        return NULL;
    }
}


/* Function:    cfg_channel_get
 * Description: 
 * Parameters:  
 * Returns:     
 */
channel_cfg_t *cfg_channel_get (channel_mgr_t *channel_mgr_p,
                                idmgr_id_t handle)
{
    channel_cfg_t *channel_p;
    id_mgr_ret ret;

    if (handle == ILLEGAL_ID) {
        return NULL;
    }

    /* Retrieval of channel pointer based on the handle */
    channel_p = (channel_cfg_t *)id_to_ptr(handle, &ret,
                                           channel_mgr_p->handle_mgr);

    if (channel_p == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "cfg_channel_get:: Invalid channel 0x%lx.\n",
                      handle);
        return NULL;
    }
    else {
        /* We will return a null pointer if the channel is inactive */
        if (channel_p->active) {
            return channel_p;
        }
        else {
            return NULL;
        }
    }
}


/* Function:    cfg_channel_print
 * Description: prints out the channel params store in channel strucutre
 * Parameters:  channel_cfg_t Channel configuration entry 
 * Returns:     
 */
cfg_channel_ret_e cfg_channel_print (channel_cfg_t *channel_p)
{
    char tmp[INET_ADDRSTRLEN];

    if (channel_p == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "cfg_channel_print:: Invalid channel pointer.");
        return CFG_CHANNEL_INVALID_CHANNEL;
    }
    else {
        printf("Channel name: %s\n", channel_p->name);
        printf("Channel handle: 0x%lx\n", channel_p->handle);
        printf("Channel session identifier: %s\n",
               channel_p->session_key);
        printf("Channel version: %llu\n",
               channel_p->version);
        printf("Configuration data: %s\n",
               (channel_p->complete) ?
               "complete" : "incomplete");

        if (channel_p->mode == SOURCE_MODE) {
            printf("Channel mode: source\n");
        }
        else if (channel_p->mode == LOOKASIDE_MODE) {
            printf("Channel mode: lookaside\n");
        }
        else if (channel_p->mode == DISTRIBUTION_MODE) {
            printf("Channel mode: distribution\n");
        }
        else if (channel_p->mode == RECV_ONLY_MODE) {
            printf("Channel mode: recvonly\n");
        }
        else {
            printf("Channel mode: unknown\n");
        }

        if (channel_p->role == SSM_DS) {
            printf("Channel role: SSM distribution source\n");
        }
        else {
            if (channel_p->role == VAM) {
                printf("Channel role: VAM\n");
            }
            else if (channel_p->role == STB) {
                printf("Channel role: STB\n");
            }
            else {
                printf("Channel role: unknown\n");
            }
        }

        printf("Original source multicast address: %s\n",
               inet_ntop(AF_INET, &channel_p->original_source_addr,
                         tmp, INET_ADDRSTRLEN));
        printf("Source address for original source stream: %s\n",
               inet_ntop(AF_INET, &channel_p->src_addr_for_original_source,
                         tmp, INET_ADDRSTRLEN));
        printf("Original source port: %d\n",
               ntohs(channel_p->original_source_port));

        if (channel_p->source_proto == RTP_STREAM) {
            printf("Original source RTCP port: %d\n",
                   ntohs(channel_p->original_source_rtcp_port));
            printf("Original source RTP payload type: %d\n",
                   channel_p->original_source_payload_type);
            printf("Original source RTCP sender bandwidth: %d\n",
                   channel_p->original_rtcp_sndr_bw);
            printf("Original source RTCP receiver bandwidth: %d\n",
                   channel_p->original_rtcp_rcvr_bw);
            printf("Original source RTCP per receiver bandwidth: %d\n",
                   channel_p->original_rtcp_per_rcvr_bw);
            printf("Original source RTCP XR Loss RLE Report: %s",
                   (channel_p->original_rtcp_xr_loss_rle != 
                    RTCP_RLE_UNSPECIFIED) ? 
                   "On" : "Off");
            if (channel_p->original_rtcp_xr_loss_rle != RTCP_RLE_UNSPECIFIED) {
                printf(" [%d bytes of chunks]\n", 
                       channel_p->original_rtcp_xr_loss_rle);
            }
            else {
                printf("\n");
            }

            printf("Original source RTCP XR Post ER Loss RLE Report: %s",
                   (channel_p->original_rtcp_xr_per_loss_rle != 
                    RTCP_RLE_UNSPECIFIED) ? 
                   "On" : "Off");
            if (channel_p->original_rtcp_xr_per_loss_rle 
                != RTCP_RLE_UNSPECIFIED) {
                printf(" [%d bytes of chunks]\n", 
                       channel_p->original_rtcp_xr_per_loss_rle);
            }
            else {
                printf("\n");
            }

            printf("Original source RTCP XR Stat Summary Report: 0x%04x\n",
                   channel_p->original_rtcp_xr_stat_flags);
            printf("Original source RTCP XR Multicast Acq Report: %s\n",
                   (channel_p->original_rtcp_xr_multicast_acq == TRUE) ?
                   "On" : "Off");
            printf("Original source support for reduced size RTCP : %s\n",
                   (channel_p->original_rtcp_rsize == TRUE) ?
                   "On" : "Off");
            printf("Original source RTCP XR Diagnostic Counters Report: %s\n",
                   (channel_p->original_rtcp_xr_diagnostic_counters == TRUE) ?
                   "On" : "Off");
        }

        printf("Maximum bit rate: %d\n",
               channel_p->bit_rate);
        printf("Feedback Target address: %s\n",
               inet_ntop(AF_INET, &channel_p->fbt_address,
                         tmp, INET_ADDRSTRLEN));

        if (channel_p->mode == SOURCE_MODE) {
            printf("Re-sourced stream multicast address: %s\n",
                   inet_ntop(AF_INET, &channel_p->re_sourced_addr,
                             tmp, INET_ADDRSTRLEN));
            printf("Re-sourced stream RTP port: %d\n",
                   ntohs(channel_p->re_sourced_rtp_port));
            printf("Re-sourced stream RTCP port: %d\n",
                   ntohs(channel_p->re_sourced_rtcp_port));
            printf("Re-sourced stream RTP payload type: %d\n",
                   channel_p->re_sourced_payload_type);
        }

        if (channel_p->mode == SOURCE_MODE ||
            channel_p->mode == LOOKASIDE_MODE) {
            printf("Retransmission RTP: %s\n",
                   inet_ntop(AF_INET, &channel_p->rtx_addr,
                             tmp, INET_ADDRSTRLEN));
            printf("Retransmission RTP port: %d\n",
                   ntohs(channel_p->rtx_rtp_port));
            printf("Retransmission RTCP port: %d\n",
                   ntohs(channel_p->rtx_rtcp_port));
            printf("Retransmission associated payload type: %d\n",
                   channel_p->rtx_apt);
            printf("Retransmission cache time: %d\n",
                   channel_p->rtx_time);

            printf("Repair stream RTCP sender bandwidth: %d\n",
                   channel_p->repair_rtcp_sndr_bw);
            printf("Repair stream RTCP receiver bandwidth: %d\n",
                   channel_p->repair_rtcp_rcvr_bw);

            printf("Repair source RTCP XR Loss RLE Report: %s",
                   (channel_p->repair_rtcp_xr_loss_rle != 
                    RTCP_RLE_UNSPECIFIED) ? 
                   "On" : "Off");
            if (channel_p->repair_rtcp_xr_loss_rle != RTCP_RLE_UNSPECIFIED) {
                printf(" [%d bytes of chunks]\n", 
                       channel_p->repair_rtcp_xr_loss_rle);
            }
            else {
                printf("\n");
            }
            printf("Repair source RTCP XR Stat Summary Report: 0x%04x\n",
                   channel_p->repair_rtcp_xr_stat_flags);

            printf("RTCP unicast feedback mode: %s\n",
                   (channel_p->rtcp_fb_mode == RSI) ?
                   "rsi" : "reflection");
        
            printf("Error repair: %s\n",
                   (channel_p->er_enable) ?
                   "enabled" : "disabled");
            printf("Fast channel change: %s\n",
                   (channel_p->fcc_enable) ?
                   "enabled" : "disabled");
            printf("Forward error correction: %s\n",
                   (channel_p->fec_enable) ?
                   "enabled" : "disabled");
        }

        if (channel_p->fec_enable) {
                printf("FEC stream1 address: %s\n",
                       inet_ntop(AF_INET,
                                 &channel_p->fec_stream1.multicast_addr,
                                 tmp, INET_ADDRSTRLEN));
                printf("Source address for FEC stream1: %s\n",
                       inet_ntop(AF_INET, &channel_p->fec_stream1.src_addr,
                                 tmp, INET_ADDRSTRLEN));
                printf("FEC stream1 RTP port: %d\n",
                       ntohs(channel_p->fec_stream1.rtp_port));
                printf("FEC stream1 RTCP port: %d\n",
                       ntohs(channel_p->fec_stream1.rtcp_port));
                printf("FEC stream1 RTP payload type: %d\n",
                       channel_p->fec_stream1.payload_type);
                printf("FEC stream1 RTCP sender bandwidth: %d\n",
                       channel_p->fec_stream1.rtcp_sndr_bw);
                printf("FEC stream1 RTCP receiver bandwidth: %d\n",
                       channel_p->fec_stream1.rtcp_rcvr_bw);

            if (channel_p->fec_mode == FEC_2D_MODE) {
                printf("FEC stream2 address: %s\n",
                       inet_ntop(AF_INET,
                                 &channel_p->fec_stream2.multicast_addr,
                                 tmp, INET_ADDRSTRLEN));
                printf("Source address for FEC stream2: %s\n",
                       inet_ntop(AF_INET, &channel_p->fec_stream2.src_addr,
                                 tmp, INET_ADDRSTRLEN));
                printf("FEC stream2 RTP port: %d\n",
                       ntohs(channel_p->fec_stream2.rtp_port));
                printf("FEC stream2 RTCP port: %d\n",
                       ntohs(channel_p->fec_stream2.rtcp_port));
                printf("FEC stream2 RTP payload type: %d\n",
                       channel_p->fec_stream2.payload_type);
                printf("FEC stream2 RTCP sender bandwidth: %d\n",
                       channel_p->fec_stream2.rtcp_sndr_bw);
                printf("FEC stream2 RTCP receiver bandwidth: %d\n",
                       channel_p->fec_stream2.rtcp_rcvr_bw);
            }
        }

        printf("\n");
    }

    return CFG_CHANNEL_SUCCESS;
}


/* Function:    cfg_channel_compare
 * Description: Compare two channel contents
 * Parameters:  channel_cfg_t Channel configuration entry 
 * Returns:     
 */
boolean cfg_channel_compare (channel_cfg_t *channel1_p,
                             channel_cfg_t *channel2_p)
{
    if (channel1_p == NULL || channel2_p == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "cfg_channel_compare:: Invalid "
                      "channel pointers %p and %p.",
                      channel1_p, channel2_p);

        return FALSE;
    }
    else {
        if (strcmp(channel1_p->name, channel2_p->name) == 0 &&
            strcmp(channel1_p->session_key, channel2_p->session_key) == 0 &&
            channel1_p->complete == channel2_p->complete &&
            channel1_p->mode == channel2_p->mode &&
            channel1_p->role == channel2_p->role &&
            channel1_p->active == channel2_p->active &&
            channel1_p->source_proto == channel2_p->source_proto &&
            channel1_p->original_source_addr.s_addr
                == channel2_p->original_source_addr.s_addr &&
            channel1_p->original_source_port
                == channel2_p->original_source_port &&
            channel1_p->src_addr_for_original_source.s_addr
                == channel2_p->src_addr_for_original_source.s_addr &&
            channel1_p->original_source_rtcp_port
                == channel2_p->original_source_rtcp_port &&
            channel1_p->original_rtcp_sndr_bw
                == channel2_p->original_rtcp_sndr_bw &&
            channel1_p->original_rtcp_rcvr_bw
                == channel2_p->original_rtcp_rcvr_bw &&
            channel1_p->original_rtcp_per_rcvr_bw
                == channel2_p->original_rtcp_per_rcvr_bw &&
            channel1_p->original_rtcp_xr_loss_rle
                == channel2_p->original_rtcp_xr_loss_rle &&
            channel1_p->original_rtcp_xr_per_loss_rle
                == channel2_p->original_rtcp_xr_per_loss_rle &&
            channel1_p->original_rtcp_xr_stat_flags
                == channel2_p->original_rtcp_xr_stat_flags &&
            channel1_p->original_rtcp_xr_multicast_acq
                == channel2_p->original_rtcp_xr_multicast_acq &&
            channel1_p->original_rtcp_xr_diagnostic_counters
                == channel2_p->original_rtcp_xr_diagnostic_counters &&
            channel1_p->original_source_payload_type
                == channel2_p->original_source_payload_type &&
            channel1_p->original_rtcp_rsize
                == channel2_p->original_rtcp_rsize &&
            channel1_p->bit_rate == channel2_p->bit_rate &&
            channel1_p->re_sourced_addr.s_addr
                == channel2_p->re_sourced_addr.s_addr &&
            channel1_p->re_sourced_rtp_port
                == channel2_p->re_sourced_rtp_port &&
            channel1_p->re_sourced_rtcp_port
                == channel2_p->re_sourced_rtcp_port &&
            channel1_p->re_sourced_payload_type
                == channel2_p->re_sourced_payload_type &&
            channel1_p->fbt_address.s_addr == channel2_p->fbt_address.s_addr &&
            channel1_p->rtx_addr.s_addr == channel2_p->rtx_addr.s_addr &&
            channel1_p->rtx_rtp_port == channel2_p->rtx_rtp_port &&
            channel1_p->rtx_rtcp_port == channel2_p->rtx_rtcp_port &&
            channel1_p->rtx_payload_type == channel2_p->rtx_payload_type &&
            channel1_p->rtx_apt == channel2_p->rtx_apt &&
            channel1_p->rtx_time == channel2_p->rtx_time &&
            channel1_p->repair_rtcp_sndr_bw
                == channel2_p->repair_rtcp_sndr_bw &&
            channel1_p->repair_rtcp_rcvr_bw
                == channel2_p->repair_rtcp_rcvr_bw &&
            channel1_p->repair_rtcp_xr_loss_rle
                == channel2_p->repair_rtcp_xr_loss_rle &&
            channel1_p->repair_rtcp_xr_stat_flags
                == channel2_p->repair_rtcp_xr_stat_flags &&
            channel1_p->er_enable == channel2_p->er_enable &&
            channel1_p->fcc_enable == channel2_p->fcc_enable &&
            channel1_p->rtcp_fb_mode == channel2_p->rtcp_fb_mode) {
            return TRUE;
        }
        else {
            return FALSE;
        }
    }
}


/* Function:    cfg_channel_copy
 * Description: Copy the content from source channel to destination channel
 * Parameters:  channel_cfg_t Channel configuration entry 
 * Returns:     
 */
cfg_channel_ret_e cfg_channel_copy (channel_cfg_t *dest_channel_p,
                                    channel_cfg_t *src_channel_p)
{
    if (src_channel_p == NULL || dest_channel_p == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "cfg_channel_copy:: Invalid channel pointers %p and %p.",
                      src_channel_p, dest_channel_p);

        return CFG_CHANNEL_INVALID_CHANNEL;
    }
    else {
        /* Copy the content from source channel to destination channel */
        strncpy(dest_channel_p->name, src_channel_p->name, SDP_MAX_STRING_LEN);
        strncpy(dest_channel_p->session_key, src_channel_p->session_key,
                MAX_KEY_LENGTH);
        dest_channel_p->version = src_channel_p->version;
        dest_channel_p->complete = src_channel_p->complete;
        dest_channel_p->mode = src_channel_p->mode;
        dest_channel_p->role = src_channel_p->role;
        dest_channel_p->active = src_channel_p->active;

        dest_channel_p->source_proto = src_channel_p->source_proto;
        dest_channel_p->original_source_addr.s_addr
            = src_channel_p->original_source_addr.s_addr;
        dest_channel_p->src_addr_for_original_source.s_addr
            = src_channel_p->src_addr_for_original_source.s_addr;
        dest_channel_p->original_source_port
            = src_channel_p->original_source_port;
        dest_channel_p->original_source_rtcp_port
            = src_channel_p->original_source_rtcp_port;
        dest_channel_p->original_source_payload_type
            = src_channel_p->original_source_payload_type;
        dest_channel_p->bit_rate = src_channel_p->bit_rate;
        dest_channel_p->original_rtcp_sndr_bw
            = src_channel_p->original_rtcp_sndr_bw;
        dest_channel_p->original_rtcp_rcvr_bw
            = src_channel_p->original_rtcp_rcvr_bw;
        dest_channel_p->original_rtcp_per_rcvr_bw
            = src_channel_p->original_rtcp_per_rcvr_bw;
        dest_channel_p->original_rtcp_xr_loss_rle
            = src_channel_p->original_rtcp_xr_loss_rle;
        dest_channel_p->original_rtcp_xr_per_loss_rle
            = src_channel_p->original_rtcp_xr_per_loss_rle;
        dest_channel_p->original_rtcp_xr_stat_flags
            = src_channel_p->original_rtcp_xr_stat_flags;
        dest_channel_p->original_rtcp_xr_multicast_acq
            = src_channel_p->original_rtcp_xr_multicast_acq;
        dest_channel_p->original_rtcp_xr_diagnostic_counters
            = src_channel_p->original_rtcp_xr_diagnostic_counters;

        dest_channel_p->original_rtcp_rsize
            = src_channel_p->original_rtcp_rsize;
        dest_channel_p->re_sourced_addr.s_addr
            = src_channel_p->re_sourced_addr.s_addr;
        dest_channel_p->re_sourced_rtp_port
            = src_channel_p->re_sourced_rtp_port;
        dest_channel_p->re_sourced_rtcp_port
            = src_channel_p->re_sourced_rtcp_port;
        dest_channel_p->re_sourced_payload_type
            = src_channel_p->re_sourced_payload_type;

        dest_channel_p->fbt_address.s_addr = src_channel_p->fbt_address.s_addr;
        dest_channel_p->rtx_addr.s_addr = src_channel_p->rtx_addr.s_addr;
        dest_channel_p->rtx_rtp_port = src_channel_p->rtx_rtp_port;
        dest_channel_p->rtx_rtcp_port = src_channel_p->rtx_rtcp_port;
        dest_channel_p->rtx_payload_type = src_channel_p->rtx_payload_type;
        dest_channel_p->rtx_apt = src_channel_p->rtx_apt;
        dest_channel_p->rtx_time = src_channel_p->rtx_time;
        dest_channel_p->repair_rtcp_sndr_bw
            = src_channel_p->repair_rtcp_sndr_bw;
        dest_channel_p->repair_rtcp_rcvr_bw
            = src_channel_p->repair_rtcp_rcvr_bw;
        dest_channel_p->repair_rtcp_xr_loss_rle
            = src_channel_p->repair_rtcp_xr_loss_rle;
        dest_channel_p->repair_rtcp_xr_stat_flags
            = src_channel_p->repair_rtcp_xr_stat_flags;

        dest_channel_p->er_enable = src_channel_p->er_enable;
        dest_channel_p->fcc_enable = src_channel_p->fcc_enable;
        dest_channel_p->rtcp_fb_mode = src_channel_p->rtcp_fb_mode;
        
        dest_channel_p->fec_enable = src_channel_p->fec_enable;
        dest_channel_p->fec_mode = src_channel_p->fec_mode;
        cp_fec(&dest_channel_p->fec_stream1, &src_channel_p->fec_stream1);
        cp_fec(&dest_channel_p->fec_stream2, &src_channel_p->fec_stream2);
        /* copy fec info */
        memcpy(&dest_channel_p->fec_info, &src_channel_p->fec_info,
               sizeof(fec_info_t));

        dest_channel_p->chksum = src_channel_p->chksum;

        return CFG_CHANNEL_SUCCESS;
    }
}


/* Function:    cfg_channel_destroy_all
 * Description: 
 * Parameters:  
 * Returns:     
 */
cfg_channel_ret_e cfg_channel_destroy_all (channel_mgr_t *channel_mgr_p)
{
    uint16_t i;

    for ( i = 0; i < channel_mgr_p->total_num_channels; i++ ) {
        if (channel_mgr_p->handles[i] != ILLEGAL_ID) {
            if (cfg_channel_delete(channel_mgr_p,
                                   channel_mgr_p->handles[i],
                                   TRUE) != CFG_CHANNEL_SUCCESS) {
                VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                              "cfg_channel_destroy_all:: Failed to "
                              "delete the channel %i.", i);
            }
        }
    }

    channel_mgr_p->total_num_channels = 0;

    return CFG_CHANNEL_SUCCESS;
}

/* Function:    is_exist
 * Description: Check whether the SDP description has already existed
 * Parameters:  sdp_p           The SDP handle
 *              channel_mgr_p   Channel manager
 * Returns:     Channel or NULL
 */
static channel_cfg_t* is_exist (void *sdp_p,
                                channel_mgr_t *channel_mgr_p)
{
    char session_key[MAX_KEY_LENGTH];
    const char *username;
    const char *sessionid;
    const char *creator_addr;
    vqe_hash_key_t hkey;
    vqe_hash_elem_t *elem;

    if (sdp_p == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "is_exist:: Invalid SDP pointer.");
        return NULL;
    }

    /* Create the info from "o=" line to create a globally unique identifier */
    username = sdp_get_owner_username(sdp_p);
    sessionid = sdp_get_owner_sessionid(sdp_p);
    creator_addr = sdp_get_owner_address(sdp_p);

    snprintf(session_key, MAX_KEY_LENGTH, "INIP4#%s#%s#%s",
             username, sessionid, creator_addr);

    VQE_HASH_MAKE_KEY(&hkey, session_key, strlen(session_key));
    elem = MCALL(channel_mgr_p->session_keys, vqe_hash_get_elem, &hkey);

    if (elem) {
        return (channel_cfg_t *)elem->data;
    }
    else {
        return NULL;
    }
}


/* Function:    linear_validation_and_extraction
 * Description: Reads the parsed linear sdp configuration and store
 * Parameters:  sdp_p           SDP handle returned by sdp_init_description.
 *              channel_p       Channel configuration entry 
 * Returns:     True if parsed SDP passes semantic checking, else FALSE
 */
static boolean linear_validation_and_extraction (void *sdp_p,
                                                 channel_cfg_t *channel_p)
{
    const char *username;
    const char *sessionid;
    const char *version;
    const char *creator_addr;
    const char *session_name;
    const char *start_time;
    const char *stop_time;
    uint64_t time_value;

    uint16_t i, j, k;
    uint16_t num_sessions;
    uint16_t media_session_idx;
    session_type_e session_type;
    char conn_address[SDP_MAX_STRING_LEN+1];
    sdp_payload_ind_e indicator;
    uint16_t payload_type;
    uint16_t pt;

    uint16_t num_attributes;
    sdp_attr_e attr_type;
    uint16_t num_instances;
    char src_address[SDP_MAX_STRING_LEN+1];
    char src_address_for_resourced_stream[SDP_MAX_STRING_LEN+1];
    struct in_addr src_addr_p;
    char dest_address[SDP_MAX_STRING_LEN+1];
    uint32_t rtp_port;
    uint32_t rtcp_port;
    char nack_param[SDP_MAX_STRING_LEN+1];

    char attr[MAX_SESSIONS][ATTR_LEN+1];
    uint16_t num_group_id;
    char group_id[MAX_SESSIONS][SDP_MAX_STRING_LEN+1];
    char group_label[GROUP_SIZE];
    char message_buffer[MAX_MSG_LENGTH];

    boolean found_src_filter_in_orig_source = FALSE;
    boolean found_src_filter_in_re_sourced = FALSE;
    boolean found_src_filter_in_fec_column = FALSE;
    boolean found_src_filter_in_fec_row = FALSE;

    boolean is_session_active;

    uint16_t loss_rle;
    uint16_t per_loss_rle;
    uint32_t stat_flags;

    if (sdp_p == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "linear_validation_and_extraction:: "
                      "Invalid SDP pointer.");
        return FALSE;
    }

    if (channel_p == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "linear_validation_and_extraction:: "
                      "Invalid channel pointer.");
        return FALSE;
    }

    /* Get the "o=" line to create a globally unique identifier */
    username = sdp_get_owner_username(sdp_p);
    sessionid = sdp_get_owner_sessionid(sdp_p);
    version = sdp_get_owner_version(sdp_p);
    creator_addr = sdp_get_owner_address(sdp_p);

    snprintf(channel_p->session_key, MAX_KEY_LENGTH, "INIP4#%s#%s#%s",
             username, sessionid, creator_addr);

    channel_p->version = atoll(version);
    if (strlen(version) >= 20 ||
        channel_p->version > MAX_VERSION_ALLOWED) {
        snprintf(message_buffer, MAX_MSG_LENGTH,
                 "reaching the maximum version number (%llu) allowed "
                 "in o= line. " CHECK_DOCUMENT, MAX_VERSION_ALLOWED);
        syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                     message_buffer);
        return FALSE;
    }

    /* Check the network type and address type */
    /* For now we only support IN and IP4 */
    if (sdp_get_owner_network_type(sdp_p) != SDP_NT_INTERNET ||
        sdp_get_owner_address_type(sdp_p) != SDP_AT_IP4) {
        syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                     "an unsupported network type or address type. "
                     "Currently only IN and IP4 are allowed.");
        return FALSE;
    }

    /* Get the name of the SDP session */
    session_name = (char *) sdp_get_session_name(sdp_p);
    strncpy(channel_p->name, session_name, SDP_MAX_STRING_LEN);

    /* 
     * Get the time description
     *
     * For VAM network element, we require that
     * time description must be either t=0 0 or
     * t=0 <NTP stop time>.
     */
    start_time = sdp_get_time_start(sdp_p);
    stop_time = sdp_get_time_stop(sdp_p);
    if (strcmp(start_time, "0") == 0) {
        if (strcmp(stop_time, "0") == 0) {
            channel_p->active = TRUE;
        }
        else {
            channel_p->active = FALSE;
            time_value = atoll(stop_time);
            if (time_value
                < abs_time_to_sec(ntp_to_abs_time(get_ntp_time()))) {
                syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                             "a past-time value specified in stop time "
                             "of t= line. " CHECK_DOCUMENT 
                             " The channel will be deleted immediately.");
            }
            else {
                syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                             "a future time value specified in stop time "
                             "of t= line. " CHECK_DOCUMENT 
                             " However, the channel will be deleted "
                             "immediately.");
            }
        }
    }
    else {
        syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                     "an unsupported non-zero starting time in t= line. "
                     CHECK_DOCUMENT);
        return FALSE;
    }

    /* Process all the media sessions */
    num_sessions = sdp_get_num_media_lines(sdp_p);
    if (num_sessions < MIN_LINEAR_SESSIONS) {
        snprintf(message_buffer, MAX_MSG_LENGTH,
                 "%d missing media session(s) [minimum %d sessions are "
                 "required.] " CHECK_DOCUMENT, 
                 MIN_LINEAR_SESSIONS-num_sessions, MIN_LINEAR_SESSIONS);
        syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                     message_buffer);
        return FALSE;
    }
        
    channel_p->role = UNSPECIFIED_ROLE;
    channel_p->mode = UNSPECIFIED_MODE;

    /* Set rtcp bandwidth to invalid first */
    channel_p->original_rtcp_sndr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->original_rtcp_rcvr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->original_rtcp_per_rcvr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->re_sourced_rtcp_sndr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->re_sourced_rtcp_rcvr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->repair_rtcp_sndr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->repair_rtcp_rcvr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->fec_stream1.rtcp_sndr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->fec_stream1.rtcp_rcvr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->fec_stream2.rtcp_sndr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->fec_stream2.rtcp_rcvr_bw = RTCP_BW_UNSPECIFIED;

    /* Set rtcp loss RLE value to invalid first, it will be overwritten
       by the maximum size specifed in SDP */
    channel_p->original_rtcp_xr_loss_rle = RTCP_RLE_UNSPECIFIED;
    channel_p->original_rtcp_xr_per_loss_rle = RTCP_RLE_UNSPECIFIED;
    channel_p->repair_rtcp_xr_loss_rle = RTCP_RLE_UNSPECIFIED;

    /* Set up all the sessions as inactive first */
    for (session_type=0; session_type< MAX_SESSIONS; session_type++) {
        strncpy(attr[session_type], INACTIVE_STR, ATTR_LEN);
    }
    
    /* Create the default addresses for each stream so if we need to */
    /* write them out it will not be rejected by SDP parser afterwards */
    channel_p->original_source_addr.s_addr = inet_addr("239.255.255.255");
    channel_p->re_sourced_addr.s_addr = inet_addr("239.255.255.255");
    channel_p->rtx_addr.s_addr = inet_addr("1.1.1.1");
    channel_p->fec_stream1.multicast_addr.s_addr =
        inet_addr("239.255.255.255");
    channel_p->fec_stream2.multicast_addr.s_addr =
        inet_addr("239.255.255.255");

    /* Set non-zero values to make it backward compatible */
    channel_p->re_sourced_rtp_port = 1;
    channel_p->rtx_rtp_port = 1;

    for (i = 0; i < num_sessions; i++) {
        media_session_idx = i + 1;
        session_type = MAX_SESSIONS;

        /* First, check whether the a= attribute is inactive or not */
        /* If it is inactive, skip all the checking on this session */
        is_session_active = FALSE;
        if (sdp_get_total_attrs(sdp_p, media_session_idx, 0, &num_attributes)
            != SDP_SUCCESS) {
            snprintf(message_buffer, MAX_MSG_LENGTH,
                     "failing to get the total number of attributes "
                     "in media session %d.",
                     media_session_idx);
            syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                         message_buffer);
            return FALSE;
        }

        for (j= 0; j < num_attributes; j++) {
            if (sdp_get_attr_type(sdp_p,
                                  media_session_idx,
                                  0,
                                  j+1,
                                  &attr_type,
                                  &num_instances) != SDP_SUCCESS) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "failing to get attribute type in media session %d.",
                         media_session_idx);
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             message_buffer);
                return FALSE;
            }

            switch (attr_type) {
                case SDP_ATTR_SENDONLY:
                case SDP_ATTR_RECVONLY:
                    is_session_active = TRUE;

                    break;

                default:
                    break;
            }
        }

        /* However, because we throw out the entire session, check whether */
        /* this is original stream with UDP protocol. If so, we must get */
        /* multicast address and port for original stream from this session */
        /* to support source mode case for STB */
        if (sdp_is_mcast_addr(sdp_p, media_session_idx) &&
            sdp_get_media_transport(sdp_p, media_session_idx)
            == SDP_TRANSPORT_UDP) {
            /* Get the connection address and destination port */
            strncpy(conn_address,
                    sdp_get_conn_address(sdp_p, media_session_idx),
                    SDP_MAX_STRING_LEN);
            if (is_valid_ip_address(conn_address) == FALSE) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "an illegal connection address %s in c= line of "
                         "original media session. " CHECK_DOCUMENT,
                         conn_address);
                syslog_print(CFG_VALIDATION_ERR,
                             channel_p->session_key,
                             message_buffer);
                return FALSE;
            }

            rtp_port = sdp_get_media_portnum(sdp_p, media_session_idx);
            if (rtp_port < MIN_PORT_ALLOWED || rtp_port > USHRT_MAX) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "an illegal RTP port number %d in m= line of "
                         "original media session. " CHECK_DOCUMENT,
                         rtp_port);
                syslog_print(CFG_VALIDATION_ERR,
                             channel_p->session_key,
                             message_buffer);
                return FALSE;
            }

            channel_p->original_source_addr.s_addr = inet_addr(conn_address);
            channel_p->original_source_port = htons(rtp_port);
            channel_p->bit_rate = get_bit_rate(sdp_p, media_session_idx);
            if (channel_p->bit_rate == -1) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "AS bandwidth being missing in original stream. "
                         CHECK_DOCUMENT);
                syslog_print(CFG_VALIDATION_ERR,
                             channel_p->session_key,
                             message_buffer);
                return FALSE;
            }
            if (channel_p->bit_rate < MIN_BIT_RATE_ALLOWED) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "AS bandwidth for media session %d "
                         "less than the min value %d required. "
                         CHECK_DOCUMENT, media_session_idx,
                         MIN_BIT_RATE_ALLOWED);
                syslog_print(CFG_VALIDATION_ERR,
                             channel_p->session_key,
                             message_buffer);
                return FALSE;
            }

            if (channel_p->bit_rate > MAX_BIT_RATE_ALLOWED) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "AS bandwidth for media session %d "
                         "exceeding the max value %d. "
                         CHECK_DOCUMENT, media_session_idx, 
                         MAX_BIT_RATE_ALLOWED);
                syslog_print(CFG_VALIDATION_ERR,
                             channel_p->session_key,
                             message_buffer);
                return FALSE;
            }

            /* Convert to bps from kbps */
            channel_p->bit_rate *= 1000;
        }

        /* Skip the rest of checking if the statement is TRUE */
        if (is_session_active == FALSE) {
            continue;
        }

        /* Try to figure out which media session it is */
        session_type = figure_out_session_type(sdp_p,
                                               media_session_idx,
                                               channel_p);
        /* Skip the unknown session */
        if (session_type == MAX_SESSIONS) {
            continue;
        }

        if (session_type == ORIGINAL_STREAM) {
            if (sdp_get_media_transport(sdp_p, media_session_idx)
                == SDP_TRANSPORT_UDP) {
                channel_p->source_proto = UDP_STREAM;
            }
            else {
                channel_p->source_proto = RTP_STREAM;
            }
        }

        /* Get the payload type */
        if (sdp_get_media_num_payload_types(sdp_p, media_session_idx) == 1) {
            payload_type = sdp_get_media_payload_type(sdp_p,
                                                      media_session_idx,
                                                      1,
                                                      &indicator);

            if ((payload_type < MIN_PT && payload_type != STATIC_PT) ||
                (payload_type > MAX_PT)) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "m= line of media session %d having invalid "
                         "payload type %d. " CHECK_DOCUMENT,
                         media_session_idx, payload_type);
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             message_buffer);
                return FALSE;
            }
        }
        else {
            snprintf(message_buffer, MAX_MSG_LENGTH,
                     "m= line of media session %d having more than "
                     "one payload type "
                     "specified. Currently, only one stream per session "
                     "is allowed.",
                     media_session_idx);
            syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                         message_buffer);
            return FALSE;
        }

        /* Get the bandwidth */
        uint32_t bw_value;
        sdp_bw_modifier_e mod;
        int total_bw_lines = sdp_get_num_bw_lines(sdp_p, media_session_idx);
        switch (session_type) {
            case ORIGINAL_STREAM:
                if (total_bw_lines == 0) {
                    snprintf(message_buffer, MAX_MSG_LENGTH,
                             "original source stream having no bandwidth "
                             "line specified. b=AS:<> line must be present.");
                    syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                                 message_buffer);
                    return FALSE;
                }

                break;
                                
            case RE_SOURCED_STREAM:
            case UNICAST_RTX_STREAM:
            case FEC_FIRST_STREAM:
            case FEC_SECOND_STREAM:

                break;
                
            case MAX_SESSIONS:
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "an unsupported stream type in media session %d.",
                         media_session_idx);
                syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                             message_buffer);

                break;
        }

        for (j = 0; j < total_bw_lines; j++) {
            bw_value = sdp_get_bw_value(sdp_p,
                                        media_session_idx,
                                        j);
            if (bw_value > MAX_BW_ALLOWED) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "bandwidth for media session %d "
                         "exceeding the max value %d. " CHECK_DOCUMENT,
                         media_session_idx, MAX_BW_ALLOWED);
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             message_buffer);
                return FALSE;
            }
            mod = sdp_get_bw_modifier(sdp_p, media_session_idx, j);
            switch(mod) {
                case SDP_BW_MODIFIER_AS:
                    if (session_type == ORIGINAL_STREAM) {
                        if (bw_value < MIN_BIT_RATE_ALLOWED) {
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "AS bandwidth in original media session "
                                     "less than the min value %d required. "
                                     CHECK_DOCUMENT, MIN_BIT_RATE_ALLOWED);
                            syslog_print(CFG_VALIDATION_ERR,
                                         channel_p->session_key,
                                         message_buffer);
                            return FALSE;
                        }

                        if (bw_value > MAX_BIT_RATE_ALLOWED) {
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "AS bandwidth in original media session "
                                     "exceeding the max value %d. "
                                     CHECK_DOCUMENT, MAX_BIT_RATE_ALLOWED);
                            syslog_print(CFG_VALIDATION_ERR,
                                         channel_p->session_key,
                                         message_buffer);
                            return FALSE;
                        }

                        channel_p->bit_rate = bw_value * 1000;
                    }
                    else {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "an AS bandwidth specification "
                                 "in media session %d other than original "
                                 "media session.",
                                 media_session_idx);
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     message_buffer);
                        return FALSE;
                    }

                    break;

                case SDP_BW_MODIFIER_RS:
                    switch (session_type) {
                        case ORIGINAL_STREAM:
                            channel_p->original_rtcp_sndr_bw = bw_value;
                            break;

                        case RE_SOURCED_STREAM:
                            channel_p->re_sourced_rtcp_sndr_bw = bw_value;
                            break;

                        case UNICAST_RTX_STREAM:
                            channel_p->repair_rtcp_sndr_bw = bw_value;
                            break;

                        case FEC_FIRST_STREAM:
                            channel_p->fec_stream1.rtcp_sndr_bw = bw_value;
                            break;

                        case FEC_SECOND_STREAM:
                            channel_p->fec_stream2.rtcp_sndr_bw = bw_value;
                            break;

                        default:
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "RS bandwidth specified in "
                                     "unknown media session %d.",
                                     media_session_idx);
                            syslog_print(CFG_VALIDATION_WARN, message_buffer);
                    }
                     
                    break;

                case SDP_BW_MODIFIER_RR:
                    switch (session_type) {
                        case ORIGINAL_STREAM:
                            channel_p->original_rtcp_rcvr_bw = bw_value;
                            break;

                        case RE_SOURCED_STREAM:
                            channel_p->re_sourced_rtcp_rcvr_bw = bw_value;
                            break;

                        case UNICAST_RTX_STREAM:
                            channel_p->repair_rtcp_rcvr_bw = bw_value;
                            break;

                        case FEC_FIRST_STREAM:
                            channel_p->fec_stream1.rtcp_rcvr_bw = bw_value;
                            break;

                        case FEC_SECOND_STREAM:
                            channel_p->fec_stream2.rtcp_rcvr_bw = bw_value;
                            break;

                        default:
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "RR bandwidth specified in "
                                     "unknown media session %d.",
                                     media_session_idx);
                            syslog_print(CFG_VALIDATION_WARN, message_buffer);
                    }

                    break;

                default:
                    snprintf(message_buffer, MAX_MSG_LENGTH,
                             "an unsupported bandwidth modifier "
                             "in media session %d. "
                             CHECK_DOCUMENT, media_session_idx);
                    syslog_print(CFG_VALIDATION_WARN,
                                 channel_p->session_key,
                                 message_buffer);
            }
        }

        /* Get the connection address and destination port */
        if (!sdp_is_mcast_addr(sdp_p, media_session_idx)) {
            switch (session_type) {
                case ORIGINAL_STREAM:
                case RE_SOURCED_STREAM:
                case FEC_FIRST_STREAM:
                case FEC_SECOND_STREAM:
                    snprintf(message_buffer, MAX_MSG_LENGTH,
                             "unicast address in media session %d. "
                             CHECK_DOCUMENT, media_session_idx);
                    syslog_print(CFG_VALIDATION_ERR,
                                 channel_p->session_key,
                                 message_buffer);
                    return FALSE;
                case UNICAST_RTX_STREAM:
                default:
                    break;
            }
        }                        
        strncpy(conn_address,
                sdp_get_conn_address(sdp_p, media_session_idx),
                SDP_MAX_STRING_LEN);
        if (is_valid_ip_address(conn_address) == FALSE) {
            snprintf(message_buffer, MAX_MSG_LENGTH,
                     "an illegal connection address %s in c= line of "
                     "media session %d. " CHECK_DOCUMENT,
                     conn_address, media_session_idx);
            syslog_print(CFG_VALIDATION_ERR,
                         channel_p->session_key,
                         message_buffer);
            return FALSE;
        }

        rtp_port = sdp_get_media_portnum(sdp_p, media_session_idx);
        if (rtp_port < MIN_PORT_ALLOWED || rtp_port > USHRT_MAX) {
            snprintf(message_buffer, MAX_MSG_LENGTH,
                     "an illegal RTP port number %d in m= line of "
                     "media session %d. " CHECK_DOCUMENT,
                     rtp_port, media_session_idx);
            syslog_print(CFG_VALIDATION_ERR,
                         channel_p->session_key,
                         message_buffer);
            return FALSE;
        }

        switch (session_type) {
            case ORIGINAL_STREAM:
                channel_p->original_source_addr.s_addr
                    = inet_addr(conn_address);
                channel_p->original_source_port = htons(rtp_port);
                channel_p->original_source_payload_type = payload_type;
                break;

            case RE_SOURCED_STREAM:
                channel_p->re_sourced_addr.s_addr = inet_addr(conn_address);
                channel_p->re_sourced_rtp_port = htons(rtp_port);
                channel_p->re_sourced_payload_type = payload_type;
                break;

            case UNICAST_RTX_STREAM:
                if (is_valid_ip_host_addr(conn_address,
                                          &(channel_p->rtx_addr.s_addr)) 
                    == FALSE) {
                    snprintf(message_buffer, MAX_MSG_LENGTH,
                             "an illegal connection address %s in "
                             "c= line of unicast retransmission stream. "
                             CHECK_DOCUMENT, conn_address);
                    syslog_print(CFG_VALIDATION_ERR,
                                 channel_p->session_key,
                                 message_buffer);
                    return FALSE;
                }
                channel_p->rtx_addr.s_addr = inet_addr(conn_address);
                channel_p->rtx_rtp_port = htons(rtp_port);
                channel_p->rtx_payload_type = payload_type;
                break;

            case FEC_FIRST_STREAM:
                channel_p->fec_stream1.multicast_addr.s_addr
                    = inet_addr(conn_address);
                channel_p->fec_stream1.rtp_port = htons(rtp_port);
                channel_p->fec_stream1.payload_type = payload_type;
                break;

            case FEC_SECOND_STREAM:
                channel_p->fec_stream2.multicast_addr.s_addr
                    = inet_addr(conn_address);
                channel_p->fec_stream2.rtp_port = htons(rtp_port);
                channel_p->fec_stream2.payload_type = payload_type;
                break;

            case MAX_SESSIONS:
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "an unsupported stream type in media session %d.",
                         media_session_idx);
                syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                             message_buffer);
        }

        /* Get the rest of attributes */
        group_id[session_type][0] = '\0';
        for (j = 0; j < num_attributes; j++) {
            if (sdp_get_attr_type(sdp_p,
                                  media_session_idx,
                                  0,
                                  j+1,
                                  &attr_type,
                                  &num_instances) != SDP_SUCCESS) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "failing to get attribute type in media session %d.",
                         media_session_idx);
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             message_buffer);
                return FALSE;
            }

            switch (attr_type) {
                case SDP_ATTR_SENDONLY:
                    strncpy(attr[session_type], SENDONLY_STR, ATTR_LEN);
                    break;

                case SDP_ATTR_RECVONLY:
                    strncpy(attr[session_type], RECVONLY_STR, ATTR_LEN);
                    break;

                case SDP_ATTR_INACTIVE:
                    strncpy(attr[session_type], INACTIVE_STR, ATTR_LEN);
                    break;

                case SDP_ATTR_SOURCE_FILTER:
                    /* Check the source filter mode */
                    if (sdp_get_source_filter_mode(sdp_p,
                                                   media_session_idx,
                                                   0,
                                                   num_instances)
                        == SDP_SRC_FILTER_INCL) {
                        if (sdp_get_filter_destination_attributes(
                                sdp_p,
                                media_session_idx,
                                0,
                                num_instances,
                                NULL,
                                NULL,
                                dest_address)
                            != SDP_SUCCESS) {
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "failing to get filter destination "
                                     "address in media session %d.",
                                     media_session_idx);
                            syslog_print(CFG_VALIDATION_ERR,
                                         channel_p->session_key,
                                         message_buffer);
                            return FALSE;
                        }
                        
                        /* Make sure that the address is a valid one */
                        if (is_valid_ip_address(dest_address) == FALSE) {
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "an illegal multicast address %s "
                                     "in a=source-filter line of media "
                                     "session %d. " CHECK_DOCUMENT,
                                     dest_address, media_session_idx);
                            syslog_print(CFG_VALIDATION_ERR,
                                         channel_p->session_key,
                                         message_buffer);
                            return FALSE;
                        }

                        /* The destination address in the filter should */
                        /* be equal to the address in the connection line */
                        if (strcmp(conn_address, dest_address) == 0 &&
                            sdp_get_filter_source_address_count(
                                sdp_p,
                                media_session_idx,
                                0,
                                num_instances) == 1) {
                            if (sdp_get_filter_source_address(
                                    sdp_p,
                                    media_session_idx,
                                    0,
                                    num_instances,
                                    0,
                                    src_address) != SDP_SUCCESS) {
                                snprintf(message_buffer, MAX_MSG_LENGTH,
                                         "failing to get filter source "
                                         "address in media session %d.",
                                         media_session_idx);
                                syslog_print(CFG_VALIDATION_ERR,
                                             channel_p->session_key,
                                             message_buffer);
                                return FALSE;
                            }

                            if (is_valid_ip_host_addr(src_address,
                                                      &src_addr_p.s_addr)
                                == FALSE) {
                                snprintf(message_buffer, MAX_MSG_LENGTH,
                                         "an illegal src address %s in "
                                         "a=source-filter line of media "
                                         "session %d. " CHECK_DOCUMENT,
                                         src_address, media_session_idx);
                                syslog_print(CFG_VALIDATION_ERR, 
                                             channel_p->session_key,
                                             message_buffer);
                                return FALSE;
                            }

                            if (session_type == ORIGINAL_STREAM) {
                                channel_p->src_addr_for_original_source.s_addr
                                    = inet_addr(src_address);
                                found_src_filter_in_orig_source = TRUE;
                            }
                            else if (session_type == RE_SOURCED_STREAM) {
                                strncpy(src_address_for_resourced_stream,
                                        src_address,
                                        SDP_MAX_STRING_LEN);
                                found_src_filter_in_re_sourced = TRUE;
                            }
                            else if (session_type == FEC_FIRST_STREAM) {
                                channel_p->fec_stream1.src_addr.s_addr
                                    = inet_addr(src_address);
                                found_src_filter_in_fec_column = TRUE;
                            }
                            else if (session_type == FEC_SECOND_STREAM) {
                                channel_p->fec_stream2.src_addr.s_addr
                                    = inet_addr(src_address);
                                found_src_filter_in_fec_row = TRUE;
                            }
                        }
                        else {
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "multicast address in a=source-filter "
                                     "line not matching the connection "
                                     "address in media session %d.",
                                     media_session_idx);
                            syslog_print(CFG_VALIDATION_ERR,
                                         channel_p->session_key,
                                         message_buffer);
                            return FALSE;
                        }
                        
                    }
                    else {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "an unsupported source filter mode "
                                 "in media session %d.",
                                 media_session_idx);
                        syslog_print(CFG_VALIDATION_ERR, 
                                     channel_p->session_key,
                                     message_buffer);
                        return FALSE;
                    }

                    break;

                case SDP_ATTR_RTPMAP:
                    /* Cross checking the payload type */
                    payload_type = sdp_attr_get_rtpmap_payload_type(
                        sdp_p,
                        media_session_idx,
                        0,
                        num_instances);
                    if ((payload_type < MIN_PT && payload_type != STATIC_PT) ||
                        (payload_type > MAX_PT)) {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "media session %d having invalid payload "
                                 "type %d in a=rtpmap:<> line. "
                                 CHECK_DOCUMENT,
                                 media_session_idx, payload_type);
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     message_buffer);
                        return FALSE;
                    }

                    pt = 0;
                    switch (session_type) {
                        case ORIGINAL_STREAM:
                            pt = channel_p->original_source_payload_type;
                            break;

                        case RE_SOURCED_STREAM:
                            pt = channel_p->re_sourced_payload_type;
                            break;

                        case UNICAST_RTX_STREAM:
                            pt = channel_p->rtx_payload_type;
                            break;

                        case FEC_FIRST_STREAM:
                            pt = channel_p->fec_stream1.payload_type;
                            break;

                        case FEC_SECOND_STREAM:
                            pt = channel_p->fec_stream2.payload_type;
                            break;

                        case MAX_SESSIONS:
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "an unsupported stream type "
                                     "in media session %d.",
                                     media_session_idx);
                            syslog_print(CFG_VALIDATION_WARN,
                                         channel_p->session_key,
                                         message_buffer);
                            break;
                    }

                    if (pt != payload_type) {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "mismatched payload type "
                                 "in media session %d. "
                                 "Check m= line and a=rtpmap line.",
                                 media_session_idx);
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     message_buffer);
                        return FALSE;
                    }

                    break;

                case SDP_ATTR_RTCP:
                    rtcp_port = sdp_get_rtcp_port(sdp_p,
                                                  media_session_idx,
                                                  0,
                                                  num_instances);

                    if (rtcp_port < MIN_PORT_ALLOWED || 
                        rtcp_port > USHRT_MAX) {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "an illegal RTCP port number %d in "
                                 "a=rtcp line of media session %d. "
                                 CHECK_DOCUMENT,
                                 rtcp_port, media_session_idx);
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     message_buffer);
                        return FALSE;
                    }

                    if (sdp_get_rtcp_conn_address(sdp_p,
                                                  media_session_idx,
                                                  0,
                                                  num_instances,
                                                  NULL,
                                                  NULL,
                                                  dest_address)
                        != SDP_SUCCESS) {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "failing to get connection "
                                 "address for RTCP in media session %d.",
                                 media_session_idx);
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     message_buffer);
                        return FALSE;
                    }
                        
                    /* Make sure that the address is a valid one */
                    if (dest_address[0] != '\0' &&
                        is_valid_ip_host_addr(dest_address,
                                              &src_addr_p.s_addr) == FALSE) {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "an illegal address %s "
                                 "in a=rtcp line of media "
                                 "session %d. " CHECK_DOCUMENT,
                                 dest_address, media_session_idx);
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     message_buffer);
                        return FALSE;
                    }

                    switch (session_type) {
                        case ORIGINAL_STREAM:
                            channel_p->original_source_rtcp_port
                                = htons(rtcp_port);
                            if (dest_address[0] != '\0') {
                                channel_p->fbt_address.s_addr
                                    = inet_addr(dest_address);
                            }

                            break;

                        case RE_SOURCED_STREAM:
                            channel_p->re_sourced_rtcp_port = htons(rtcp_port);
                            if (dest_address[0] != '\0') {
                                channel_p->fbt_address.s_addr
                                    = inet_addr(dest_address);
                            }
                            break;

                        case UNICAST_RTX_STREAM:
                            channel_p->rtx_rtcp_port = htons(rtcp_port);
                            if (dest_address[0] != '\0' &&
                                channel_p->rtx_addr.s_addr != 
                                inet_addr(dest_address)) {
                                snprintf(message_buffer, MAX_MSG_LENGTH,
                                         "an unmatched address %s "
                                         "in a=rtcp line of media "
                                         "session %d from c= line. " 
                                         CHECK_DOCUMENT,
                                         dest_address, media_session_idx);
                                syslog_print(CFG_VALIDATION_WARN,
                                             channel_p->session_key,
                                             message_buffer);
                            }
                            break;

                        case FEC_FIRST_STREAM:
                            channel_p->fec_stream1.rtcp_port = htons(rtcp_port);
                            break;

                        case FEC_SECOND_STREAM:
                            channel_p->fec_stream2.rtcp_port = htons(rtcp_port);
                            break;

                        case MAX_SESSIONS:
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "an unsupported stream type "
                                     "in media session %d.",
                                     media_session_idx);
                            syslog_print(CFG_VALIDATION_WARN,
                                         channel_p->session_key,
                                         message_buffer);
                            break;
                    }

                    break;

                case SDP_ATTR_RTCP_FB:
                    if (session_type != UNICAST_RTX_STREAM ||
                        session_type != FEC_FIRST_STREAM ||
                        session_type != FEC_SECOND_STREAM) {
                        payload_type = 0;
                        if (sdp_get_rtcp_fb_nack_param(sdp_p,
                                                       media_session_idx,
                                                       0,
                                                       num_instances,
                                                       &payload_type,
                                                       nack_param)
                            != SDP_SUCCESS) {
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "failing to get RTCP feedback parameters "
                                     "in media session %d.",
                                     media_session_idx);
                            syslog_print(CFG_VALIDATION_ERR, 
                                         channel_p->session_key,
                                         message_buffer);
                            return FALSE;
                        }

                        switch (session_type) {
                            case ORIGINAL_STREAM:
                                /* Cross checking the payload type first */
                                if (channel_p->original_source_payload_type
                                    != payload_type) {
                                    snprintf(message_buffer, MAX_MSG_LENGTH,
                                             "mismatched payload type "
                                             "in original media session. "
                                             "Check m= line and a=rtcp-fb line.");
                                    syslog_print(CFG_VALIDATION_ERR,
                                                 channel_p->session_key,
                                                 message_buffer);
                                    return FALSE;
                                }
                                break;

                            case RE_SOURCED_STREAM:
                                if (channel_p->re_sourced_payload_type
                                    != payload_type) {
                                    snprintf(message_buffer, MAX_MSG_LENGTH,
                                             "mismatched payload type "
                                             "in re-sourced media session. "
                                             "Check m= line and a=rtcp-fb line.");
                                    syslog_print(CFG_VALIDATION_ERR,
                                                 channel_p->session_key,
                                                 message_buffer);
                                    return FALSE;
                                }
                                break;

                            case UNICAST_RTX_STREAM:
                            case FEC_FIRST_STREAM:
                            case FEC_SECOND_STREAM:
                                break;

                            case MAX_SESSIONS:
                                snprintf(message_buffer, MAX_MSG_LENGTH,
                                         "an unsupported stream type "
                                         "in media session %d.",
                                         media_session_idx);
                                syslog_print(CFG_VALIDATION_WARN,
                                             channel_p->session_key,
                                             message_buffer);
                                break;
                        }
                        
                        if (strcmp(nack_param, "") == 0) {
                            channel_p->er_enable = TRUE;
                        }

                        if (strcmp(nack_param, "pli") == 0) {
                            channel_p->fcc_enable = TRUE;
                        }
                    }
                    else {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "an illegal RTCP feedback specified in "
                                 "media session %d.",
                                 media_session_idx);
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     message_buffer);
                        return FALSE;
                    }

                    break;

                case SDP_ATTR_MID:
                    strncpy(group_id[session_type],
                            sdp_attr_get_simple_string(sdp_p,
                                                       SDP_ATTR_MID,
                                                       media_session_idx,
                                                       0,
                                                       num_instances),
                            SDP_MAX_STRING_LEN);

                    break;

                case SDP_ATTR_FMTP:
                    if (session_type == UNICAST_RTX_STREAM) {
                        for (k = 1; k < num_instances+1; k++) {
                            /* Check apt value exists and corresponds */
                            /* to the primary payload number */
                            if (sdp_attr_fmtp_get_fmtp_format(
                                sdp_p,
                                media_session_idx,
                                0,
                                k) == SDP_FMTP_RTP_RETRANS) {
                                /* Cross checking the payload type first */
                                payload_type = sdp_attr_get_fmtp_payload_type(
                                    sdp_p,
                                    media_session_idx,
                                    0,
                                    k);
                                if (channel_p->rtx_payload_type
                                    != payload_type) {
                                    snprintf(message_buffer, MAX_MSG_LENGTH,
                                             "mismatched payload type "
                                             "in unicast retransmission "
                                             "session. "
                                             "Check m= line and a=fmtp line.");
                                    syslog_print(CFG_VALIDATION_ERR,
                                                 channel_p->session_key,
                                                 message_buffer);
                                    return FALSE;
                                }
                                
                                if (sdp_attr_get_fmtp_apt(sdp_p,
                                                          media_session_idx,
                                                          0, k) > 0) {
                                    channel_p->rtx_apt = sdp_attr_get_fmtp_apt(
                                        sdp_p,
                                        media_session_idx,
                                        0,
                                        k);
                                }
                                
                                if (sdp_attr_get_fmtp_rtx_time(
                                        sdp_p,
                                        media_session_idx, 0, k) > 0) {
                                    channel_p->rtx_time
                                        = sdp_attr_get_fmtp_rtx_time(
                                            sdp_p,
                                            media_session_idx,
                                            0,
                                            k);
                                }
                            }
                            else {
                                snprintf(message_buffer, MAX_MSG_LENGTH,
                                         "an illegal FMTP specified in "
                                         "media session %d.",
                                         media_session_idx);
                                syslog_print(CFG_VALIDATION_ERR,
                                             channel_p->session_key,
                                             message_buffer);
                                return FALSE;
                            }
                        }
                    }
                    else if (session_type == ORIGINAL_STREAM) {
                        /* Cross checking the payload type first */
                        payload_type = sdp_attr_get_fmtp_payload_type(
                            sdp_p,
                            media_session_idx,
                            0,
                            1);
                        if (channel_p->original_source_payload_type
                            != payload_type) {
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "mismatched payload type "
                                     "in original stream session. "
                                     "Check m= line and a=fmtp line.");
                            syslog_print(CFG_VALIDATION_ERR,
                                         channel_p->session_key,
                                         message_buffer);
                            return FALSE;
                        }

                        bw_value = sdp_attr_get_fmtp_rtcp_per_rcvr_bw(
                            sdp_p,
                            media_session_idx,
                            0,
                            1);
                        if (bw_value > MAX_BW_ALLOWED) {
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "rtcp-per-rcvr-bw value for original "
                                     " media session "
                                     "exceeding the max value %d. "
                                     CHECK_DOCUMENT, MAX_BW_ALLOWED);
                            syslog_print(CFG_VALIDATION_ERR,
                                         channel_p->session_key,
                                         message_buffer);
                            return FALSE;
                        }

                        channel_p->original_rtcp_per_rcvr_bw = bw_value;
                    }
                    else {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "FMTP not allowed in media "
                                 "session %d.", media_session_idx);
                        syslog_print(CFG_VALIDATION_WARN,
                                     channel_p->session_key,
                                     message_buffer);
                    }

                    break;

                case SDP_ATTR_RTCP_RSIZE:
                    channel_p->original_rtcp_rsize=
                        sdp_attr_get_rtcp_rsize(sdp_p, 
                                                media_session_idx,
                                                0, 1);
                    break;
                case SDP_ATTR_RTCP_XR:
                    loss_rle = sdp_attr_get_rtcp_xr_loss_rle(sdp_p, 
                                                             media_session_idx,
                                                             0, 1);

                    /* Substract the size of header for Loss RLE */
                    if (loss_rle != RTCP_RLE_UNSPECIFIED) {
                        if (loss_rle > LOSS_RLE_HEADER) {
                            loss_rle -= LOSS_RLE_HEADER;
                        }
                        else {
                            loss_rle = 0;
                        }
                    }

                    stat_flags = sdp_attr_get_rtcp_xr_stat_summary(
                        sdp_p, media_session_idx, 0, 1);

                    switch (session_type) {
                        case ORIGINAL_STREAM:
                            channel_p->original_rtcp_xr_loss_rle = loss_rle;
                            channel_p->original_rtcp_xr_stat_flags = stat_flags;
                            per_loss_rle = sdp_attr_get_rtcp_xr_per_loss_rle(
                                sdp_p, media_session_idx, 0, 1);

                            /* Substract the size of header for Loss RLE */
                            if (per_loss_rle != RTCP_RLE_UNSPECIFIED) {
                                if (per_loss_rle > LOSS_RLE_HEADER) {
                                    per_loss_rle -= LOSS_RLE_HEADER;
                                }
                                else {
                                    per_loss_rle = 0;
                                }
                            }

                            channel_p->original_rtcp_xr_per_loss_rle = 
                                per_loss_rle;

                            /* Check whether it can report on full range 
                               of data */
                            if (possible_lose_xr_data(channel_p)) {
                                syslog_print(CFG_VALIDATION_WARN,
                                             channel_p->session_key,
                                             "been configured for XR reports, "
                                             "but may not be able to report "
                                             "the full range of data, due to "
                                             "insufficient RTCP per-receiver "
                                             "bandwidth.");
                            }
                            channel_p->original_rtcp_xr_multicast_acq =
                                sdp_attr_get_rtcp_xr_multicast_acq(
                                  sdp_p, media_session_idx, 0, 1);
                            channel_p->original_rtcp_xr_diagnostic_counters =
                                sdp_attr_get_rtcp_xr_diagnostic_counters(
                                  sdp_p, media_session_idx, 0, 1);
                            break;

                        case RE_SOURCED_STREAM:
                        case FEC_FIRST_STREAM:
                        case FEC_SECOND_STREAM:
                            break;

                        case UNICAST_RTX_STREAM:
                            channel_p->repair_rtcp_xr_loss_rle = loss_rle;
                            channel_p->repair_rtcp_xr_stat_flags = stat_flags;

                            break;

                        case MAX_SESSIONS:
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "an unsupported stream type "
                                     "in media session %d.",
                                     media_session_idx);
                            syslog_print(CFG_VALIDATION_WARN,
                                         channel_p->session_key,
                                         message_buffer);
                            break;
                    }

                    break;

                default:
                    snprintf(message_buffer, MAX_MSG_LENGTH,
                             "an unsupported attribute in media session %d.",
                             media_session_idx);
                    syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                                 message_buffer);
            }
        }
    }

    /* Figure out which role and which mode this channel is */
    /*
     * The following is the lookup table:
     *
     * Role/Mode    Original    Re-Sourced    Unicast RTX   FEC1     FEC2
     *
     * SSM_DS       sendonly    inactive       inactive   inactive inactive  
     * SSM_DS       sendonly    inactive       inactive   sendonly inactive  
     * SSM_DS       sendonly    inactive       inactive   sendonly sendonly  
     * VAM/src      recvonly    sendonly       sendonly   inactive inactive
     * VAM/las      recvonly    inactive       sendonly   inactive inactive
     * STB/src      inactive    recvonly       recvonly   inactive inactive
     * STB/src      inactive    recvonly       recvonly   recvonly inactive
     * STB/src      inactive    recvonly       recvonly   recvonly recvonly
     * STB/las      recvonly    inactive       recvonly   inactive inactive
     * STB/las      recvonly    inactive       recvonly   recvonly inactive
     * STB/las      recvonly    inactive       recvonly   recvonly recvonly
     *
     */
    if (strcmp(attr[ORIGINAL_STREAM], SENDONLY_STR) == 0 &&
        strcmp(attr[RE_SOURCED_STREAM], INACTIVE_STR) == 0 &&
        strcmp(attr[UNICAST_RTX_STREAM], INACTIVE_STR) == 0) {
        channel_p->role = SSM_DS;
        channel_p->mode = DISTRIBUTION_MODE;
    }
    else if (strcmp(attr[ORIGINAL_STREAM], RECVONLY_STR) == 0 &&
             strcmp(attr[RE_SOURCED_STREAM], SENDONLY_STR) == 0 &&
             strcmp(attr[UNICAST_RTX_STREAM], SENDONLY_STR) == 0) {
        channel_p->role = VAM;
        channel_p->mode = SOURCE_MODE;
        if (group_id[RE_SOURCED_STREAM][0] == '\0' ||
            group_id[UNICAST_RTX_STREAM][0] == '\0') {
            syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                         "missing mid in either re-sourced or retransmission "
                         " session. " CHECK_DOCUMENT);
            return FALSE;
        }
    }
    else if (strcmp(attr[ORIGINAL_STREAM], RECVONLY_STR) == 0 &&
             strcmp(attr[RE_SOURCED_STREAM], INACTIVE_STR) == 0 &&
             strcmp(attr[UNICAST_RTX_STREAM], SENDONLY_STR) == 0) {
        channel_p->role = VAM;
        channel_p->mode = LOOKASIDE_MODE;
        if (group_id[ORIGINAL_STREAM][0] == '\0' ||
            group_id[UNICAST_RTX_STREAM][0] == '\0') {
            syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                         "missing mid in either original or retransmission "
                         " session. " CHECK_DOCUMENT);
            return FALSE;
        }
    }
    else if (strcmp(attr[ORIGINAL_STREAM], INACTIVE_STR) == 0 &&
             strcmp(attr[RE_SOURCED_STREAM], RECVONLY_STR) == 0 &&
             strcmp(attr[UNICAST_RTX_STREAM], RECVONLY_STR) == 0) {
        channel_p->role = STB;
        channel_p->mode = SOURCE_MODE;
        if (group_id[RE_SOURCED_STREAM][0] == '\0' ||
            group_id[UNICAST_RTX_STREAM][0] == '\0') {
            syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                         "missing mid in either re-sourced or retransmission "
                         " session. " CHECK_DOCUMENT);
            return FALSE;
        }
    }
    else if (strcmp(attr[ORIGINAL_STREAM], RECVONLY_STR) == 0 &&
             strcmp(attr[RE_SOURCED_STREAM], INACTIVE_STR) == 0 &&
             strcmp(attr[UNICAST_RTX_STREAM], RECVONLY_STR) == 0) {
        channel_p->role = STB;
        channel_p->mode = LOOKASIDE_MODE;
        if (group_id[ORIGINAL_STREAM][0] == '\0' ||
            group_id[UNICAST_RTX_STREAM][0] == '\0') {
            syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                         "missing mid in either original or retransmission "
                         " session. " CHECK_DOCUMENT);
            return FALSE;
        }
    }
    else if (strcmp(attr[ORIGINAL_STREAM], RECVONLY_STR) == 0 &&
             strcmp(attr[RE_SOURCED_STREAM], INACTIVE_STR) == 0 &&
             strcmp(attr[UNICAST_RTX_STREAM], INACTIVE_STR) == 0) {
        channel_p->role = STB;
        channel_p->mode = RECV_ONLY_MODE;
    }
    else {
        syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                     "either the role or the mode not being specified. "
                     CHECK_DOCUMENT);
        return FALSE;
    }

    if (strcmp(attr[UNICAST_RTX_STREAM], INACTIVE_STR) == 0 &&
        (channel_p->er_enable || channel_p->fcc_enable)) {
        channel_p->er_enable = FALSE;
        channel_p->fcc_enable = FALSE;
        syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                     "Error repair and RCC disabled due to missing "
                     "retransmission section.", CHECK_DOCUMENT);
    }

    if (strcmp(attr[FEC_FIRST_STREAM], RECVONLY_STR) == 0 &&
        strcmp(attr[FEC_SECOND_STREAM], INACTIVE_STR) == 0) {
        channel_p->fec_enable = TRUE;
        channel_p->fec_mode = FEC_1D_MODE;
        if (channel_p->mode == LOOKASIDE_MODE || 
            channel_p->mode == RECV_ONLY_MODE) {
            if (group_id[ORIGINAL_STREAM][0] == '\0' ||
                group_id[FEC_FIRST_STREAM][0] == '\0') {
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             "missing mid in either original or first FEC "
                             " session. " CHECK_DOCUMENT);
                return FALSE;
            }
        }
        else if (channel_p->mode == SOURCE_MODE) {
            if (group_id[RE_SOURCED_STREAM][0] == '\0' ||
                group_id[FEC_FIRST_STREAM][0] == '\0') {
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             "missing mid in either re-sourced or first FEC "
                             " session. " CHECK_DOCUMENT);
                return FALSE;
            }
        }
    }
    else if (strcmp(attr[FEC_FIRST_STREAM], RECVONLY_STR) == 0 &&
             strcmp(attr[FEC_SECOND_STREAM], RECVONLY_STR) == 0) {
        channel_p->fec_enable = TRUE;
        channel_p->fec_mode = FEC_2D_MODE;
        if (channel_p->mode == LOOKASIDE_MODE || 
            channel_p->mode == RECV_ONLY_MODE) {
            if (group_id[ORIGINAL_STREAM][0] == '\0' ||
                group_id[FEC_FIRST_STREAM][0] == '\0' ||
                group_id[FEC_SECOND_STREAM][0] == '\0') {
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             "missing mid in either original or one of FEC "
                             " sessions. " CHECK_DOCUMENT);
                return FALSE;
            }
        }
        else if (channel_p->mode == SOURCE_MODE) {
            if (group_id[RE_SOURCED_STREAM][0] == '\0' ||
                group_id[FEC_FIRST_STREAM][0] == '\0' ||
                group_id[FEC_SECOND_STREAM][0] == '\0') {
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             "missing mid in either re-sourced or one of FEC "
                             " sessions. " CHECK_DOCUMENT);
                return FALSE;
            }
        }
    }

    /* Check a=group:FID or FEC */
    boolean found_fid = FALSE;
    boolean found_fec = FALSE;
    for (j = 1; j < MAX_GROUP_LINES; j++) {
        switch (sdp_get_group_attr(sdp_p, SDP_SESSION_LEVEL, 0, j)) {
            case SDP_GROUP_ATTR_FID:
                if (channel_p->mode == SOURCE_MODE
                    || channel_p->mode == LOOKASIDE_MODE) {
                    found_fid = TRUE;
                    num_group_id = sdp_get_group_num_id(sdp_p,
                                                        SDP_SESSION_LEVEL,
                                                        0, j);
                    /* We only support group of two streams for now */
                    if (num_group_id != 2) {
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     "only two group ids allowed in FID "
                                     "attribute. " CHECK_DOCUMENT);
                        return FALSE;
                    }
                    else {
                        memset(group_label, 0, 4*SDP_MAX_STRING_LEN);
                        for (i = 0; i< num_group_id; i++) {
                            strncat(group_label,
                                    sdp_get_group_id(sdp_p,
                                                     SDP_SESSION_LEVEL,
                                                     0, j, i+1),
                                    SDP_MAX_STRING_LEN);
                            strncat(group_label, " ", 2);
                        }

                        /* Check to see whether all the sessions */
                        /* are grouped correctly or not */
                        if (channel_p->mode == SOURCE_MODE) {
                            if (strstr(group_label,
                                       group_id[RE_SOURCED_STREAM])
                                == NULL || 
                                strstr(group_label,
                                       group_id[UNICAST_RTX_STREAM])
                                == NULL) {
                                syslog_print(CFG_VALIDATION_ERR,
                                             channel_p->session_key,
                                             "FID attribute not matching "
                                             "session stream definition. "
                                             "Check a=group:FID line.");
                                return FALSE;
                            }
                        }
                        else if (channel_p->mode == LOOKASIDE_MODE) {
                            if (strstr(group_label,
                                       group_id[ORIGINAL_STREAM])
                                == NULL || 
                                strstr(group_label,
                                       group_id[UNICAST_RTX_STREAM])
                                == NULL) {
                                syslog_print(CFG_VALIDATION_ERR,
                                             channel_p->session_key,
                                             "FID attribute not matching "
                                             "session stream definition. "
                                             "Check a=group:FID line.");
                                return FALSE;
                            }
                        }
                    }
                }

                break;

            case SDP_GROUP_ATTR_FEC:
                /* Only check if FEC is enabled */
                if (channel_p->fec_enable == TRUE) {
                    found_fec = TRUE;
                    num_group_id = sdp_get_group_num_id(sdp_p,
                                                        SDP_SESSION_LEVEL,
                                                        0, j);
                    if (channel_p->fec_mode == FEC_1D_MODE) {
                        if (num_group_id != 2) {
                            syslog_print(CFG_VALIDATION_ERR,
                                         channel_p->session_key,
                                         "only two group ids required "
                                         "in FEC attribute for 1-D FEC. "
                                         "Check a=group:FEC line.");
                            return FALSE;
                        }
                        else {
                            memset(group_label, 0, SDP_MAX_STRING_LEN+1);
                            for (i = 0; i< num_group_id; i++) {
                                strncat(group_label,
                                        sdp_get_group_id(sdp_p,
                                                         SDP_SESSION_LEVEL,
                                                         0, j, i+1),
                                        SDP_MAX_STRING_LEN);
                                strncat(group_label, " ", 2);
                            }

                            /* Check to see whether all the sessions */
                            /* are grouped correctly or not */
                            if (channel_p->mode == SOURCE_MODE) {
                                if (strstr(group_label,
                                           group_id[RE_SOURCED_STREAM])
                                    == NULL || 
                                    strstr(group_label, 
                                           group_id[FEC_FIRST_STREAM])
                                    == NULL) {
                                    syslog_print(CFG_VALIDATION_ERR,
                                                 channel_p->session_key,
                                                 "FEC attribute not matching "
                                                 "session stream definition. "
                                                 "Check a=group:FEC line.");
                                    return FALSE;
                                }
                            }
                            else if (channel_p->mode == LOOKASIDE_MODE ||
                                     channel_p->mode == RECV_ONLY_MODE) {
                                if (strstr(group_label,
                                           group_id[ORIGINAL_STREAM])
                                    == NULL || 
                                    strstr(group_label, 
                                           group_id[FEC_FIRST_STREAM])
                                    == NULL) {
                                    syslog_print(CFG_VALIDATION_ERR,
                                                 channel_p->session_key,
                                                 "FEC attribute not matching "
                                                 "session stream definition. "
                                                 "Check a=group:FEC line.");
                                    return FALSE;
                                }
                            }
                        }
                    }
                    else if (channel_p->fec_mode == FEC_2D_MODE) {
                        if (num_group_id != 3) {
                            syslog_print(CFG_VALIDATION_ERR,
                                         channel_p->session_key,
                                         "only three group ids required "
                                         "in FEC attribute for 2-D FEC. "
                                         "Check a=group:FEC line.");
                            return FALSE;
                        }
                        else {
                            memset(group_label, 0, SDP_MAX_STRING_LEN+1);
                            for (i = 0; i< num_group_id; i++) {
                                strncat(group_label,
                                        sdp_get_group_id(sdp_p,
                                                         SDP_SESSION_LEVEL,
                                                         0, j, i+1),
                                        SDP_MAX_STRING_LEN);
                                strncat(group_label, " ", 2);
                            }

                            /* Check to see whether all the sessions */
                            /* are grouped correctly or not */
                            if (channel_p->mode == SOURCE_MODE) {
                                if (strstr(group_label,
                                           group_id[RE_SOURCED_STREAM])
                                    == NULL || 
                                    strstr(group_label, 
                                           group_id[FEC_FIRST_STREAM])
                                    == NULL ||
                                    strstr(group_label, 
                                           group_id[FEC_SECOND_STREAM]) 
                                    == NULL) {
                                    syslog_print(CFG_VALIDATION_ERR,
                                                 channel_p->session_key,
                                                 "FEC attribute not matching "
                                                 "session stream definition. "
                                                 "Check a=group:FEC line.");
                                    return FALSE;
                                }
                            }
                            else if (channel_p->mode == LOOKASIDE_MODE ||
                                     channel_p->mode == RECV_ONLY_MODE) {
                                if (strstr(group_label,
                                           group_id[ORIGINAL_STREAM])
                                    == NULL || 
                                    strstr(group_label, 
                                           group_id[FEC_FIRST_STREAM])
                                    == NULL ||
                                    strstr(group_label, 
                                           group_id[FEC_SECOND_STREAM])
                                    == NULL) {
                                    syslog_print(CFG_VALIDATION_ERR,
                                                 channel_p->session_key,
                                                 "FEC attribute not matching "
                                                 "session stream definition. "
                                                 "Check a=group:FEC line.");
                                    return FALSE;
                                }
                            }
                        }
                    }
                }

                break;
            
            default:
                break;
        }
    }

    if (channel_p->mode == SOURCE_MODE || channel_p->mode == LOOKASIDE_MODE) {
        if (found_fid == FALSE) {
            syslog_print(CFG_VALIDATION_ERR,
                         channel_p->session_key,
                         "missing FID attribute." CHECK_DOCUMENT);
            return FALSE;
        }
    }

    if (channel_p->fec_enable) {
        if (found_fec == FALSE) {
            syslog_print(CFG_VALIDATION_ERR,
                         channel_p->session_key,
                         "missing FEC attribute." CHECK_DOCUMENT);
            return FALSE;
        }
    }
        

    /* Check a=rtcp-unicast: */
    switch (sdp_get_rtcp_unicast_mode(sdp_p, SDP_SESSION_LEVEL, 0, 1)) {
      case SDP_RTCP_UNICAST_MODE_REFLECTION:
          syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                       "reflection mode not being supported. "
                       CHECK_DOCUMENT
                       " rsi mode will be used for RTCP unicast feedback.");
          //channel_p->rtcp_fb_mode = REFLECTION;
          channel_p->rtcp_fb_mode = RSI;
          break;

      case SDP_RTCP_UNICAST_MODE_RSI:
          channel_p->rtcp_fb_mode = RSI;
          break;
          
      case SDP_RTCP_UNICAST_MODE_NOT_PRESENT:
          if (channel_p->mode != DISTRIBUTION_MODE) {
              syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                           "a=rtcp-unicast line not being present. "
                           CHECK_DOCUMENT
                           " RTCP unicast feedback with rsi mode will be used.");
              channel_p->rtcp_fb_mode = RSI;
          }

          break;
          
      default:
          syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                       "an unsupported mode (rtcp-unicast).");
          return FALSE;
    }
    
    /*********************************/
    /* Make final pass of validation */
    /*********************************/
    switch (channel_p->role) {
        case VAM:
            /* One of the bandwidthes must be present in original session */
            if (channel_p->bit_rate == 0 && 
                channel_p->original_rtcp_sndr_bw == RTCP_BW_UNSPECIFIED &&
                channel_p->original_rtcp_rcvr_bw == RTCP_BW_UNSPECIFIED &&
                channel_p->original_rtcp_per_rcvr_bw == RTCP_BW_UNSPECIFIED) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "no bandwidth being specified.");
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             message_buffer);
                return FALSE;
            }

            /* RTP port cannot be the same as RTCP port in original session */
            if (channel_p->original_source_port ==
                channel_p->original_source_rtcp_port &&
                channel_p->source_proto == RTP_STREAM) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "RTP port being the same as RTCP port for "
                         "original stream. "
                         "Check m= line and a=rtcp line.");
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             message_buffer);
                return FALSE;
            }

            /* Allow rtcp port being missed in configuration */
            if (channel_p->source_proto == RTP_STREAM) {
                if (check_rtcp_port(&channel_p->original_source_port,
                                    &channel_p->original_source_rtcp_port,
                                    "original", channel_p) == FALSE) {
                    return FALSE;
                }
            }

            /* The source filter should be present for orignal stream. */
            if (found_src_filter_in_orig_source == FALSE) {
                syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                             "a=source-filter line not being specified in "
                             "original stream. " USE_IN_ADDR_ANY);
            }


            break;

        default:
            break;
    }


    switch (channel_p->mode) {
        case SOURCE_MODE:
            /* RTP port can't be the same as RTCP port in re-sourced session */
            if (channel_p->re_sourced_rtp_port ==
                channel_p->re_sourced_rtcp_port) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "RTP port being the same as RTCP port for "
                         "re-sourced stream. "
                         "Check m= line and a=rtcp line.");
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             message_buffer);
                return FALSE;
            }

            /* Allow rtcp port being missed in configuration */
            if (check_rtcp_port(&channel_p->re_sourced_rtp_port,
                                &channel_p->re_sourced_rtcp_port,
                                "re-sourced", channel_p) == FALSE) {
                return FALSE;
            }

            /* Original source address cannot be the same as re-sourced one */
            if (channel_p->original_source_addr.s_addr ==
                channel_p->re_sourced_addr.s_addr) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "the original multicast address being the same as "
                         "the re-sourced multicast address.");
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             message_buffer);
                return FALSE;
            }

            /* This is for backward compatibility */
            if (channel_p->fbt_address.s_addr == 0) {
                channel_p->fbt_address.s_addr = channel_p->rtx_addr.s_addr;
            }
            else {
                /* We require that the FBT must be the same as RTX */
                if (channel_p->fbt_address.s_addr != 
                    channel_p->rtx_addr.s_addr) {
                    snprintf(message_buffer, MAX_MSG_LENGTH,
                             "the rtx address not being the same as the FBT "
                             "address in source mode. " CHECK_DOCUMENT);
                    syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                                 message_buffer);
                }
            }
            
            /* The source filter must be present for re-sourced stream. */
            if (found_src_filter_in_re_sourced == FALSE) {
                syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                             "a=source-filter line not being specified in "
                             "re-sourced stream. " USE_IN_ADDR_ANY);
            }

            /* The src ip address must be the same as fbt address. */
            if (channel_p->fbt_address.s_addr != 
                inet_addr(src_address_for_resourced_stream)) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "the source IP address not being the same as the FBT "
                         "address in source mode.");
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             message_buffer);
                return FALSE;
            }

            /* The a=fmtp in unicast retransmission does not have */
            /* the correct association */
            if (channel_p->re_sourced_payload_type !=
                channel_p->rtx_apt) {
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             "a=fmtp:<payload> apt= line being incorrectly "
                             "specified in unicast retransmission stream. "
                             CHECK_DOCUMENT);
                return FALSE;
            }

            /* Make sure that RTCP is not being turned off */
            if (channel_p->er_enable || channel_p->fcc_enable) {
                if (channel_p->re_sourced_rtcp_sndr_bw == 0 &&
                    channel_p->re_sourced_rtcp_rcvr_bw == 0) {
                    snprintf(message_buffer, MAX_MSG_LENGTH,
                             "bandwidth being turned off. No unicast error "
                             "repair is enabled. " CHECK_DOCUMENT);
                    syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                                 message_buffer);
                    channel_p->er_enable = FALSE;
                    channel_p->fcc_enable = FALSE;
                }
            }

            break;

        case LOOKASIDE_MODE:
            /* The a=fmtp in unicast retransmission does not have */
            /* the correct association */
            if (channel_p->original_source_payload_type !=
                channel_p->rtx_apt) {
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             "a=fmtp:<payload> apt= line being incorrectly "
                             "specified in unicast retransmission stream. "
                             CHECK_DOCUMENT);
                return FALSE;
            }

            /* This is for backward compatibility */
            if (channel_p->fbt_address.s_addr == 0) {
                channel_p->fbt_address.s_addr = channel_p->rtx_addr.s_addr;
            }
            else {
                /* We require that the FBT must be the same as RTX */
                if (channel_p->fbt_address.s_addr != 
                    channel_p->rtx_addr.s_addr) {
                    snprintf(message_buffer, MAX_MSG_LENGTH,
                             "the rtx address not being the same as the FBT "
                             "address in lookaside mode. " CHECK_DOCUMENT);
                    syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                                 message_buffer);
                }
            }

            /* Make sure that RTCP is not being turned off */
            if (channel_p->er_enable || channel_p->fcc_enable) {
                if (channel_p->original_rtcp_per_rcvr_bw == 0 ||
                    (channel_p->original_rtcp_per_rcvr_bw 
                     == RTCP_BW_UNSPECIFIED  &&
                     channel_p->original_rtcp_sndr_bw == 0 &&
                     channel_p->original_rtcp_rcvr_bw == 0)) {
                    snprintf(message_buffer, MAX_MSG_LENGTH,
                             "bandwidth being turned off. No unicast error "
                             "repair is enabled. " CHECK_DOCUMENT);
                    syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                                 message_buffer);
                    channel_p->er_enable = FALSE;
                    channel_p->fcc_enable = FALSE;
                }
            }

            /* Do not break it and let it continue for the rest of checks */
            
        case RECV_ONLY_MODE:
            if (channel_p->fbt_address.s_addr == 0 && 
                (channel_p->original_rtcp_per_rcvr_bw != 0  ||
                 channel_p->original_rtcp_sndr_bw != 0 ||
                 channel_p->original_rtcp_rcvr_bw != 0)) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "the FBT address not being specified "
                         "in receiver only mode. " CHECK_DOCUMENT);
                syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                             message_buffer);
                return FALSE;
            }

        case DISTRIBUTION_MODE:
            /* RTP port cannot be the same as RTCP port in original session */
            if (channel_p->original_source_port ==
                channel_p->original_source_rtcp_port) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "RTP port being the same as RTCP port for "
                         "original stream. "
                         "Check m= line and a=rtcp line.");
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             message_buffer);
                return FALSE;
            }

            /* Allow rtcp port being missed in configuration */
            if (channel_p->source_proto == RTP_STREAM) {
                if (check_rtcp_port(&channel_p->original_source_port,
                                    &channel_p->original_source_rtcp_port,
                                    "original", channel_p) == FALSE) {
                    return FALSE;
                }
            }

            /* The source filter must be present for orignal stream. */
            if (found_src_filter_in_orig_source == FALSE) {
                syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                             "a=source-filter line not being specified in "
                             "original source stream. " USE_IN_ADDR_ANY);
            }

            break;

        default:
            break;
    }

    if (channel_p->mode == SOURCE_MODE ||
        channel_p->mode == LOOKASIDE_MODE) {
        /* RTP port cannot be the same as RTCP port in rtx session */
        if (channel_p->rtx_rtp_port == channel_p->rtx_rtcp_port) {
            snprintf(message_buffer, MAX_MSG_LENGTH,
                     "RTP port being the same as RTCP port for "
                     "unicast retransmission stream. "
                     "Check m= line and a=rtcp line.");
            syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                         message_buffer);
            return FALSE;
        }

        if (check_rtcp_port(&channel_p->rtx_rtp_port,
                            &channel_p->rtx_rtcp_port,
                            "retransmission", channel_p) == FALSE) {
            return FALSE;
        }

        /* RTCP port for original stream cannot be the same as the RTPC port */
        /* in rtx session. */
        if (channel_p->rtx_rtcp_port == channel_p->original_source_rtcp_port) {
            snprintf(message_buffer, MAX_MSG_LENGTH,
                     "RTCP port of original stream being the same as "
                     "RTCP port for unicast retransmission stream.");
            syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                         message_buffer);
            return FALSE;
        }

        /* Make sure that the FBT address is assigned */
        if (channel_p->fbt_address.s_addr == 0) {
            syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                         "the FBT address not being specified. "
                         CHECK_DOCUMENT);
            return FALSE;
        }
    }

    if (channel_p->fec_enable) {
        /* The source filter must be present for FEC stream1. */
        if (found_src_filter_in_fec_column == FALSE) {
            syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                         "a=source-filter line not being specified in first "
                         "FEC stream. " USE_IN_ADDR_ANY);
        }

        if (channel_p->fec_stream1.rtp_port ==
            channel_p->fec_stream1.rtcp_port) {
            snprintf(message_buffer, MAX_MSG_LENGTH,
                     "RTP port being the same as RTCP port for "
                     "first FEC stream. "
                     "Check m= line and a=rtcp line.");
            syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                         message_buffer);
            return FALSE;
        }

        /* We will not automatically created the RTCP port for FEC */
        /* in release 2.1                                          */

        /* If the FEC stream has the same address with the original one, */
        /* the RTP port must be different from the original one */
        if (channel_p->original_source_addr.s_addr ==
            channel_p->fec_stream1.multicast_addr.s_addr) {
            if (channel_p->original_source_port == 
                channel_p->fec_stream1.rtp_port) {
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             "the RTP port being the same in first FEC "
                             "stream as original source stream.");
                return FALSE;
            }

            if (channel_p->original_source_port == 
                channel_p->fec_stream1.rtcp_port) {
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             "the RTCP port being the same in first FEC "
                             "stream as RTP port in original source stream.");
                return FALSE;
            }

            if (channel_p->original_source_rtcp_port == 
                channel_p->fec_stream1.rtcp_port) {
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             "the RTCP port being the same in first FEC "
                             "stream as original source stream.");
                return FALSE;
            }

            if (channel_p->original_source_rtcp_port == 
                channel_p->fec_stream1.rtp_port) {
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             "the RTP port being the same in first FEC "
                             "stream as RTCP port in original source stream.");
                return FALSE;
            }
        }

        if (channel_p->fec_mode == FEC_2D_MODE) {
            /* The source filter must be present for FEC stream2. */
            if (found_src_filter_in_fec_row == FALSE) {
                syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                             "a=source-filter line not being specified in "
                             "second FEC stream. " USE_IN_ADDR_ANY);
            }

            if (channel_p->fec_stream2.rtp_port ==
                channel_p->fec_stream2.rtcp_port) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "RTP port being the same as RTCP port for "
                         "second FEC stream. "
                         "Check m= line and a=rtcp line.");
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             message_buffer);
                return FALSE;
            }

            /* We will not automatically created the RTCP port for FEC */
            /* in release 2.1                                          */

            /* If the FEC stream has the same address with the original one, */
            /* the RTP port must be different from the original one */
            if (channel_p->original_source_addr.s_addr ==
                channel_p->fec_stream2.multicast_addr.s_addr) {
                if (channel_p->original_source_port == 
                    channel_p->fec_stream2.rtp_port) {
                    syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                                 "the RTP port being the same in second FEC "
                                 "stream as original source stream.");
                    return FALSE;
                }

                if (channel_p->original_source_port == 
                    channel_p->fec_stream2.rtcp_port) {
                    syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                                 "the RTCP port being the same in second FEC "
                                 "stream as RTP port in original source "
                                 "stream.");
                    return FALSE;
                }

                if (channel_p->original_source_rtcp_port == 
                    channel_p->fec_stream2.rtcp_port) {
                    syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                                 "the RTCP port being the same in second FEC "
                                 "stream as original source stream.");
                    return FALSE;
                }

                if (channel_p->original_source_rtcp_port == 
                    channel_p->fec_stream2.rtp_port) {
                    syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                                 "the RTP port being the same in second FEC "
                                 "stream as RTCP port in original source "
                                 "stream.");
                    return FALSE;
                }
            }

            if (channel_p->fec_stream1.multicast_addr.s_addr == 
                channel_p->fec_stream2.multicast_addr.s_addr) {
                if (channel_p->fec_stream1.rtp_port == 
                    channel_p->fec_stream2.rtp_port) {
                    syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                                 "the RTP port being the same in second FEC "
                                 "stream as first FEC stream.");
                    return FALSE;
                }

                if (channel_p->fec_stream1.rtp_port == 
                    channel_p->fec_stream2.rtcp_port) {
                    syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                                 "the RTCP port being the same in second FEC "
                                 "stream as RTP port in first FEC stream.");
                    return FALSE;
                }

                if (channel_p->fec_stream1.rtcp_port == 
                    channel_p->fec_stream2.rtp_port) {
                    syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                                 "the RTP port being the same in second FEC "
                                 "stream as RTCP port in first FEC stream.");
                    return FALSE;
                }

                if ((channel_p->fec_stream1.rtcp_port == 
                     channel_p->fec_stream2.rtcp_port) &&
                    (channel_p->fec_stream1.rtcp_port != 0)){
                    syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                                 "the RTCP port being the same in second FEC "
                                 "stream as first FEC stream.");
                    return FALSE;
                }
            }
        }
    }

    /* Check whether the XR options are supported in current release or not */
    if ((channel_p->original_rtcp_xr_stat_flags & RTCP_XR_UNSUPPORTED_OPT_MASK)
        != 0) {
        syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                     "some unsupported values specified in stat-summary "
                     "of rtcp-xr attribute for original stream. "
                     "Currently, only loss, dup, and jitt are supported.");
        channel_p->original_rtcp_xr_stat_flags &= RTCP_XR_SUPPORTED_OPT_MASK;
    }

    if ((channel_p->repair_rtcp_xr_stat_flags & RTCP_XR_UNSUPPORTED_OPT_MASK)
        != 0) {
        syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                     "some unsupported values specified in stat-summary "
                     "of rtcp-xr attribute for retransmission stream. "
                     "Currently, only loss, dup, and jitt are supported.");
        channel_p->repair_rtcp_xr_stat_flags &= RTCP_XR_SUPPORTED_OPT_MASK;
    }

    /* Give a warning if post repair Loss RLE is enabled in recvonly mode
       without any repair mechanism enabled */
    if (channel_p->mode == RECV_ONLY_MODE && channel_p->fec_enable != TRUE) {
        if (channel_p->original_rtcp_xr_per_loss_rle != RTCP_RLE_UNSPECIFIED) {
            syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                         "post repair Loss RLE being enabled in receive-only "
                         "mode.");
        }
    }

    if (channel_p->bit_rate == -1) {
        snprintf(message_buffer, MAX_MSG_LENGTH,
                 "AS bandwidth being missing in original stream. "
                 CHECK_DOCUMENT);
        syslog_print(CFG_VALIDATION_ERR,
                     channel_p->session_key,
                     message_buffer);
        return FALSE;
    }

    if (strcmp(channel_p->session_key, "") == 0) {
        channel_p->complete = FALSE;
    }
    else {
        channel_p->complete = TRUE;
    }

    return (TRUE);
}


/* Function:    vod_validation_and_extraction
 * Description: Reads the parsed vod sdp configuration and store
 * Parameters:  sdp_p           SDP handle returned by sdp_init_description.
 *              channel_p       Channel configuration entry 
 * Returns:     True if parsed SDP passes semantic checking, else FALSE
 */
static boolean vod_validation_and_extraction (void *sdp_p,
                                              channel_cfg_t *channel_p)
{
    const char *username;
    const char *sessionid;
    const char *version;
    const char *creator_addr;
    const char *session_name;

    uint16_t i, j, k;
    uint16_t num_sessions;
    uint16_t media_session_idx;
    session_type_e session_type;
    char conn_address[SDP_MAX_STRING_LEN+1];
    sdp_payload_ind_e indicator;
    uint16_t payload_type;
    uint16_t pt;

    uint16_t num_attributes;
    sdp_attr_e attr_type;
    uint16_t num_instances;
    struct in_addr src_addr_p;
    char dest_address[SDP_MAX_STRING_LEN+1];
    uint32_t rtp_port;
    uint32_t rtcp_port;
    char nack_param[SDP_MAX_STRING_LEN+1];

    uint16_t num_group_id;
    char group_id[MAX_SESSIONS][SDP_MAX_STRING_LEN+1];
    char group_label[GROUP_SIZE];
    char message_buffer[MAX_MSG_LENGTH];

    uint16_t loss_rle;
    uint16_t per_loss_rle;
    uint32_t stat_flags;

    boolean prim_found = FALSE;
    boolean rtx_found = FALSE;

    if (sdp_p == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "vod_validation_and_extraction:: "
                      "Invalid SDP pointer.");
        return FALSE;
    }

    if (channel_p == NULL) {
        VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                      "vod_validation_and_extraction:: "
                      "Invalid channel pointer.");
        return FALSE;
    }

    /* Get the "o=" line to create a globally unique identifier */
    username = sdp_get_owner_username(sdp_p);
    sessionid = sdp_get_owner_sessionid(sdp_p);
    version = sdp_get_owner_version(sdp_p);
    creator_addr = sdp_get_owner_address(sdp_p);

    snprintf(channel_p->session_key, MAX_KEY_LENGTH, "INIP4#%s#%s#%s",
             username, sessionid, creator_addr);

    channel_p->version = atoll(version);
    if (strlen(version) >= 20 ||
        channel_p->version > MAX_VERSION_ALLOWED) {
        snprintf(message_buffer, MAX_MSG_LENGTH,
                 "reaching the maximum version number (%llu) allowed "
                 "in o= line. " CHECK_DOCUMENT, MAX_VERSION_ALLOWED);
        syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                     message_buffer);
        return FALSE;
    }

    /* Check the network type and address type */
    /* For now we only support IN and IP4 */
    if (sdp_get_owner_network_type(sdp_p) != SDP_NT_INTERNET ||
        sdp_get_owner_address_type(sdp_p) != SDP_AT_IP4) {
        syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                     "an unsupported network type or address type. "
                     "Currently only IN and IP4 are allowed.");
        return FALSE;
    }

    /* Get the name of the SDP session */
    session_name = (char *) sdp_get_session_name(sdp_p);
    strncpy(channel_p->name, session_name, SDP_MAX_STRING_LEN);

    /* Process all the media sessions */
    num_sessions = sdp_get_num_media_lines(sdp_p);
    if (num_sessions < MIN_VOD_SESSIONS) {
        snprintf(message_buffer, MAX_MSG_LENGTH,
                 "%d missing media session(s) [minimum %d sessions are "
                 "required.] " CHECK_DOCUMENT, MIN_VOD_SESSIONS-num_sessions,
                 MIN_VOD_SESSIONS);
        syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                     message_buffer);
        return FALSE;
    }
    
    /* VoD assumptions */    
    channel_p->role = STB;
    channel_p->mode = LOOKASIDE_MODE;
    channel_p->source_proto = RTP_STREAM;

    /* Set rtcp bandwidth to invalid first */
    channel_p->original_rtcp_sndr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->original_rtcp_rcvr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->original_rtcp_per_rcvr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->re_sourced_rtcp_sndr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->re_sourced_rtcp_rcvr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->repair_rtcp_sndr_bw = RTCP_BW_UNSPECIFIED;
    channel_p->repair_rtcp_rcvr_bw = RTCP_BW_UNSPECIFIED;

    /* Set rtcp loss RLE value to invalid first, it will be overwritten
       by the maximum size specifed in SDP */
    channel_p->original_rtcp_xr_loss_rle = RTCP_RLE_UNSPECIFIED;
    channel_p->original_rtcp_xr_per_loss_rle = RTCP_RLE_UNSPECIFIED;
    channel_p->repair_rtcp_xr_loss_rle = RTCP_RLE_UNSPECIFIED;

    for (i = 0; i < num_sessions; i++) {
        media_session_idx = i + 1;
        session_type = MAX_SESSIONS;

        /* Get total number of attributes */
        if (sdp_get_total_attrs(sdp_p, media_session_idx, 0, &num_attributes)
            != SDP_SUCCESS) {
            snprintf(message_buffer, MAX_MSG_LENGTH,
                     "failed to get the total number of attributes "
                     "in media session %d.",
                     media_session_idx);
            syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                         message_buffer);
            return FALSE;
        }

        /* Try to figure out which media session it is */
        session_type = figure_out_session_type(sdp_p,
                                               media_session_idx,
                                               channel_p);

        /* Retransmission stream found ? */
        if (session_type == UNICAST_RTX_STREAM) {
            rtx_found = TRUE;
        } else if (session_type == ORIGINAL_STREAM) {
            prim_found = TRUE;
        }

        /* Skip the unknown session */
        if (session_type != ORIGINAL_STREAM 
               && session_type != UNICAST_RTX_STREAM) {
            continue;
        }

        /* Get the payload type */
        if (sdp_get_media_num_payload_types(sdp_p, media_session_idx) == 1) {
            payload_type = sdp_get_media_payload_type(sdp_p,
                                                      media_session_idx,
                                                      1,
                                                      &indicator);

            if ((payload_type < MIN_PT && payload_type != STATIC_PT) ||
                (payload_type > MAX_PT)) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "m= line of media session %d has invalid "
                         "payload type %d. " CHECK_DOCUMENT,
                         media_session_idx, payload_type);
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             message_buffer);
                return FALSE;
            }
        }
        else {
            snprintf(message_buffer, MAX_MSG_LENGTH,
                     "m= line of media session %d has more than "
                     "one payload type "
                     "specified. Currently, only one stream per session "
                     "is allowed.",
                     media_session_idx);
            syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                         message_buffer);
            return FALSE;
        }

        /* Get the bandwidth */
        uint32_t bw_value;
        sdp_bw_modifier_e mod;
        int total_bw_lines = sdp_get_num_bw_lines(sdp_p, media_session_idx);
        switch (session_type) {
            case ORIGINAL_STREAM:
                if (total_bw_lines == 0) {
                    snprintf(message_buffer, MAX_MSG_LENGTH,
                             "primary source stream has no bandwidth "
                             "line specified. b=AS:<> line must be present.");
                    syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                                 message_buffer);
                    return FALSE;
                }

                break;
                                
            case UNICAST_RTX_STREAM:

                break;
                
            default:
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "an unsupported stream type in media session %d.",
                         media_session_idx);
                syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                             message_buffer);

                break;
        }

        for (j = 0; j < total_bw_lines; j++) {
            bw_value = sdp_get_bw_value(sdp_p,
                                        media_session_idx,
                                        j);
            if (bw_value > MAX_BW_ALLOWED) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "bandwidth for media session %d "
                         "exceeds the max value %d. " CHECK_DOCUMENT,
                         media_session_idx, MAX_BW_ALLOWED);
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             message_buffer);
                return FALSE;
            }
            mod = sdp_get_bw_modifier(sdp_p, media_session_idx, j);
            switch(mod) {
                case SDP_BW_MODIFIER_AS:
                    if (session_type == ORIGINAL_STREAM) {
                        if (bw_value < MIN_BIT_RATE_ALLOWED) {
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "AS bandwidth in primary media session "
                                     "less than the min value %d required. "
                                     CHECK_DOCUMENT, MIN_BIT_RATE_ALLOWED);
                            syslog_print(CFG_VALIDATION_ERR,
                                         channel_p->session_key,
                                         message_buffer);
                            return FALSE;
                        }

                        if (bw_value > MAX_BIT_RATE_ALLOWED) {
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "AS bandwidth in primary media session "
                                     "exceeds the max value %d. "
                                     CHECK_DOCUMENT, MAX_BIT_RATE_ALLOWED);
                            syslog_print(CFG_VALIDATION_ERR,
                                         channel_p->session_key,
                                         message_buffer);
                            return FALSE;
                        }

                        channel_p->bit_rate = bw_value * 1000;
                    }
                    else {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "an AS bandwidth specification "
                                 "in media session %d other than primary "
                                 "media session.",
                                 media_session_idx);
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     message_buffer);
                        return FALSE;
                    }

                    break;

                case SDP_BW_MODIFIER_RS:
                    switch (session_type) {
                        case ORIGINAL_STREAM:
                            channel_p->original_rtcp_sndr_bw = bw_value;
                            break;

                        case UNICAST_RTX_STREAM:
                            channel_p->repair_rtcp_sndr_bw = bw_value;
                            break;

                        default:
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "RS bandwidth specified in "
                                     "unknown media session %d.",
                                     media_session_idx);
                            syslog_print(CFG_VALIDATION_WARN, message_buffer);
                    }
                     
                    break;

                case SDP_BW_MODIFIER_RR:
                    switch (session_type) {
                        case ORIGINAL_STREAM:
                            channel_p->original_rtcp_rcvr_bw = bw_value;
                            break;

                        case UNICAST_RTX_STREAM:
                            channel_p->repair_rtcp_rcvr_bw = bw_value;
                            break;

                        default:
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "RR bandwidth specified in "
                                     "unknown media session %d.",
                                     media_session_idx);
                            syslog_print(CFG_VALIDATION_WARN, message_buffer);
                    }

                    break;

                default:
                    snprintf(message_buffer, MAX_MSG_LENGTH,
                             "an unsupported bandwidth modifier "
                             "in media session %d. "
                             CHECK_DOCUMENT, media_session_idx);
                    syslog_print(CFG_VALIDATION_WARN,
                                 channel_p->session_key,
                                 message_buffer);
            }
        }

        /* Get the connection address and destination port */
        strncpy(conn_address,
                sdp_get_conn_address(sdp_p, media_session_idx),
                SDP_MAX_STRING_LEN);
        if (is_valid_ip_address(conn_address) == FALSE &&
            strncmp(conn_address, "0.0.0.0", strlen("0.0.0.0"))) {
            snprintf(message_buffer, MAX_MSG_LENGTH,
                     "an illegal connection address %s in c= line of "
                     "media session %d. " CHECK_DOCUMENT,
                     conn_address, media_session_idx);
            syslog_print(CFG_VALIDATION_ERR,
                         channel_p->session_key,
                         message_buffer);
            return FALSE;
        }

        rtp_port = sdp_get_media_portnum(sdp_p, media_session_idx);
        if (rtp_port > USHRT_MAX) {
            snprintf(message_buffer, MAX_MSG_LENGTH,
                     "an illegal RTP port number %d in m= line of "
                     "media session %d. " CHECK_DOCUMENT,
                     rtp_port, media_session_idx);
            syslog_print(CFG_VALIDATION_ERR,
                         channel_p->session_key,
                         message_buffer);
            return FALSE;
        }

        switch (session_type) {
            case ORIGINAL_STREAM:
                channel_p->original_source_addr.s_addr
                    = inet_addr(conn_address);
                channel_p->original_source_port = htons(rtp_port);
                channel_p->original_source_payload_type = payload_type;
                break;

            case UNICAST_RTX_STREAM:
                if (is_valid_ip_host_addr(conn_address,
                                          &(channel_p->rtx_addr.s_addr)) 
                    == FALSE &&
                    strncmp(conn_address, "0.0.0.0", strlen("0.0.0.0"))) {
                    snprintf(message_buffer, MAX_MSG_LENGTH,
                             "an illegal connection address %s in "
                             "c= line of unicast retransmission stream. "
                             CHECK_DOCUMENT, conn_address);
                    syslog_print(CFG_VALIDATION_ERR,
                                 channel_p->session_key,
                                 message_buffer);
                    return FALSE;
                }
                channel_p->rtx_addr.s_addr = inet_addr(conn_address);
                channel_p->rtx_rtp_port = htons(rtp_port);
                channel_p->rtx_payload_type = payload_type;
                break;

            default:
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "an unsupported stream type in media session %d.",
                         media_session_idx);
                syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                             message_buffer);
        }

        /* Get the remaining attributes */
        group_id[session_type][0] = '\0';
        for (j = 0; j < num_attributes; j++) {
            if (sdp_get_attr_type(sdp_p,
                                  media_session_idx,
                                  0,
                                  j+1,
                                  &attr_type,
                                  &num_instances) != SDP_SUCCESS) {
                snprintf(message_buffer, MAX_MSG_LENGTH,
                         "failing to get attribute type in media session %d.",
                         media_session_idx);
                syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                             message_buffer);
                return FALSE;
            }

            switch (attr_type) {
                case SDP_ATTR_RTPMAP:
                    /* Cross checking the payload type */
                    payload_type = sdp_attr_get_rtpmap_payload_type(
                        sdp_p,
                        media_session_idx,
                        0,
                        num_instances);
                    if ((payload_type < MIN_PT && payload_type != STATIC_PT) ||
                        (payload_type > MAX_PT)) {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "media session %d has invalid payload "
                                 "type %d in a=rtpmap:<> line. "
                                 CHECK_DOCUMENT,
                                 media_session_idx, payload_type);
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     message_buffer);
                        return FALSE;
                    }

                    pt = 0;
                    switch (session_type) {
                        case ORIGINAL_STREAM:
                            pt = channel_p->original_source_payload_type;
                            break;

                        case UNICAST_RTX_STREAM:
                            pt = channel_p->rtx_payload_type;
                            break;

                        default:
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "an unsupported stream type "
                                     "in media session %d.",
                                     media_session_idx);
                            syslog_print(CFG_VALIDATION_WARN,
                                         channel_p->session_key,
                                         message_buffer);
                            break;
                    }

                    if (pt != payload_type) {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "mismatched payload type "
                                 "in media session %d. "
                                 "Check m= line and a=rtpmap line.",
                                 media_session_idx);
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     message_buffer);
                        return FALSE;
                    }

                    break;

                case SDP_ATTR_RTCP:
                    rtcp_port = sdp_get_rtcp_port(sdp_p,
                                                  media_session_idx,
                                                  0,
                                                  num_instances);

                    if (rtcp_port > USHRT_MAX) {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "an illegal RTCP port number %d in "
                                 "a=rtcp line of media session %d. "
                                 CHECK_DOCUMENT,
                                 rtcp_port, media_session_idx);
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     message_buffer);
                        return FALSE;
                    }

                    if (sdp_get_rtcp_conn_address(sdp_p,
                                                  media_session_idx,
                                                  0,
                                                  num_instances,
                                                  NULL,
                                                  NULL,
                                                  dest_address)
                        != SDP_SUCCESS) {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "failing to get connection "
                                 "address for RTCP in media session %d.",
                                 media_session_idx);
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     message_buffer);
                        return FALSE;
                    }
                        
                    /* Make sure that the address is a valid one */
                    if (dest_address[0] != '\0' &&
                        is_valid_ip_host_addr(dest_address,
                                              &src_addr_p.s_addr) == FALSE &&
                        strncmp(conn_address, "0.0.0.0", strlen("0.0.0.0"))) {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "an illegal address %s "
                                 "in a=rtcp line of media "
                                 "session %d. " CHECK_DOCUMENT,
                                 dest_address, media_session_idx);
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     message_buffer);
                        return FALSE;
                    }

                    switch (session_type) {
                        case ORIGINAL_STREAM:
                            channel_p->original_source_rtcp_port
                                = htons(rtcp_port);
                            if (dest_address[0] != '\0') {
                                channel_p->fbt_address.s_addr
                                    = inet_addr(dest_address);
                            }

                            break;

                        case UNICAST_RTX_STREAM:
                            channel_p->rtx_rtcp_port = htons(rtcp_port);
                            if (dest_address[0] != '\0' &&
                                channel_p->rtx_addr.s_addr != 
                                inet_addr(dest_address)) {
                                snprintf(message_buffer, MAX_MSG_LENGTH,
                                         "an unmatched address %s "
                                         "in a=rtcp line of media "
                                         "session %d from c= line. " 
                                         CHECK_DOCUMENT,
                                         dest_address, media_session_idx);
                                syslog_print(CFG_VALIDATION_WARN,
                                             channel_p->session_key,
                                             message_buffer);
                            }
                            break;

                        default:
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "an unsupported stream type "
                                     "in media session %d.",
                                     media_session_idx);
                            syslog_print(CFG_VALIDATION_WARN,
                                         channel_p->session_key,
                                         message_buffer);
                            break;
                    }

                    break;

                case SDP_ATTR_RTCP_FB:
                    if (session_type == ORIGINAL_STREAM) {
                        payload_type = 0;
                        if (sdp_get_rtcp_fb_nack_param(sdp_p,
                                                       media_session_idx,
                                                       0,
                                                       num_instances,
                                                       &payload_type,
                                                       nack_param)
                            != SDP_SUCCESS) {
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "failed to get RTCP feedback parameters "
                                     "in media session %d.",
                                     media_session_idx);
                            syslog_print(CFG_VALIDATION_ERR, 
                                         channel_p->session_key,
                                         message_buffer);
                            return FALSE;
                        }

                        /* Check that protocol is RTP/AVPF */
                        if (sdp_get_media_transport(sdp_p, media_session_idx)
                            != SDP_TRANSPORT_RTPAVPF) {
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "a=rtcp-fb line present for non-"
                                     "RTP/AVPF media session. ");
                            syslog_print(CFG_VALIDATION_ERR,
                                         channel_p->session_key,
                                         message_buffer);
                            return FALSE;
                        }

                        /* Cross checking the payload type first */
                        if (channel_p->original_source_payload_type
                            != payload_type) {
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "mismatched payload type "
                                     "in primary media session. "
                                     "Check m= line and a=rtcp-fb line.");
                            syslog_print(CFG_VALIDATION_ERR,
                                         channel_p->session_key,
                                         message_buffer);
                            return FALSE;
                        }
                        
                        if (strcmp(nack_param, "") == 0) {
                            channel_p->er_enable = TRUE;
                        }

                        if (strcmp(nack_param, "pli") == 0) {
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "unsupported RTCP feedback parameter. "
                                     "'nack pli' not supported for VoD.");
                            syslog_print(CFG_VALIDATION_WARN, 
                                         channel_p->session_key,
                                         message_buffer);
                        }
                    }
                    else {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "an illegal RTCP feedback specified in "
                                 "media session %d.",
                                 media_session_idx);
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     message_buffer);
                        return FALSE;
                    }

                    break;

                case SDP_ATTR_MID:
                    strncpy(group_id[session_type],
                            sdp_attr_get_simple_string(sdp_p,
                                                       SDP_ATTR_MID,
                                                       media_session_idx,
                                                       0,
                                                       num_instances),
                            SDP_MAX_STRING_LEN);

                    break;

                case SDP_ATTR_FMTP:
                    if (session_type == UNICAST_RTX_STREAM) {
                        for (k = 1; k < num_instances+1; k++) {
                            /* Check apt value exists and corresponds */
                            /* to the primary payload number */
                            if (sdp_attr_fmtp_get_fmtp_format(
                                sdp_p,
                                media_session_idx,
                                0,
                                k) == SDP_FMTP_RTP_RETRANS) {
                                /* Cross checking the payload type first */
                                payload_type = sdp_attr_get_fmtp_payload_type(
                                    sdp_p,
                                    media_session_idx,
                                    0,
                                    k);
                                if (channel_p->rtx_payload_type
                                    != payload_type) {
                                    snprintf(message_buffer, MAX_MSG_LENGTH,
                                             "mismatched payload type "
                                             "in unicast retransmission "
                                             "session. "
                                             "Check m= line and a=fmtp line.");
                                    syslog_print(CFG_VALIDATION_ERR,
                                                 channel_p->session_key,
                                                 message_buffer);
                                    return FALSE;
                                }
                                
                                if (sdp_attr_get_fmtp_apt(sdp_p,
                                                          media_session_idx,
                                                          0, k) > 0) {
                                    channel_p->rtx_apt = sdp_attr_get_fmtp_apt(
                                        sdp_p,
                                        media_session_idx,
                                        0,
                                        k);
                                }
                                
                                if (sdp_attr_get_fmtp_rtx_time(
                                        sdp_p,
                                        media_session_idx, 0, k) > 0) {
                                    channel_p->rtx_time
                                        = sdp_attr_get_fmtp_rtx_time(
                                            sdp_p,
                                            media_session_idx,
                                            0,
                                            k);
                                }
                            }
                            else {
                                snprintf(message_buffer, MAX_MSG_LENGTH,
                                         "an illegal FMTP specified in "
                                         "media session %d.",
                                         media_session_idx);
                                syslog_print(CFG_VALIDATION_ERR,
                                             channel_p->session_key,
                                             message_buffer);
                                return FALSE;
                            }
                        }
                    }
                    else {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "FMTP not allowed in media "
                                 "session %d.", media_session_idx);
                        syslog_print(CFG_VALIDATION_WARN,
                                     channel_p->session_key,
                                     message_buffer);
                    }

                    break;

                case SDP_ATTR_RTCP_XR:
                    loss_rle = sdp_attr_get_rtcp_xr_loss_rle(sdp_p, 
                                                             media_session_idx,
                                                             0, 1);

                    /* Substract the size of header for Loss RLE */
                    if (loss_rle != RTCP_RLE_UNSPECIFIED) {
                        if (loss_rle > LOSS_RLE_HEADER) {
                            loss_rle -= LOSS_RLE_HEADER;
                        }
                        else {
                            loss_rle = 0;
                        }
                    }

                    stat_flags = sdp_attr_get_rtcp_xr_stat_summary(
                        sdp_p, media_session_idx, 0, 1);

                    switch (session_type) {
                        case ORIGINAL_STREAM:
                            channel_p->original_rtcp_xr_loss_rle = loss_rle;
                            channel_p->original_rtcp_xr_stat_flags = stat_flags;
                            per_loss_rle = sdp_attr_get_rtcp_xr_per_loss_rle(
                                sdp_p, media_session_idx, 0, 1);

                            /* Substract the size of header for Loss RLE */
                            if (per_loss_rle != RTCP_RLE_UNSPECIFIED) {
                                if (per_loss_rle > LOSS_RLE_HEADER) {
                                    per_loss_rle -= LOSS_RLE_HEADER;
                                }
                                else {
                                    per_loss_rle = 0;
                                }
                            }

                            channel_p->original_rtcp_xr_per_loss_rle = 
                                per_loss_rle;
                            channel_p->original_rtcp_xr_multicast_acq =
                                sdp_attr_get_rtcp_xr_multicast_acq(
                                  sdp_p, media_session_idx, 0, 1);
                            channel_p->original_rtcp_rsize =
                                sdp_attr_get_rtcp_rsize(
                                  sdp_p, media_session_idx, 0, 1);
                            channel_p->original_rtcp_xr_diagnostic_counters =
                                sdp_attr_get_rtcp_xr_diagnostic_counters(
                                  sdp_p, media_session_idx, 0, 1);
                            break;

                        case UNICAST_RTX_STREAM:
                            channel_p->repair_rtcp_xr_loss_rle = loss_rle;
                            channel_p->repair_rtcp_xr_stat_flags = stat_flags;

                            break;

                        default:
                            snprintf(message_buffer, MAX_MSG_LENGTH,
                                     "an unsupported stream type "
                                     "in media session %d.",
                                     media_session_idx);
                            syslog_print(CFG_VALIDATION_WARN,
                                         channel_p->session_key,
                                         message_buffer);
                            break;
                    }

                    break;

                default:
                    snprintf(message_buffer, MAX_MSG_LENGTH,
                             "an unsupported attribute in media session %d.",
                             media_session_idx);
                    syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                                 message_buffer);
            }
        }
    }

    /* Check that primary media present */
    if (!prim_found) {
        syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                     "Primary media section missing from SDP. "
                     CHECK_DOCUMENT);
        return FALSE;
    }

    /* Check a=group:FID */
    boolean found_fid = FALSE;
    for (j = 1; j < MAX_GROUP_LINES; j++) {
        switch (sdp_get_group_attr(sdp_p, SDP_SESSION_LEVEL, 0, j)) {
            case SDP_GROUP_ATTR_FID:
                found_fid = TRUE;
                num_group_id = sdp_get_group_num_id(sdp_p,
                                                    SDP_SESSION_LEVEL,
                                                    0, j);
                /* We only support group of two streams for now */
                if (num_group_id != 2) {
                    syslog_print(CFG_VALIDATION_ERR,
                                 channel_p->session_key,
                                 "only two group ids allowed in FID "
                                 "attribute. " CHECK_DOCUMENT);
                    return FALSE;
                }
                else {
                    memset(group_label, 0, 4*SDP_MAX_STRING_LEN);
                    for (i = 0; i< num_group_id; i++) {
                        strncat(group_label,
                                sdp_get_group_id(sdp_p,
                                                 SDP_SESSION_LEVEL,
                                                 0, j, i+1),
                                SDP_MAX_STRING_LEN);
                        strncat(group_label, " ", 2);
                    }

                    /* Check that media identifiers are valid */
                    if (group_id[ORIGINAL_STREAM][0] == '\0' ||
                        group_id[UNICAST_RTX_STREAM][0] == '\0') {
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     "primary or retransmission "
                                     "missing mid attribute.");
                        return FALSE;
                    }

                    /* Check to see whether all the sessions */
                    /* are grouped correctly or not */
                    if (strstr(group_label,
                               group_id[ORIGINAL_STREAM])
                        == NULL || 
                        strstr(group_label,
                               group_id[UNICAST_RTX_STREAM])
                        == NULL) {
                        syslog_print(CFG_VALIDATION_ERR,
                                     channel_p->session_key,
                                     "FID attribute not matching "
                                     "session stream definition. "
                                     "Check a=group:FID line.");
                        return FALSE;
                    }
                }

                break;

            default:
                break;
        }
    }

    /* Check for group FID line */
    if (rtx_found && !found_fid) {
        syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                     "missing FID attribute (a=group:FID)",
                     CHECK_DOCUMENT);
        return FALSE;
    }        

    /* The a=fmtp in unicast retransmission does not have */
    /* the correct association */
    if (rtx_found && (channel_p->original_source_payload_type !=
        channel_p->rtx_apt)) {
        syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                     "a=fmtp:<payload> apt= line being incorrectly "
                     "specified in unicast retransmission stream. "
                     CHECK_DOCUMENT);
        return FALSE;
    }

    /* We require that the FBT must be the same as rtx */
    if (rtx_found && (channel_p->fbt_address.s_addr != 
        channel_p->rtx_addr.s_addr)) {
        snprintf(message_buffer, MAX_MSG_LENGTH,
                 "the rtx address differs from the FBT "
                 "address in the primary media section. " CHECK_DOCUMENT);
        syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                     message_buffer);
    }

    /* Make sure that RTCP is not being turned off */
    if (channel_p->er_enable) {
        if (channel_p->original_rtcp_per_rcvr_bw == 0 ||
            (channel_p->original_rtcp_per_rcvr_bw 
             == RTCP_BW_UNSPECIFIED  &&
             channel_p->original_rtcp_sndr_bw == 0 &&
             channel_p->original_rtcp_rcvr_bw == 0)) {
            snprintf(message_buffer, MAX_MSG_LENGTH,
                     "bandwidth being turned off. No unicast error "
                     "repair is enabled. " CHECK_DOCUMENT);
            syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                         message_buffer);
            channel_p->er_enable = FALSE;
            channel_p->fcc_enable = FALSE;
        }
    }

    /* Check whether the XR options are supported in current release or not */
    if ((channel_p->original_rtcp_xr_stat_flags & RTCP_XR_UNSUPPORTED_OPT_MASK)
        != 0) {
        syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                     "some unsupported values specified in stat-summary "
                     "of rtcp-xr attribute for original stream. "
                     "Currently, only loss, dup, and jitt are supported.");
        channel_p->original_rtcp_xr_stat_flags &= RTCP_XR_SUPPORTED_OPT_MASK;
    }
    if ((channel_p->repair_rtcp_xr_stat_flags & RTCP_XR_UNSUPPORTED_OPT_MASK)
        != 0) {
        syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                     "some unsupported values specified in stat-summary "
                     "of rtcp-xr attribute for retransmission stream. "
                     "Currently, only loss, dup, and jitt are supported.");
        channel_p->repair_rtcp_xr_stat_flags &= RTCP_XR_SUPPORTED_OPT_MASK;
    }

    /* Check for original stream bit rate */
    if (channel_p->bit_rate == -1) {
        snprintf(message_buffer, MAX_MSG_LENGTH,
                 "AS bandwidth being missing in original stream. "
                 CHECK_DOCUMENT);
        syslog_print(CFG_VALIDATION_ERR,
                     channel_p->session_key,
                     message_buffer);
        return FALSE;
    }

    /* Check that retransmission media present */
    if (!rtx_found && (found_fid || channel_p->er_enable)) {
        syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                     "Retransmission section missing from SDP. "
                     CHECK_DOCUMENT);
        return FALSE;
    }

    /* Optional RTSP-specific checks */
    if (channel_p->original_source_addr.s_addr ||
        channel_p->original_source_port ||
        channel_p->original_source_rtcp_port ||
        channel_p->rtx_addr.s_addr ||
        channel_p->rtx_rtp_port ||
        channel_p->rtx_rtcp_port) {
        syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                     "Transport parameters present in SDP for VoD. "
                     CHECK_DOCUMENT);
    }

    if (strcmp(channel_p->session_key, "") == 0) {
        channel_p->complete = FALSE;
    }
    else {
        channel_p->complete = TRUE;
    }

    return (TRUE);
}

static uint32_t channel_hash_func (channel_hash_key key)
{

    uint32_t hash;

    hash = ntohl(key.addr) % CHAN_MAP_DB_HASH_SIZE;
    
    return hash;
}


static boolean channel_equal (channel_handle_t *c1,
                              channel_handle_t *c2)
{

    if(c1->address.s_addr != c2->address.s_addr ||
       c1->port != c2->port) {
        return FALSE;
    }

    return TRUE;
}

static channel_hash_key channel_hash_key_by_chan (channel_handle_t *chan)
{
    channel_hash_key key;

    key.addr = chan->address.s_addr;
    key.port = chan->port;

    return key;
}

boolean channel_add_map (channel_map_db_t *chan_db,
                         struct in_addr address,
                         in_port_t port,
                         idmgr_id_t handle)
{
    uint32_t hash;
    channel_hash_key key;
    idmgr_id_t temp_handle;
    channel_handle_t *chan;

    /*
     * If the port is not set, return FALSE
     */
    if(port == 0) {
        return FALSE;
    }

    /*
     * If the channel is already there, return TRUE;
     */
    if(channel_lookup(chan_db, address, 
                      port, &temp_handle)) {
        return FALSE;
    }

    if (chan_db) {
        chan = malloc(sizeof(channel_handle_t));
        if (chan == NULL) {
            return FALSE;
        }

        chan->address.s_addr = address.s_addr;
        chan->port = port;
        chan->handle = handle;
        chan->next = NULL;
        
        key = channel_hash_key_by_chan(chan);
        hash = channel_hash_func(key);
        
        if (!chan_db->chan_cache_head[hash])
        {
            chan_db->chan_cache_head[hash] = chan;
        }
        else {
            chan->next = chan_db->chan_cache_head[hash];
            chan_db->chan_cache_head[hash] = chan;
        }
        
        return TRUE;
    }

    return FALSE;
}

boolean channel_remove_map (channel_map_db_t *chan_db,
                            struct in_addr address,
                            in_port_t port,
                            idmgr_id_t handle)
{
    uint32_t hash;
    channel_hash_key key;
    channel_handle_t *chan;

    boolean ret = FALSE;
    idmgr_id_t tmp_hdl;

    /* Look it up first. If not exist, return TRUE */
    if (channel_lookup(chan_db, address, port, &tmp_hdl) == FALSE) {
        return TRUE;
    }

    if (chan_db) {
        chan = malloc(sizeof(channel_handle_t));
        if (chan == NULL) {
            return FALSE;
        }

        chan->address.s_addr = address.s_addr;
        chan->port = port;
        chan->handle = handle;
        chan->next = NULL;

        key = channel_hash_key_by_chan(chan);
        hash = channel_hash_func(key);
        

        if (chan_db->chan_cache_head[hash]) {
            channel_handle_t *temp = chan_db->chan_cache_head[hash];
            channel_handle_t *temp1;
            
            if(temp) {
                if (channel_equal(temp, chan)) {
                    chan_db->chan_cache_head[hash] = temp->next;
                    free(temp);
                    ret = TRUE;
                }
                else {
                    while (temp->next && 
                           !channel_equal(temp->next, chan)) {
                        temp = temp->next;
                    }
                    
                    if (temp->next && channel_equal(temp->next, chan)) {
                        temp1 = temp->next;
                        temp->next = temp1->next;
                        free(temp1);
                        ret = TRUE;
                    }
                }
            }
        }

        free(chan);
    }

    return ret;
}


boolean channel_lookup (channel_map_db_t *chan_db,
                        struct in_addr address, 
                        in_port_t port,
                        idmgr_id_t *handle)
{
    boolean ret = FALSE;

    uint32_t hash;
    channel_hash_key key;

    if (chan_db) {
        key.addr = address.s_addr;
        key.port = port;

        hash = channel_hash_func( key );

        if ( chan_db->chan_cache_head[hash]) {
            channel_handle_t *temp = chan_db->chan_cache_head[hash];

            while (temp) {
                if (temp->address.s_addr == address.s_addr &&
                    temp->port == port ) {
                    *handle = temp->handle;

                    ret = TRUE;
                    break;
                }
                else {
                    temp = temp->next;
                }
            }
        }
    }

    return ret;

}


/* Function:    figure_out_session_type
 * Description: Get the session type based on SDP media session
 * Parameters:  sdp_p           SDP handle returned by sdp_init_description.
 *              index           media session index
 * Returns:     session type defined above
 */
static session_type_e figure_out_session_type (void *sdp_p,
                                               uint16_t index,
                                               channel_cfg_t *channel_p)
{
    int i;
    uint16_t num_attributes;
    uint16_t num_instances;
    sdp_attr_e attr_type;
    uint32_t clock_rate;
    const char *encoded_name;
    char message_buffer[MAX_MSG_LENGTH];

    /* Check whether the media type is video */
    /* Currently we can only handle video stream */
    if (sdp_get_media_type(sdp_p, index) != SDP_MEDIA_VIDEO) {
        snprintf(message_buffer, MAX_MSG_LENGTH,
                 "m= line of media session %d not being a video type. "
                 "Currently, only vedeo type is supported.", index);
        syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                     message_buffer);
        return MAX_SESSIONS;
    }

    /*
     * The way we figure out which type the media session is based on
     * a=rtpmap line. Then we would validate it with m= line.
     *
     */
    if (sdp_get_total_attrs(sdp_p, index, 0, &num_attributes)
        != SDP_SUCCESS) {
        snprintf(message_buffer, MAX_MSG_LENGTH,
                 "failing to get the total number of attributes "
                 "in media session %d.",
                 index);
        syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                     message_buffer);
        return MAX_SESSIONS;
    }
    for (i = 0; i < num_attributes; i++) {
        if (sdp_get_attr_type(sdp_p, index, 0, i+1,
                              &attr_type, &num_instances) != SDP_SUCCESS) {
            /* No any attributes including a=rtpmap line */
            snprintf(message_buffer, MAX_MSG_LENGTH,
                     "failing to get attribute type in media session %d.",
                     index);
            syslog_print(CFG_VALIDATION_WARN, channel_p->session_key,
                         message_buffer);
            return MAX_SESSIONS;
        }

        switch (attr_type) {
            case SDP_ATTR_RTPMAP:
                /* Check the clock rate */
                clock_rate = sdp_attr_get_rtpmap_clockrate(sdp_p,
                                                           index,
                                                           0,
                                                           num_instances);
                if (clock_rate != 90000) {
                    snprintf(message_buffer, MAX_MSG_LENGTH,
                             "an unsupported clock rate "
                             "in a=rtpmap line of media session %d. "
                             CHECK_DOCUMENT, index);
                    syslog_print(CFG_VALIDATION_WARN,
                                 channel_p->session_key,
                                 message_buffer);
                    return MAX_SESSIONS;
                }

                /* Check for encoded name */
                encoded_name = sdp_attr_get_rtpmap_encname(sdp_p, index,
                                                           0, num_instances);
                /* Unicast Retransmission Stream */
                if (strcasecmp(encoded_name, "rtx") == 0) {
                    /* Make sure that the address is unicast */
                    if (sdp_is_mcast_addr(sdp_p, index) == FALSE &&
                        sdp_get_media_transport(sdp_p, index)
                        == SDP_TRANSPORT_RTPAVPF) {
                        return UNICAST_RTX_STREAM;
                    }
                    else {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "an illegal media protocol or address "
                                 "in m= line of media session %d"
                                 CHECK_DOCUMENT, index);
                        syslog_print(CFG_VALIDATION_WARN,
                                     channel_p->session_key,
                                     message_buffer);
                        return MAX_SESSIONS;
                    }
                }
                /* Orginal Multicast or Re-Sourced Stream */
                else if (strcasecmp(encoded_name, "MP2T") == 0) {
                    if (sdp_get_media_transport(sdp_p, index)
                        == SDP_TRANSPORT_RTPAVP ||
                        sdp_get_media_transport(sdp_p, index)
                        == SDP_TRANSPORT_UDP) {
                        return ORIGINAL_STREAM;
                    }
                    else {
                        if (get_bit_rate(sdp_p, index) != -1) {
                            return ORIGINAL_STREAM;
                        }
                        else {
                            return RE_SOURCED_STREAM;
                        }
                    }
                }
                /* FEC Stream */
                else if (strcasecmp(encoded_name, "parityfec") == 0) {
                    if (sdp_is_mcast_addr(sdp_p, index) &&
                        sdp_get_media_transport(sdp_p, index)
                        == SDP_TRANSPORT_RTPAVP) {
                        return FEC_FIRST_STREAM;
                    }
                    else {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "an illegal media protocol or address "
                                 "in m= line of media session %d"
                                 CHECK_DOCUMENT, index);
                        syslog_print(CFG_VALIDATION_WARN,
                                     channel_p->session_key,
                                     message_buffer);
                        return MAX_SESSIONS;
                    }
                }
                /* FEC Streams */
                else if (strcasecmp(encoded_name, "2dparityfec") == 0) {
                    if (sdp_is_mcast_addr(sdp_p, index) &&
                        sdp_get_media_transport(sdp_p, index)
                        == SDP_TRANSPORT_RTPAVP) {
                        /* Check to see any info being stored in FEC stream1 */
                        /* If not, use it. Otherwise, use FEC stream2. */
                        if (channel_p->fec_stream1.payload_type == 0) {
                            return  FEC_FIRST_STREAM;
                        }
                        else {
                            return FEC_SECOND_STREAM;
                        }
                    }
                    else {
                        snprintf(message_buffer, MAX_MSG_LENGTH,
                                 "an illegal media protocol or address "
                                 "in m= line of media session %d"
                                 CHECK_DOCUMENT, index);
                        syslog_print(CFG_VALIDATION_WARN,
                                     channel_p->session_key,
                                     message_buffer);
                        return MAX_SESSIONS;
                    }
                }
                else {
                    snprintf(message_buffer, MAX_MSG_LENGTH,
                             "an unsupported encoded name "
                             "defined in a=rtpmap line of media session %d. "
                             CHECK_DOCUMENT, index);
                    syslog_print(CFG_VALIDATION_WARN,
                                 channel_p->session_key,
                                 message_buffer);
                    return MAX_SESSIONS;
                }

            default:
                break;
        }
    }

    return MAX_SESSIONS;
}


/* Function:    get_bit_rate
 * Description: Get the bit rate from SDP media session
 * Parameters:  sdp_p           SDP handle returned by sdp_init_description.
 *              index           media session index
 * Returns:     AS value in SDP or -1 if not exist
 */
static int get_bit_rate (void *sdp_p, uint16_t index)
{
    int i, bw_value = -1;
    int total_bw_lines = sdp_get_num_bw_lines(sdp_p, index);
    sdp_bw_modifier_e mod;
    
    for (i = 0; i < total_bw_lines; i++) {
        mod = sdp_get_bw_modifier(sdp_p, index, i);
        switch(mod) {
            case SDP_BW_MODIFIER_AS:
                bw_value = sdp_get_bw_value(sdp_p, index, i);

                return bw_value;

            default:
                break;
        }
    }

    return bw_value;
}


/* Function:    check_rtcp_port
 * Description: Check the RTCP port and if zero, apply the default rule
 * Parameters:  rtp             pointer to the RTP port
 *              rtcp            pointer to rtcp port
 *              stream_name     media session name
 *              channel_p       pointer to the channel data structure
 * Returns:     N/A
 */
boolean check_rtcp_port(in_port_t *rtp, in_port_t *rtcp,
                        char *stream_name, channel_cfg_t *channel_p)
{
    char message_buffer[MAX_MSG_LENGTH];

    if (*rtcp == 0) {
        if (ntohs(*rtp) != USHRT_MAX) {
            *rtcp = *rtp + htons(1);
            return TRUE;
        }
        else {
            snprintf(message_buffer, MAX_MSG_LENGTH,
                     "RTP port being 65535 for %s stream "
                     "and no RTCP port being specified. "
                     CHECK_DOCUMENT, stream_name);
            syslog_print(CFG_VALIDATION_ERR, channel_p->session_key,
                         message_buffer);
            return FALSE;
        }
    }
    else {
        return TRUE;
    }
}


/* Function:    cp_fec
 * Description: Copy all the info for FEC stream
 * Parameters:  dest    FEC stream configuration data
 *              src     FEC stream configuration data
 * Returns:     N/A
 */
static void cp_fec (fec_stream_cfg_t *dest, fec_stream_cfg_t *src)
{
    if (src != NULL && dest != NULL) {
        dest->multicast_addr.s_addr = src->multicast_addr.s_addr;
        dest->src_addr.s_addr = src->src_addr.s_addr;
        dest->rtp_port = src->rtp_port;
        dest->rtcp_port = src->rtcp_port;
        dest->payload_type = src->payload_type;
        dest->rtcp_sndr_bw = src->rtcp_sndr_bw;
        dest->rtcp_rcvr_bw = src->rtcp_rcvr_bw;
    }
}


/* Function:    cfg_channel_parse_session_key
 * Description: Parse internal session_key
 * Parameters:  p_channel  pointer to the channel configuration
 *              user_name  pointer to the user name
 *              session_id pointer to the session id
 *              creator_addr pointer to the creator address
 * Returns:     N/A
 */
void cfg_channel_parse_session_key (channel_cfg_t *p_channel,
                                    char *user_name,
                                    char *session_id,
                                    char *creator_addr)
{
    int i;
    char *sep = "#";
    char *token, *brkt;
    char buffer[MAX_KEY_LENGTH];

    if (p_channel== NULL || user_name == NULL ||
        session_id == NULL || creator_addr == NULL ) {
        return;
    }

    memset(buffer, 0, MAX_KEY_LENGTH);
    strlcpy(buffer, p_channel->session_key, MAX_KEY_LENGTH);
    /*
     * The session key is formatted as follows:
     *
     * INIP4#<user-name>#<session-id>#<creator-address>
     *
     */
    for (i = 1, token = strtok_r(buffer, sep, &brkt);
         token;
         i++, token = strtok_r(NULL, sep, &brkt))
    {
        switch(i) {
            case 1:
                /* Skip the first string */
                break;
        
            case 2:
                strlcpy(user_name, token, SDP_MAX_STRING_LEN);
                break;

            case 3:
                strlcpy(session_id, token, SDP_MAX_STRING_LEN);
                break;

            case 4:
                strlcpy(creator_addr, token, SDP_MAX_STRING_LEN);
                break;

            default:
                VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                              "cfg_channel_parse_session_key:: failed to "
                              "parse session key %s\n",
                              p_channel->session_key);
                return;
        }
    }
}


/* Function:    cfg_channel_extract
 * Description: Extract channel information into a channel_cfg_t
 * Parameters:  sdp_p      Pointer to SDP description
 *              channel_p  Pointer to the channel configuration
 *              chan_type  Channel type (Linear or VoD)
 * Returns:     TRUE if extraction successful, FALSE otherwise
 */
boolean cfg_channel_extract(void *sdp_p,
                            channel_cfg_t *channel_p,
                            cfg_chan_type_e chan_type)
{
    boolean valid_sdp;

    if (sdp_p == NULL || channel_p == NULL) {
        return FALSE;
    }

    /* Clean up the memory */
    memset(channel_p, 0, sizeof(channel_cfg_t));

    /* Semantic validation and content extraction */
    switch (chan_type) {
        case CFG_LINEAR:
            valid_sdp = linear_validation_and_extraction(sdp_p, channel_p);
            break;
        case CFG_VOD:
            valid_sdp = vod_validation_and_extraction(sdp_p, channel_p);
            break;
        default:
            valid_sdp = FALSE;
    }

    return valid_sdp;
}


#define MAX_RTP_PKT_SIZE 1370
#define AVE_RTCP_PKT_SIZE 100
#define BITS_PER_BYTE 8
#define RTCP_MAX_INTERVAL 65534
#define RTCP_MIN_TIME 5
#define MAX_JITTER_FACTOR 1.5

/* Function:    possible_lose_xr_data
 * Description: Check whether the bit rate and BW will cost to lose XR data
 * Parameters:  channel_p       pointer to channel configuration data
 * Returns:     True or false
 */
static boolean possible_lose_xr_data (channel_cfg_t *channel_p)
{
    double pkt_rate;
    double rtcp_intvl;

    if (channel_p) {
        if (channel_p->original_rtcp_per_rcvr_bw) {
            pkt_rate = (double) channel_p->bit_rate 
                / (double) (BITS_PER_BYTE * MAX_RTP_PKT_SIZE);
            rtcp_intvl = (double) (AVE_RTCP_PKT_SIZE * BITS_PER_BYTE)
                / (double) channel_p->original_rtcp_per_rcvr_bw;

            if (rtcp_intvl < RTCP_MIN_TIME) {
                rtcp_intvl = RTCP_MIN_TIME;
            }

            if (MAX_JITTER_FACTOR * rtcp_intvl > 
                (double) RTCP_MAX_INTERVAL / pkt_rate) {
                VQE_CFG_DEBUG(CFG_DEBUG_CHANNEL, NULL,
                              "possible_lose_xr_data:: Channel %s: RTCP "
                              "interval %.1f secs, Max range represented "
                              "in 64k %.2f secs\n",
                              channel_p->session_key,
                              MAX_JITTER_FACTOR * rtcp_intvl,
                              (double) RTCP_MAX_INTERVAL / pkt_rate);
                return TRUE;
            } else {
                return FALSE;
            }
        } else {
            syslog_print(CFG_VALIDATION_WARN,
                         channel_p->session_key,
                         "no RTCP per-receiver bandwidth specified.");
            return FALSE;
        }
    } else {
        syslog_print(CFG_VALIDATION_ERR,
                     channel_p->session_key,
                     "empty channel config data pointer.");
        return FALSE;
    }
}

