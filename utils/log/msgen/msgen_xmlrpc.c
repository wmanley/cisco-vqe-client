/**-----------------------------------------------------------------
 * @brief
 * Test Tool: msgen - xmlrpc interface
 *
 * @file
 * msgen_xmlrpc.c
 *
 * November 2007, Donghai Ma
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */
#include <errno.h>

#include "log/vqes_cp_syslog_def.h"
#include "log/vqes_syslog_limit.h"

#include "utils/vam_util.h"

#include "msgen_para.h"
#include "msgen_xmlrpc.h"

#define BUFFER_SIZE  64

/* Not thread safe: but it's ok for this test tool */
extern uint16_t xmlrpc_port;

typedef enum {
    XMLRPC_BOOL,
    XMLRPC_INT,
    XMLRPC_UINT,
    XMLRPC_FLOAT,
    XMLRPC_DOUBLE,
    XMLRPC_CHAR,
    XMLRPC_INT64
} stats_type_e;

/* Function:	build_xml_struct
 * Description:	Build the xmlrpc structure for a given stats
 * Parameters:	N/A
 * Returns:	xml formatted data
 */
static xmlrpc_value *build_xml_struct (xmlrpc_env *const env,
                                       xmlrpc_value *retval,
                                       const char *stats_name,
                                       void *value,
                                       stats_type_e type)
{
    xmlrpc_value *stats_value = NULL;
    char buffer[BUFFER_SIZE];

    switch (type) {
        case XMLRPC_BOOL:
            stats_value = xmlrpc_build_value(env,
                                             "{s:b}",
                                             stats_name,
                                             *((boolean*)value));
            xmlrpc_array_append_item(env, retval, stats_value);
            xmlrpc_DECREF(stats_value);

            break;

        case XMLRPC_INT:
            stats_value = xmlrpc_build_value(env,
                                             "{s:i}",
                                             stats_name,
                                             *((int*)value));
            xmlrpc_array_append_item(env, retval, stats_value);
            xmlrpc_DECREF(stats_value);

            break;

        case XMLRPC_UINT:
            snprintf(buffer, BUFFER_SIZE,
                     "%u", *((int*)value));
            stats_value = xmlrpc_build_value(env,
                                             "{s:s}",
                                             stats_name,
                                             buffer);
            xmlrpc_array_append_item(env, retval, stats_value);
            xmlrpc_DECREF(stats_value);

            break;

        case XMLRPC_FLOAT:
            snprintf(buffer, BUFFER_SIZE,
                     "%.3f", *((float*)value));
            stats_value = xmlrpc_build_value(env,
                                             "{s:s}",
                                             stats_name,
                                             buffer);
            xmlrpc_array_append_item(env, retval, stats_value);
            xmlrpc_DECREF(stats_value);

            break;

        case XMLRPC_DOUBLE:
            snprintf(buffer, BUFFER_SIZE,
                     "%.3f", *((double*)value));
            stats_value = xmlrpc_build_value(env,
                                             "{s:s}",
                                             stats_name,
                                             buffer);
            xmlrpc_array_append_item(env, retval, stats_value);
            xmlrpc_DECREF(stats_value);

            break;

        case XMLRPC_CHAR:
            stats_value = xmlrpc_build_value(env,
                                             "{s:s}",
                                             stats_name,
                                             (char*)value);
            xmlrpc_array_append_item(env, retval, stats_value);
            xmlrpc_DECREF(stats_value);

            break;

        case XMLRPC_INT64:
            snprintf(buffer, BUFFER_SIZE,
                     "%llu", *((uint64_t*)value));
            stats_value = xmlrpc_build_value(env,
                                             "{s:s}",
                                             stats_name,
                                             buffer);
            xmlrpc_array_append_item(env, retval, stats_value);
            xmlrpc_DECREF(stats_value);

            break;
    }

    if (retval == NULL) {
        xmlrpc_env_set_fault(env,
                             XMLRPC_INVALID_DATA_ERROR_CODE,
                             XMLRPC_INVALID_DATA_ERROR_MSG);
    }

    return retval;
}


static char string_buffer[BUFFER_SIZE];

