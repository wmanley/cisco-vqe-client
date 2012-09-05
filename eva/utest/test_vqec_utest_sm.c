/*------------------------------------------------------------------
 *
 * Dec 2007, 
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

/*
 * test_vqec_utest_sm.c - unit test sm functions for vqec module.
 * 
 */

#if HAVE_FCC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
#include "test_vqec_utest_interposers.h"
#include "vqec_assert_macros.h"
#include "vqec_debug.h"
#include "vqec_channel_api.h"
#include "vqec_channel_private.h"
#include "vqec_sm.h"

#define VQEC_MAX_INPUT_PKT_SIZE 1500 /*$$$ very bad. need particles */    
#define VQEC_THE_TOP_MAX_PKTS 1000 /*$$$ make bigger.  eg 300 channels* 333pps * 3sec */

char *vqec_sm_log_evt_to_string (vqec_sm_log_event_t evt);
void vqec_sm_log(vqec_sm_log_event_t log_evt, vqec_chan_t *chan,
                  vqec_sm_state_t state, vqec_sm_event_t event);
boolean
vqec_sm_app_timer_start (vqec_chan_t *chan,
                         vqec_sm_state_t state, 
                         vqec_sm_event_t event, void *arg);
boolean 
vqec_sm_app_timer_stop (vqec_chan_t *chan,
                        vqec_sm_state_t state, 
                        vqec_sm_event_t event, void *arg);
void 
vqec_sm_app_timeout_handler (const vqec_event_t * const evptr,
                             int fd, short event, void *arg);


int test_vqec_sm_init(void)
{

    vqec_chan_err_t err;

    if (!g_channel_module) {
        printf("**** previous remaining channel module \n");
        vqec_chan_module_deinit();
    }
    test_malloc_make_fail(0);
    vqec_pak_pool_create(
        "CP sm", VQEC_MAX_INPUT_PKT_SIZE, VQEC_THE_TOP_MAX_PKTS);

//    VQEC_SET_DEBUG_FLAG(VQEC_DEBUG_RCC);
    err = vqec_chan_module_init();
    if (err != VQEC_CHAN_ERR_OK) {
        printf(" **** channel init fails \n");
    }
    if (!g_channel_module) {
        printf("channel init success \n");
    } else {
        printf("**** channel init fails\n");
    }
    return 0;
}

int test_vqec_sm_clean(void)
{
    vqec_pak_pool_destroy();
    vqec_chan_module_deinit();
    return 0;
}


static void test_vqec_sm_event_to_string(void)
{
    vqec_sm_event_t event;

    event = VQEC_EVENT_RAPID_CHANNEL_CHANGE;
    CU_ASSERT(strcmp(vqec_sm_event_to_string(event), 
                     "RAPID_CHANNEL_CHANGE") == 0);
    event = VQEC_EVENT_SLOW_CHANNEL_CHANGE;
    CU_ASSERT(strcmp(vqec_sm_event_to_string(event), 
                     "SLOW_CHANNEL_CHANGE") == 0);
    event = VQEC_EVENT_NAT_BINDING_COMPLETE;
    CU_ASSERT(strcmp(vqec_sm_event_to_string(event),
                     "NAT_BINDING_COMPLETE") == 0);
    event = VQEC_EVENT_RECEIVE_VALID_APP;
    CU_ASSERT(strcmp(vqec_sm_event_to_string(event),
                     "RECEIVE_VALID_APP") == 0);
    event = VQEC_EVENT_RECEIVE_INVALID_APP;
    CU_ASSERT(strcmp(vqec_sm_event_to_string(event), 
                     "RECEIVE_INVALID_APP") == 0);
    event = VQEC_EVENT_RECEIVE_NULL_APP;
    CU_ASSERT(strcmp(vqec_sm_event_to_string(event),
                     "RECEIVE_NULL_APP") == 0);
    event = VQEC_EVENT_RCC_START_TIMEOUT;
    CU_ASSERT(strcmp(vqec_sm_event_to_string(event),
                     "RCC_START_TIMEOUT") == 0);
    event = VQEC_EVENT_RCC_INTERNAL_ERR;
    CU_ASSERT(strcmp(vqec_sm_event_to_string(event), 
                     "RCC_INTERNAL_ERROR") == 0);
    event = 1000;
    CU_ASSERT(strcmp(vqec_sm_event_to_string(event), 
                     "UNKNOWN_ERROR") == 0);
}

