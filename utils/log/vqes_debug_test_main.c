/********************************************************************
 * vqes_debug_test_main.c
 *
 * Test VQES debug and syslog message logging
 *
 *
 * Copyright (c) 2007-2011 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <inttypes.h>

/* VQES-CP */
/*------------------------------------------------------------------------*/
/* Among all the C files including it, only ONE C file (say the one where 
 * main() is defined) should instantiate the message definitions described in 
 * XXX_syslog_def.h.   To do so, the designated C file should define the symbol
 * SYSLOG_DEFINITION before including the file.  
 */
#define SYSLOG_DEFINITION
#include <log/vqes_cp_syslog_def.h>
#undef SYSLOG_DEFINITION

/* Include base definitions: boolean flags, enum codes */
#include <log/vqes_cp_debug.h>
/* Declare the debug array */
#define __DECLARE_DEBUG_ARR__
#include <log/vqes_cp_debug_flags.h>
/*------------------------------------------------------------------------*/


/* RTP */
/*------------------------------------------------------------------------*/
#define SYSLOG_DEFINITION
#include <log/vqe_rtp_syslog_def.h>
#undef SYSLOG_DEFINITION

#include "utils/vam_util.h"   /* for uint32_ntoa_r() */

/* Include base definitions: boolean flags, enum codes */
#include <log/vqe_rtp_debug.h>
/* Declare the debug array */
#define __DECLARE_DEBUG_ARR__
#include <log/vqe_rtp_debug_flags.h>
/*------------------------------------------------------------------------*/


/* Define TEST_OTHER_INCLUDE to check if other debug include files compile 
 * or not.
 */
//#define TEST_OTHER_INCLUDE
#ifdef TEST_OTHER_INCLUDE

#define SYSLOG_DEFINITION
#include <log/vqes_dp_syslog_def.h>
#include <log/vqes_pm_syslog_def.h>
#include <log/vqes_mlb_syslog_def.h>
#include <log/vqe_rtsp_syslog_def.h>
#undef SYSLOG_DEFINITION

#include <log/vqes_dp_debug.h>
#define __DECLARE_DEBUG_ARR__
#include <log/vqes_dp_debug_flags.h>

#include <log/vqes_mlb_debug.h>
#define __DECLARE_DEBUG_ARR__
#include <log/vqes_mlb_debug_flags.h>

#include <log/vqes_pm_debug.h>
#define __DECLARE_DEBUG_ARR__
#include <log/vqes_pm_debug_flags.h>

#include <log/vqe_rtsp_debug.h>
#define __DECLARE_DEBUG_ARR__
#include <log/vqe_rtsp_debug_flags.h>

#endif /* #if TEST_OTHER_INCLUDE */

extern void test_vqes_cp_chan_log(uint32_t chan_ip, uint32_t chan_port);
extern void test_vqes_cp_stb_log(uint32_t stb_ip);

extern void test_librtp_log(uint32_t ssrc);