char *CONV_U64_TO_STRING (uint64_t value)
{
    memset(string_buffer, 0, BUFFER_SIZE);
    snprintf(string_buffer, BUFFER_SIZE,
             "%llu", value);

    return string_buffer;
}



/* Three steps to add a new xmlrpc message:
 *  1. Add an enum to msgen_xmlrpc_msg_enum.
 *  2. Define the message callback routine: the real work.
 *  3. Add an entry to array msgen_xmlrpc_msgs[].
 */

/* Forward declaration */
xmlrpc_msg_t msgen_xmlrpc_msgs[];




/* Function:	build_suppressed_msg_value
 * Description:	Pack info of a syslog message into a xmlrpc array of structs:
 *                  Key:  "msg name"
 *                  Value: Array of N items:
 *                         Index 0 Struct of 1 members:
 *                         Index 0 Key:   String "para name"
 *                         Index 0 Value: Struct "para value"
 *
 *                         Index 1 Struct of 1 members:
 *                         Index 1 Key:   String "para name"
 *                         Index 1 Value: Struct "para value"
 *                  
 * Parameters:	env[in]:   xmlrpc env struct
 *              p_msg[in]: ptr to a msg struct
 * Returns:	xml formatted data
 */
static xmlrpc_value *build_suppressed_msg_value (xmlrpc_env *const env,
                                                 syslog_msg_def_t *p_msg)
{
    xmlrpc_value *retval;
    xmlrpc_value *msg_value = NULL;

    msg_value = xmlrpc_array_new(env);

    if (!msg_value) {
        goto done;
    }

    if ((build_xml_struct(env, msg_value, 
                          "Name", 
                          p_msg->msg_name,
                          XMLRPC_CHAR) == NULL) || 
        (build_xml_struct(env, msg_value,
                          "Facility",
                          p_msg->facility->fac_name,
                          XMLRPC_CHAR) == NULL) || 
        (build_xml_struct(env, msg_value,
                          "Format",
                          p_msg->output_format,
                          XMLRPC_CHAR) == NULL)) {
        return NULL;
    }
    
    if (p_msg->limiter) {
        char ts_buf[BUFFER_SIZE], string_buffer[BUFFER_SIZE];
        snprintf(string_buffer, BUFFER_SIZE, "%llu", 
                 p_msg->limiter->suppressed);

        abs_time_t now = get_sys_time();
        rel_time_t last_sup_time = TIME_SUB_A_A(now, 
                                                p_msg->limiter->last_time);

        if ((build_xml_struct(env, msg_value,
                              "Limit rate",
                              &(p_msg->limiter->rate_limit),
                              XMLRPC_UINT) == NULL) || 
            (build_xml_struct(env, msg_value,
                              "Suppressed last time (sec ago)",
                              (char *)rel_time_to_str(
                                  last_sup_time, ts_buf, BUFFER_SIZE),
                              XMLRPC_CHAR) == NULL) || 
            (build_xml_struct(env, msg_value,
                              "Suppressed message count",
                              string_buffer,
                              XMLRPC_CHAR) == NULL)) {
            return NULL;
        }
    }

done:
    retval = xmlrpc_build_value(env, "{s:A}",
                                p_msg->msg_name,
                                msg_value);
    xmlrpc_DECREF(msg_value);

    return retval;
}



/* To show the xmlrpc message list */
static xmlrpc_value*
msgen_help (xmlrpc_env *   const env, 
            xmlrpc_value * const param_array, 
            void *         const user_data ATTR_UNUSED)
{
    xmlrpc_value *retval = NULL;
    msgen_xmlrpc_msg_enum i;

    /* Parse params */
    if (env->fault_occurred){
        return NULL;

    } else{
        xmlrpc_value *command_struct = NULL;
        char command_line[BUFFER_LEN];

        retval = xmlrpc_array_new(env);

        /* Loop to add each xmlrpc method to array */
        for (i = MSGEN_FIRST_MSG; i < MSGEN_LAST_MSG; i++) {
            snprintf(command_line, BUFFER_LEN,
                     "xmlrpc localhost:%d %s %s",
                     xmlrpc_port, 
                     msgen_xmlrpc_msgs[i].name, msgen_xmlrpc_msgs[i].cb_arg);
            command_struct = xmlrpc_build_value(env, "s",
                                                command_line);
            xmlrpc_array_append_item(env, retval, command_struct);
            xmlrpc_DECREF(command_struct);
        }

        if(env->fault_occurred){
            retval = NULL;
        }
    }

    return (retval);
}    