static void test_vqec_sm_state_to_string (void)
{
    vqec_sm_state_t state;

    state = VQEC_SM_STATE_INVALID;
    CU_ASSERT(strcmp(vqec_sm_state_to_string(state), 
                     "STATE_INVALID") == 0);
    state = VQEC_SM_STATE_INIT;
    CU_ASSERT(strcmp(vqec_sm_state_to_string(state),
                     "STATE_INIT") == 0);
    state =  VQEC_SM_STATE_WAIT_APP;
    CU_ASSERT(strcmp(vqec_sm_state_to_string(state),
                     "STATE_WAIT_APP") == 0);
    state = VQEC_SM_STATE_ABORT;
    CU_ASSERT(strcmp(vqec_sm_state_to_string(state), 
                     "STATE_ABORT") == 0);
    state = VQEC_SM_STATE_FIN_SUCCESS;
    CU_ASSERT(strcmp(vqec_sm_state_to_string(state), 
                     "STATE_SUCCESS") == 0);
    state = 1000;
    CU_ASSERT(strcmp(vqec_sm_state_to_string(state),
                     "UNKNOWN_STATE") == 0);
}

static void test_vqec_sm_log_evt_to_string(void)
{
    uint32_t event;

    event = VQEC_SM_LOG_STATE_EVENT;
    CU_ASSERT(strcmp(vqec_sm_log_evt_to_string(event), "FSM_EVENT") == 0);
    event = VQEC_SM_LOG_STATE_ENTER;
    CU_ASSERT(strcmp(vqec_sm_log_evt_to_string(event), "STATE_ENTER") == 0);
    event =  VQEC_SM_LOG_STATE_EXIT;
    CU_ASSERT(strcmp(vqec_sm_log_evt_to_string(event), "STATE_EXIT") == 0);
    event = 1000;
    CU_ASSERT(strcmp(vqec_sm_log_evt_to_string(event), "UKNOWN_EVENT") == 0);
}

static void test_vqec_sm_log(void)
{
    vqec_chan_t chan;
    vqec_sm_state_t state;
    vqec_sm_event_t event;
    uint32_t log_evt;
    memset(&chan, 0, sizeof(vqec_chan_t));
    chan.sm_log_tail = 1;
    log_evt = 2;
    state = VQEC_SM_STATE_INIT;
    event = VQEC_EVENT_RAPID_CHANNEL_CHANGE;
    vqec_sm_log(log_evt,&chan,state,event);
    CU_ASSERT(chan.sm_log[1].log_event == 2);
    CU_ASSERT(chan.sm_log[1].state == VQEC_SM_STATE_INIT);
    CU_ASSERT(chan.sm_log[1].event == VQEC_EVENT_RAPID_CHANNEL_CHANGE);
    CU_ASSERT(chan.sm_log_tail == 2);

    vqec_sm_display_log(&chan);
    vqec_sm_print();
}


