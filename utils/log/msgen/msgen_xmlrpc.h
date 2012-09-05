/**-----------------------------------------------------------------
 * @brief
 * Test Tool: msgen. xmlrpc interface
 *
 * @file
 * msgen_xmlrpc.h
 *
 * November 2007, Donghai Ma
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef _MSGEN_XMLRPC_H_
#define _MSGEN_XMLRPC_H_

#include <netinet/in.h>
#include "utils/vam_types.h"
#include "xmlrpc_server.h"

#define MSGEN_COMPONENT_NAME "msgen"

#define BUFFER_LEN        1024
#define SMALL_BUFFER_LEN  64

#define XMLRPC_MSG_NAME_LEN  32
#define XMLRPC_CB_HELP_LEN   32


typedef enum msgen_xmlrpc_msg_enum_ {
    /* Add new message after the first one */
    MSGEN_FIRST_MSG = 0,

    MSGEN_HELP = MSGEN_FIRST_MSG,
    MSGEN_HELP_1,
    MSGEN_HELP_2,
    MSGEN_SHOW,
    MSGEN_SET,
    MSGEN_ONTSHOT_BURST,
    MSGEN_CHECK_SML,

    /* Add new message before the last one */
    MSGEN_LAST_MSG
} msgen_xmlrpc_msg_enum;


/* xmlrpc message callback function pointer */
typedef xmlrpc_value* (*xmlrpc_cb_t)(
    xmlrpc_env *   const env, 
    xmlrpc_value * const param_array, 
    void *         const user_data ATTR_UNUSED);


/* xmlrpc message and its associated callback routine */
typedef struct xmlrpc_msg_t_ {
    char name[XMLRPC_MSG_NAME_LEN];
    xmlrpc_cb_t cb_method;
    char cb_arg[XMLRPC_CB_HELP_LEN];
} xmlrpc_msg_t;



extern xmlrpc_msg_t msgen_xmlrpc_msgs[];


/* Register the xmlrpc methods and startup the xmlrpc server thread */
extern boolean msgen_xmlrpc_start (in_port_t xmlrpc_port, char *xmlrpc_log);


#endif /* _MSGEN_XMLRPC_H_ */