/* Message for showing both the configured parameters, the stats, and the
 * suppressed messages on the SML, in the following format:
 *   Array of M items:
 *     Index 0 Struct of 1 member:
 *     Index 0 Key:   "msgen parameters and stats"
 *     Index 0 Value: Array of 4 items:
 *
 *     Index 1 Struct of 1 member:
 *     Index 1 Key:  "msg name"
 *     Index 1 Value: Array of N items:
 *              Index 0 Struct of 1 members:
 *              Index 0 Key:   String "para name"
 *              Index 0 Value: Struct "para value"
 *
 *              Index 1 Struct of 1 members:
 *              Index 1 Key:   String "para name"
 *              Index 1 Value: Struct "para value"
 *
 *     [one for each message on the SML]
 */
static xmlrpc_value*
msgen_show (xmlrpc_env *   const env, 
            xmlrpc_value * const param_array, 
            void *         const user_data ATTR_UNUSED)
{
    xmlrpc_value *retval = NULL;
    syslog_msg_def_t *p_msg = NULL;

    xmlrpc_value *msgen_info, *msgen_struct;
    xmlrpc_value *sup_msg_struct;

    uint64_t msg_sent;
    uint32_t msg_rate, msg_burst;
    boolean  oneshot_burst;
    
    if (env->fault_occurred) {
        retval = NULL;
        goto done;
    }

    /* Get the parameters from the msgen_para api */
    get_msgen_paras(&msg_rate, &msg_burst, &oneshot_burst, &msg_sent);

    retval = xmlrpc_array_new(env);

    /* msgen information */
    msgen_info = xmlrpc_array_new(env);

    if ((build_xml_struct(env, msgen_info,
                          "Message rate",
                          &(msg_rate),
                          XMLRPC_UINT) == NULL) ||
        (build_xml_struct(env, msgen_info,
                          "Message burst",
                          &(msg_burst),
                          XMLRPC_UINT) == NULL) ||
        (build_xml_struct(env, msgen_info,
                          "Oneshot burst flag",
                          &(oneshot_burst),
                          XMLRPC_BOOL) == NULL) ||
        (build_xml_struct(env, msgen_info,
                          "Message sent",
                          CONV_U64_TO_STRING(msg_sent),
                          XMLRPC_CHAR) == NULL)) {
        retval = NULL;
        goto done;
    }

    msgen_struct = xmlrpc_build_value(env, "{s:A}",
                                      "msgen config and stats",
                                      msgen_info);
    xmlrpc_DECREF(msgen_info);
    xmlrpc_array_append_item(env, retval, msgen_struct);
    xmlrpc_DECREF(msgen_struct);

    /* Walk the SML and build one xmlrpc data struct for each msg on SML */
    for (p_msg = get_first_sup_msg(); 
         p_msg; 
         p_msg = get_next_sup_msg(p_msg)) {

        /* Build the message struct and append to the array */
        sup_msg_struct = build_suppressed_msg_value(env, p_msg);
        xmlrpc_array_append_item(env, retval, sup_msg_struct);
        xmlrpc_DECREF(sup_msg_struct);
    }

done:
    return (retval);
}


/* Set runtime parameters
 *    XMLRPC message syntax: Set [Rate|Burst] i/<value>
 *    where the unit for the <value> is:
 *        message per second for Rate
 *        messages for Burst
 */
