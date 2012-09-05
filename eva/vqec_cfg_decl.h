/********************************************************************
 * vqec_cfg_decl.h
 *
 * Macros and Data structure templates for the VQE Configuration
 *
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 **********************************************************************/

#ifndef __VQEC_CFG_DECL_H__
#define __VQEC_CFG_DECL_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum vqec_param_type_ {
    VQEC_TYPE_STRING,
    VQEC_TYPE_BOOLEAN,
    VQEC_TYPE_UINT32_T
} vqec_param_type_t;

typedef struct vqec_cfg_item__ {
    char *param_name;
    vqec_param_type_t param_type;
    char *param_comment;
} vqec_cfg_item_t;

typedef enum vqec_cfg_attrib_update_type_ {
    VQEC_UPDATE_INVALID,        /* no attribute-update is possible */
    VQEC_UPDATE_STARTUP,        /* attribute updated at startup */
    VQEC_UPDATE_IMMEDIATE,      /* attribute updated immediately */
    VQEC_UPDATE_NEWCHANCHG,     /* attribute update at new channel bind  */
    /* Add new types here */
} vqec_cfg_attrib_update_type_t;
    
/*
 * Status of a VQE-C configuration parameter.
 *
 * Properties of a parameter based on its status are listed below.
 *
 *                             CURRENT         DEPRECATED        OBSOLETE
 *
 * 1.                                                 
 * effect on VQE-C syscfg?     yes             only when         no
 *                                             a successor
 *                                             parameter is
 *                                             not also
 *                                             supplied
 * 
 * 2.
 * included in                 yes             no                no
 * "show syscfg"
 * "show syscfg default" 
 * displays?
 *
 * 3.
 * included in                 yes             yes               yes
 * "show syscfg startup"       (when           (when             (when
 * "show syscfg network"        configured &    configured &      configured &
 * "show syscfg override"       supported)      supported)        supported)
 * displays?
 *
 * 4.
 * warning issued when          no             yes,              yes,
 * parameter is detected                       "deprecated"      "obsolete"
 * in a config file?
 *
 * NOTE:  A parameter's status defines these properties, though
 *        properties 1 and 4 require some manual edits to implement 
 *        these properties in addition to simply setting this value.
 *        e.g. when obsoleting a parmeter:
 *               - its field should be removed from vqec_syscfg_t, 
 *               - various case statements in vqec_syscfg.c which handle
 *                 the parameter should be updated (see existing examples)
 *             when deprecating a parameter:
 *               - vqec_syscfg_read() should be updated to assign the current
 *                 parameter based on the deprecated parameter, if configured
 *                 (see existing examples)
 *               - dependencies in VQE-C on the deprecated parameter should
 *                 be converted to dependencies on the current parameter
 *               - various case statements in vqec_syscfg.c which handle
 *                 the parameter should be updated (see existing examples)
 */
typedef enum vqec_cfg_param_status_t_ {
    VQEC_PARAM_STATUS_CURRENT = 0,
    VQEC_PARAM_STATUS_DEPRECATED,
    VQEC_PARAM_STATUS_OBSOLETE
} vqec_cfg_param_status_t;
    
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VQEC_CFG_DECL_H */