static void test_vqec_sm_deliver_event(void)
{
    vqec_chan_t chan;
    boolean res = TRUE;
    vqec_sm_event_t event; 

    memset(&chan, 0, sizeof(vqec_chan_t));
    vqec_sm_init(&chan);

    event =  VQEC_EVENT_RAPID_CHANNEL_CHANGE;
    res = vqec_sm_deliver_event(&chan, event, NULL);
    CU_ASSERT(res == TRUE);
    CU_ASSERT(chan.state == VQEC_SM_STATE_WAIT_APP);
    vqec_sm_deinit(&chan);

    memset (&chan,0, sizeof(vqec_chan_t));
    vqec_sm_init(&chan);

    event =  VQEC_EVENT_SLOW_CHANNEL_CHANGE;
    res = vqec_sm_deliver_event(&chan, event, NULL);
    CU_ASSERT(res == TRUE);
    CU_ASSERT(chan.state == VQEC_SM_STATE_FIN_SUCCESS);

    memset (&chan,0, sizeof(vqec_chan_t));
    chan.state = VQEC_SM_STATE_WAIT_APP;
    event =  VQEC_EVENT_NAT_BINDING_COMPLETE;
    res = vqec_sm_deliver_event(&chan, event, NULL);
    CU_ASSERT(res == FALSE);
    printf("chan.state1 = %s\n",vqec_sm_state_to_string(chan.state));
    CU_ASSERT(chan.state == VQEC_SM_STATE_ABORT);

    chan.state = VQEC_SM_STATE_WAIT_APP;
    event =  VQEC_EVENT_RECEIVE_VALID_APP;
    res = vqec_sm_deliver_event(&chan, event, NULL);
    CU_ASSERT(res == TRUE);
    printf("chan.state2 = %s\n",vqec_sm_state_to_string(chan.state));
    CU_ASSERT(chan.state == VQEC_SM_STATE_FIN_SUCCESS);

    memset (&chan,0, sizeof (vqec_chan_t));
    chan.state = VQEC_SM_STATE_WAIT_APP;
    event =  VQEC_EVENT_RECEIVE_INVALID_APP;
    res = vqec_sm_deliver_event(&chan, event, NULL);
    CU_ASSERT(res == TRUE);
    CU_ASSERT(chan.state == VQEC_SM_STATE_ABORT);

    memset (&chan,0, sizeof (vqec_chan_t));
    chan.state = VQEC_SM_STATE_WAIT_APP;
    event =  VQEC_EVENT_RECEIVE_NULL_APP;
    res = vqec_sm_deliver_event(&chan, event, NULL);
    CU_ASSERT(res == TRUE);
    CU_ASSERT(chan.state == VQEC_SM_STATE_ABORT);

    memset (&chan,0, sizeof (vqec_chan_t));
    chan.state = VQEC_SM_STATE_WAIT_APP;
    event =  VQEC_EVENT_RCC_START_TIMEOUT;
    res = vqec_sm_deliver_event(&chan, event, NULL);
    CU_ASSERT(res == TRUE);
    CU_ASSERT(chan.state == VQEC_SM_STATE_ABORT);
 
    memset (&chan,0, sizeof (vqec_chan_t));
    chan.state = VQEC_SM_STATE_FIN_SUCCESS;
    event =  VQEC_EVENT_RECEIVE_NULL_APP;
    res = vqec_sm_deliver_event(&chan, event, NULL);
    CU_ASSERT(res == TRUE);
    CU_ASSERT(chan.state == VQEC_SM_STATE_FIN_SUCCESS);


    memset (&chan,0, sizeof (vqec_chan_t));
    chan.state = VQEC_SM_STATE_ABORT;
    event =  VQEC_EVENT_RECEIVE_NULL_APP;
    res = vqec_sm_deliver_event(&chan, event, NULL);
    CU_ASSERT(res == TRUE);
    CU_ASSERT(chan.state == VQEC_SM_STATE_ABORT);

}


void test_vqec_sm_app_timer_start_stop(void)
{
    vqec_chan_t chan;
    vqec_sm_state_t state = VQEC_SM_STATE_INIT;
    vqec_sm_event_t event =  VQEC_EVENT_RAPID_CHANNEL_CHANGE;
    boolean res = FALSE;

    memset(&chan, 0, sizeof(vqec_chan_t)); 
    chan.app_timeout_interval = TIME_MK_R(msec, 2000);
    res = vqec_sm_app_timer_start(&chan, state, event, NULL);
    CU_ASSERT(res == TRUE);
    CU_ASSERT(chan.app_timeout != NULL);

    res = vqec_sm_app_timer_stop(&chan, state, event, NULL);

    CU_ASSERT(res == TRUE);
    CU_ASSERT(chan.app_timeout == NULL);
}

void test_vqec_sm_app_timeout_handler(void)
{
    vqec_chan_t chan;
    vqec_sm_event_t event = VQEC_EVENT_RAPID_CHANNEL_CHANGE;

    memset(&chan, 0, sizeof(vqec_chan_t)); 
    chan.state = VQEC_SM_STATE_WAIT_APP;

    vqec_sm_app_timeout_handler(NULL, 1, event, (void*)&chan);

    CU_ASSERT(chan.state == VQEC_SM_STATE_ABORT);
}


CU_TestInfo test_array_sm[] = {
    {"test_vqec_sm_event_to_string", test_vqec_sm_event_to_string},
    {"test_vqec_sm_state_to_string",test_vqec_sm_state_to_string},
    {"test_vqec_sm_log_evt_to_string",test_vqec_sm_log_evt_to_string},
    {"test_vqec_sm_log",test_vqec_sm_log},
    {"test_vqec_sm_deliver_event",test_vqec_sm_deliver_event},
    {"test_vqec_sm_app_timer_start_stop",test_vqec_sm_app_timer_start_stop},
    {"test_vqec_sm_app_timeout_handler",test_vqec_sm_app_timeout_handler},
    CU_TEST_INFO_NULL,
};

#endif   /* HAVE_FCC */