static xmlrpc_value*
msgen_set (xmlrpc_env *   const env, 
           xmlrpc_value * const param_array, 
           void *         const user_data ATTR_UNUSED)
{
    xmlrpc_value *retval = NULL;    
    const char *var_name = NULL;   /* "rate" or "burst" */
    size_t len;
    uint32_t value;
    char response_msg[BUFFER_LEN];

    /* Parse the params */
    xmlrpc_decompose_value(env, param_array, "(s#i)", &var_name,
                           &len, &value);
    if (env->fault_occurred) {
        retval = NULL;

    } else {
        if (strcasecmp(var_name, "Rate") == 0) { 
            set_msg_rate(value);
            snprintf(response_msg, BUFFER_LEN, "Set Rate to %d", value);

        } else if (strcasecmp(var_name, "Burst") == 0 ) {
            set_msg_burst(value);
            snprintf(response_msg, BUFFER_LEN, "Set Burst to %d", value);
        } else {
            snprintf(response_msg, BUFFER_LEN,
                     "Invalid variable name \"%s\"", var_name);
        }

        retval = xmlrpc_build_value(env,
                                    "s",
                                    response_msg);
    }

    if (var_name) {
        free((void *)var_name);
    }

    return retval;
}


/* Message to start an oneshot burst. The number of the messages to 
 * burst is set by msg_burst.
 */
static xmlrpc_value*
msgen_oneshot_burst (xmlrpc_env *   const env, 
                     xmlrpc_value * const param_array, 
                     void *         const user_data ATTR_UNUSED)
{
    xmlrpc_value *retval = NULL;    
    char response_msg[BUFFER_LEN];
    static boolean flag = FALSE;  /* the oneshot flag is default to FALSE */

    if (env->fault_occurred) {
        retval = NULL;
    } else {
        flag = !flag;
        set_oneshot_burst_flag(flag);

        snprintf(response_msg, BUFFER_LEN,
                 "Set oneshot_burst to %s", flag?"TRUE":"FALSE");
        retval = xmlrpc_build_value(env, "s", response_msg);
    }
    
    return (retval);
}


/* Gateway to check_sml(), which checks for staled suppressed messages
 * on the Suppressed Message List (SML).
 */
static xmlrpc_value*
msgen_check_sml (xmlrpc_env *   const env, 
                 xmlrpc_value * const param_array, 
                 void *         const user_data ATTR_UNUSED)
{
    xmlrpc_value *retval = NULL;    
    char response_msg[BUFFER_LEN];

    if (env->fault_occurred) {
        retval = NULL;
    } else {

        check_sml(ABS_TIME_0);

        snprintf(response_msg, BUFFER_LEN,
                 "To check Suppressed Message List");
        retval = xmlrpc_build_value(env, "s", response_msg);
    }
    
    return (retval);
}


/* Array to hold the XMLRPC messages and their callback routines */
xmlrpc_msg_t msgen_xmlrpc_msgs[] = {
    {"Help",              msgen_help,               ""},
    {"help",              msgen_help,               ""},
    {"h",                 msgen_help,               ""},
    {"Show",              msgen_show,               ""},
    {"Set",               msgen_set,                "[Rate|Burst] i/<value>"},
    {"Oneshot-Burst",     msgen_oneshot_burst,      ""},
    {"Check-SML",         msgen_check_sml    ,      ""},
};

/**
 * Register the xmlrpc callback routines and startup the xmlrpc server thread
 *
 * @param xmlrpc_port		- port on which the xmlrpc server is listening
 * @param xmlrpc_log		- log file
 * @return TRUE if successful
 */
boolean msgen_xmlrpc_start (in_port_t xmlrpc_port, char *xmlrpc_log)
{
    msgen_xmlrpc_msg_enum i;

    /* Initialize XML-RPC Server */
    if (xmlrpc_server_init(xmlrpc_port, xmlrpc_log) !=  XMLRPC_SUCCESS) {
        printf("Error init XMLRPC server\n");
        return FALSE;
    }
		
    /* Associate callbacks with the messages */
    for (i = MSGEN_FIRST_MSG; i < MSGEN_LAST_MSG; i++) {
        if (xmlrpc_server_addMethod(msgen_xmlrpc_msgs[i].name,
                                    msgen_xmlrpc_msgs[i].cb_method,
                                    NULL) != XMLRPC_SUCCESS) {
            printf("Cannot add callback %s", msgen_xmlrpc_msgs[i].name);
            return FALSE;
        }
    }
	
    /* Start the server thread */
    if (xmlrpc_server_start() != XMLRPC_SUCCESS) {
        printf("Error starting XMLRPC server\n");
        return FALSE;
    }

    return TRUE;
}


