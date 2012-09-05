/******************************************************************************
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: vqec_error.h
 *
 * Description: VQEC error return codes.
 *
 * Documents:
 *
 *****************************************************************************/
#include "vqec_error_macros.h"

ARR_BEGIN(vqec_error_arr)
ARR_ELEM(VQEC_OK,                   
         "Successful")
ARR_ELEM(VQEC_ERR_DISABLED,
         "VQE-C is disabled")
ARR_ELEM(VQEC_OK_IGMP_PROXY_DISABLED, 
         "Successfully disabled IGMP proxy")
ARR_ELEM(VQEC_ERR_TIMERADD,         
         "Adding RTP event timer failed")
ARR_ELEM(VQEC_ERR_EVENTCREATE,         
         "Event creation failed")
ARR_ELEM(VQEC_ERR_CHANNELPARSE,     
         "Channel URL parse error")
ARR_ELEM(VQEC_ERR_CHANNELLOOKUP,    
         "Channel not present in database")
ARR_ELEM(VQEC_ERR_DUPLICATECHANNELTUNEREQ,
         "Tuner already tuned to this channel")
ARR_ELEM(VQEC_ERR_SESSIONINIT,      
         "RTP session module initialization failed")
ARR_ELEM(VQEC_ERR_SESSIONCREATE,    
         "RTP session creation failed")
ARR_ELEM(VQEC_ERR_SOCKETCREATE,     
         "Socket creation failed")
ARR_ELEM(VQEC_ERR_MALLOC,           
         "Memory allocation failed")
ARR_ELEM(VQEC_ERR_INVALIDARGS,      
         "A parameter specified in the input arguments is invalid")
ARR_ELEM(VQEC_ERR_NOSUCHTUNER,      
         "Tuner does not exist")
ARR_ELEM(VQEC_ERR_EXISTTUNER,       
         "Tuner already exists")
ARR_ELEM(VQEC_ERR_CREATETUNER,      
         "Error allocating tuner resources")
ARR_ELEM(VQEC_ERR_MAXLIMITTUNER,    
         "Max tuner limit as specified in configuration is reached")
ARR_ELEM(VQEC_ERR_ALREADYINITIALIZED,       
         "Failure in re-initialization")
ARR_ELEM(VQEC_ERR_NOTINITIALIZED,
         "Initialization required before use")
ARR_ELEM(VQEC_ERR_NOBOUNDCHAN,      
         "No channel is associated to the tuner")
ARR_ELEM(VQEC_ERR_CHANNOTACTIVE,
         "Channel is not currently bound to any tuner")
ARR_ELEM(VQEC_ERR_NOCHANNELLINEUP,  
         "Channel lineup could not be found")
ARR_ELEM(VQEC_ERR_NOSTUNSERVER,     
         "Stun server could not be found")
ARR_ELEM(VQEC_ERR_SYSCALL,          
         "A system call failed")
ARR_ELEM(VQEC_ERR_INVCLIENTSTATE,   
         "Client interface not initialized or in invalid state")
ARR_ELEM(VQEC_ERR_CONFLICTSRC,
         "Multiple channels are using the same src transport address")
ARR_ELEM(VQEC_ERR_CREATETHREAD,
         "Failed to create thread")
ARR_ELEM(VQEC_ERR_JOINTHREAD, 
         "Failed to join thread")
ARR_ELEM(VQEC_ERR_DESTROYTHREAD, 
         "Failed to destroy thread")
ARR_ELEM(VQEC_ERR_SETCHANCFG,
         "Failed to set the channel configuration")
ARR_ELEM(VQEC_ERR_CNAME_REG,
         "Failed to register the CNAME override function")
ARR_ELEM(VQEC_ERR_BADRTPHDR,
         "Invalid RTP header")
ARR_ELEM(VQEC_ERR_CONFIG_NOT_FOUND,          
         "Configuration file not present.")
ARR_ELEM(VQEC_ERR_ALREADYREGISTERED,
         "Configuration event handler is already registered")
ARR_ELEM(VQEC_ERR_PARAMRANGEINVALID,
         "A parameter value specified in the configuration is invalid")
ARR_ELEM(VQEC_ERR_CONFIG_CHECKSUM_VERIFICATION,
         "Configuration checksum verification failed")
ARR_ELEM(VQEC_ERR_CONFIG_INVALID,
         "Configuration contents not syntactically valid")
ARR_ELEM(VQEC_ERR_UPDATE_FAILED_EXISTING_INTACT,
         "Configuration update failed, existing config remains intact")
ARR_ELEM(VQEC_ERR_UPDATE_FAILED_EXISTING_REMOVED,
         "Configuration update failed, existing config was removed")
ARR_ELEM(VQEC_ERR_UPDATE_INIT_FAILURE,
         "Updater initialization failure")
ARR_ELEM(VQEC_ERR_NO_RPC_SUPPORT,
         "No RPC support for this function call")
ARR_ELEM(VQEC_ERR_INTERNAL,
         "Internal error")
ARR_ELEM(VQEC_ERR_UNKNOWN,          
         "Unknown error")
ARR_ELEM(VQEC_ERR_MAX,              "")
ARR_DONE