int main(int argc, char *argv[])
{
    char str[INET_ADDRSTRLEN];
    boolean flag_state;
    uint8_t prio_filter = LOG_ERR;
    debug_filter_item_t cp_filter, *ret_filter, *p_null_filter=NULL;

    /*--------  May take a user specified logging priority --------*/
    if (argc == 2)
	prio_filter = atoi(argv[1]);

    /* LOG_DEBUG is the largest priority number */
    if (prio_filter > LOG_DEBUG)
	prio_filter = LOG_DEBUG;
    /*-------------------------------------------------------------*/
    

    /*--------- First thing is to openlog for the process ---------*/
    syslog_facility_open(LOG_VQES_CP, LOG_CONS);
    /*-------------------------------------------------------------*/


    /*-------- Set the the process facility prioity level ---------*/
    syslog_facility_filter_set(LOG_VQES_CP, prio_filter);
    /* Setting logmask is now part of the syslog_facility_filter_set macro.
     * The above macro is also changed to always set the LOG_DEBUG mask so that
     * debug messages are logged as soon as a debug flag is turned on.
     *
     * No need to do vqes_setlogmask() anymore.
     */
    //vqes_setlogmask(LOG_UPTO(prio_filter)); 
    printf("Syslog facility filter set to %d\n", prio_filter);
    /*-------------------------------------------------------------*/


    /*---- Sync up syslog facilitiy priorities with the process's ----*/
    /* Note you may call this function again for a different library */
    sync_lib_facility_filter_with_process(LOG_VQES_CP, LOG_RTP);
    /*----------------------------------------------------------------*/


    /*----- Just checking: get back the current facility priority -----*/
    syslog_facility_filter_get(LOG_VQES_CP, prio_filter);
    printf("Got process syslog facility (%s): "
           "priority filter=%d; use_stderr=%d\n", 
           syslog_fac_LOG_VQES_CP.fac_name, prio_filter, 
           syslog_fac_LOG_VQES_CP.use_stderr);

    syslog_facility_filter_get(LOG_RTP, prio_filter);
    printf("Got library syslog facility (%s): "
           "priority filter=%d; use_stderr=%d\n", 
           syslog_fac_LOG_RTP.fac_name, prio_filter,
           syslog_fac_LOG_RTP.use_stderr);
    /*----------------------------------------------------------------*/


    /*-------------------------- Test logging ------------------------*/
    /* Simulate turning on the ERA_DEBUG_ER flag and setting up a filter.
     * Note they normally would be initiated from an application UI.
     */

    /* Test Control Plane logging */
    VQES_CP_SET_DEBUG_FLAG(ERA_DEBUG_ER);
    VQES_CP_SET_DEBUG_FLAG(ERA_DEBUG_FCC);

    cp_filter.type = DEBUG_FILTER_TYPE_CHANNEL;
    /* only log debug messages for Channel 224.1.1.2:50000 */
    cp_filter.val = CONCAT_U32_U16_TO_U64(inet_addr("224.1.1.2"), 
                                          htons(50000));
    
    /* Negative test for the set_filter function */
    VQES_CP_SET_FILTER(ERA_DEBUG_ER, p_null_filter);

    /* Note a filter is only set for a debug flag after the flag 
       is set to TRUE*/
    VQES_CP_SET_FILTER(ERA_DEBUG_ER, &cp_filter);
    VQES_CP_SET_FILTER(ERA_DEBUG_FCC, &cp_filter);
    
    /* Test the get macro */
    if (VQES_CP_GET_DEBUG_FLAG_STATE(ERA_DEBUG_ER, &flag_state, &ret_filter)
        == DEBUG_FILTER_NOT_SET) {
        printf(">>> Debug filter not set for flag %d\n", ERA_DEBUG_ER);
    } else {
        /* Note: to access the opaque filter vals, a real application needs to
         * check the returned filter type.
         */
        printf(">>> Get flag %d's state: %d;  filter: type=%d, val=0x%"PRIu64"\n",
               ERA_DEBUG_ER, flag_state, ret_filter->type, ret_filter->val);
    }

    printf("\n>>> Only debug messages of Channel %s:%u should be logged\n",
           uint32_ntoa_r(cp_filter.val>>16, str, sizeof(str)), 
           ntohs(cp_filter.val&0xffff));

    test_vqes_cp_chan_log(inet_addr("224.1.1.2"), htons(1000));
    test_vqes_cp_chan_log(inet_addr("224.1.1.2"), htons(50000));
    test_vqes_cp_chan_log(inet_addr("225.1.1.5"), htons(34560));

    /* Now change to log debug messages only for Channel 225.1.1.5:34560 */
    cp_filter.val = CONCAT_U32_U16_TO_U64(inet_addr("225.1.1.5"), 
                                          htons(34560));

    VQES_CP_SET_FILTER(ERA_DEBUG_ER, &cp_filter);
    VQES_CP_SET_FILTER(ERA_DEBUG_FCC, &cp_filter);

    printf("\n>>> Only debug messages of Channel 225.1.1.5:34560 should be "
           "logged\n");
    test_vqes_cp_chan_log(inet_addr("224.1.1.2"), htons(1000));
    test_vqes_cp_chan_log(inet_addr("224.1.1.2"), htons(50000));
    test_vqes_cp_chan_log(inet_addr("225.1.1.5"), htons(34560));


    /* Clear/reset the filter: now all debug messages should be seen */
    printf("\n>>> Clear/reset the filter: now all debug messages should be "
           "seen\n");
    VQES_CP_RESET_FILTER(ERA_DEBUG_ER);
    VQES_CP_RESET_FILTER(ERA_DEBUG_FCC);
    test_vqes_cp_chan_log(inet_addr("224.1.1.2"), htons(1000));
    test_vqes_cp_chan_log(inet_addr("224.1.1.2"), htons(50000));
    test_vqes_cp_chan_log(inet_addr("225.1.1.5"), htons(34560));


    /* Turn off debug flags: no debug messages should be logged */
    printf("\n>>> Turn off debug flags: no debug messages should be logged\n");
    VQES_CP_RESET_DEBUG_FLAG(ERA_DEBUG_ER);
    VQES_CP_RESET_DEBUG_FLAG(ERA_DEBUG_FCC);

    test_vqes_cp_chan_log(inet_addr("224.1.1.2"), htons(1000));
    test_vqes_cp_chan_log(inet_addr("224.1.1.2"), htons(50000));
    test_vqes_cp_chan_log(inet_addr("225.1.1.5"), htons(34560));


    /*- - - - - - - - - - - - - - - - - - - - -*/
    /* Test Control Plane logging */
    VQES_CP_SET_DEBUG_FLAG(ERA_DEBUG_ER);
    VQES_CP_SET_DEBUG_FLAG(ERA_DEBUG_FCC);

    cp_filter.type = DEBUG_FILTER_TYPE_STB_IP;
    /* only log debug messages for STB 23.34.45.56 */
    cp_filter.val = inet_addr("23.34.45.56");
    VQES_CP_SET_FILTER(ERA_DEBUG_ER, &cp_filter);
    VQES_CP_SET_FILTER(ERA_DEBUG_FCC, &cp_filter);

    printf("\n\n>>> Only debug messages of STB %s should be logged\n", 
           uint32_ntoa_r(cp_filter.val, str, sizeof(str)));
    test_vqes_cp_stb_log(inet_addr("10.20.30.40"));
    test_vqes_cp_stb_log(inet_addr("167.24.3.12"));
    test_vqes_cp_stb_log(inet_addr("23.34.45.56"));

    /* only log debug messages for STB 10.20.30.40 */
    cp_filter.val = inet_addr("10.20.30.40");
    VQES_CP_SET_FILTER(ERA_DEBUG_ER, &cp_filter);
    VQES_CP_SET_FILTER(ERA_DEBUG_FCC, &cp_filter);


    printf("\n>>> Only debug messages of STB %s should be logged\n", 
           uint32_ntoa_r(cp_filter.val, str, sizeof(str)));
    test_vqes_cp_stb_log(inet_addr("10.20.30.40"));
    test_vqes_cp_stb_log(inet_addr("167.24.3.12"));
    test_vqes_cp_stb_log(inet_addr("23.34.45.56"));


    /* Turn off debug flags: no debug messages should be logged */
    printf("\n>>> Turn off debug flags: no debug messages should be logged\n");
    VQES_CP_RESET_DEBUG_FLAG(ERA_DEBUG_ER);
    VQES_CP_RESET_DEBUG_FLAG(ERA_DEBUG_FCC);

    test_vqes_cp_stb_log(inet_addr("10.20.30.40"));
    test_vqes_cp_stb_log(inet_addr("167.24.3.12"));
    test_vqes_cp_stb_log(inet_addr("23.34.45.56"));

    /*----------------------------------------------------------------*/    


    /*-------- Close logging facility in your close() routine --------*/
    syslog_facility_close();
    /*----------------------------------------------------------------*/    

    return 0;
}
