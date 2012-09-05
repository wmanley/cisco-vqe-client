/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008, 2010 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: NAT unit tests.
 *
 * Documents: 
 *
 *****************************************************************************/

#include "test_vqec_utest_interposers.h"
#include "vqec_nat_interface.h"
#include "vqec_event.h"
#include "vqec_syscfg.h"
#include "vqec_pthread.h"
#include "vqec_debug.h"
#include <stun/stun.h>
#include <stun/stun_private.h>
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
#include <stdio.h>
#include <sys/wait.h>

/**---------------------------------------------------------------------------
 * Libevent thread handler. 
 *---------------------------------------------------------------------------*/  
static void *
test_stunproto_run_libevent (void *in)
{
    printf("== start libevent dispatch ==\n");
    vqec_event_dispatch();    
    printf("== end libevent dispatch ==\n");
    return ((void *)0);
}


/**---------------------------------------------------------------------------
 * Initialize the NAT unit-tests. 
 *---------------------------------------------------------------------------*/  
int32_t test_vqec_nat_init (void) 
{
    if (!vqec_event_init()) {
        printf("== libevent initalization failure ==\n");
    }
    return 0;
}

/**---------------------------------------------------------------------------
 * Cleanup after NAT unit-tests. 
 *---------------------------------------------------------------------------*/  
int test_vqec_nat_clean (void) 
{
    return 0;
}


/**---------------------------------------------------------------------------
 * stun-protocol: destruction.
 *---------------------------------------------------------------------------*/  
static void 
test_nat_stunproto_destroy (void)
{
    vqec_nat_module_deinit();
    CU_ASSERT(TRUE);
}


/**---------------------------------------------------------------------------
 * stun-protocol: instantiation.
 *---------------------------------------------------------------------------*/  
static void 
test_nat_stunproto_create (void)
{
#define VQEC_NATPROTO_INVALID_BINDINGS (VQEC_NAT_MAX_BINDINGS + 1)
#define VQEC_NATPROTO_REFRESH_INTERVAL 5
#define VQEC_NATPROTO_MAXPAKSIZE 1500

    vqec_error_t err;
    vqec_nat_init_params_t nat_params;

    memset(&nat_params, 0, sizeof(nat_params));
    nat_params.max_bindings = VQEC_NAT_MAX_BINDINGS;
    nat_params.refresh_interval = VQEC_NATPROTO_REFRESH_INTERVAL;
    nat_params.max_paksize = VQEC_NATPROTO_MAXPAKSIZE;

    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_NUM_VAL,  /* invalid */
                               &nat_params);
    CU_ASSERT(err != VQEC_OK);

    nat_params.max_bindings = 0;  /* invalid */
    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_STUN,
                               &nat_params);
    CU_ASSERT(err != VQEC_OK);

    nat_params.max_bindings = VQEC_NATPROTO_INVALID_BINDINGS;  /* invalid */
    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_STUN,
                               &nat_params);
    CU_ASSERT(err != VQEC_OK);

    nat_params.max_bindings = VQEC_NAT_MAX_BINDINGS;
    nat_params.refresh_interval = VQEC_NATPROTO_REFRESH_INTERVAL;
    nat_params.max_paksize = 0; /* invalid */
    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_STUN,
                               &nat_params);
    CU_ASSERT(err != VQEC_OK);

    nat_params.max_paksize = VQEC_NATPROTO_MAXPAKSIZE;
    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_STUN,
                               &nat_params);
    CU_ASSERT(err == VQEC_OK);

    test_nat_stunproto_destroy();
    test_vqec_clear_nat_inject_desc();
}


/**---------------------------------------------------------------------------
 * stun-protocol: exhaust bind identifiers.
 *---------------------------------------------------------------------------*/  
static void
test_nat_stunproto_exhaust_ids (void)
{
#define VQEC_NATPROTO_LONG_REFRESH_INTERVAL 90
#define VQEC_NATPROTO_BASE_ADDR 0x1000
    vqec_error_t err;
    uint32_t i;
    vqec_nat_bind_desc_t desc;
    vqec_nat_bindid_t id[VQEC_NAT_MAX_BINDINGS], last_id;
    vqec_nat_bind_data_t data;
    vqec_nat_init_params_t nat_params;

    memset(&nat_params, 0, sizeof(nat_params));
    nat_params.max_bindings = VQEC_NAT_MAX_BINDINGS;
                                /* don't allow NAT timers to pop */
    nat_params.refresh_interval = VQEC_NATPROTO_LONG_REFRESH_INTERVAL;
    nat_params.max_paksize = VQEC_NATPROTO_MAXPAKSIZE;

    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_STUN,
                               &nat_params);
    CU_ASSERT(err == VQEC_OK);

    for (i = 0; i < VQEC_NAT_MAX_BINDINGS; i++) {
        memset(&desc, 0, sizeof(desc));
        desc.internal_addr = i + VQEC_NATPROTO_BASE_ADDR;
        desc.internal_port = i + VQEC_NATPROTO_BASE_ADDR;
        
        id[i] = vqec_nat_open_binding(&desc);
        CU_ASSERT(id[i] != VQEC_NAT_BINDID_INVALID);        
    }

    desc.internal_addr = i + VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = i + VQEC_NATPROTO_BASE_ADDR;
    last_id = vqec_nat_open_binding(&desc);
    CU_ASSERT(last_id == VQEC_NAT_BINDID_INVALID);    

    for (i = 0; i < VQEC_NAT_MAX_BINDINGS; i++) {
        vqec_nat_close_binding(id[i]);
        CU_ASSERT(!vqec_nat_query_binding(id[i], &data, FALSE));
    }

    for (i = 0; i < VQEC_NAT_MAX_BINDINGS; i++) {
        memset(&desc, 0, sizeof(desc));
        desc.internal_addr = i + VQEC_NATPROTO_BASE_ADDR;
        desc.internal_port = i + VQEC_NATPROTO_BASE_ADDR;
        
        id[i] = vqec_nat_open_binding(&desc);
        CU_ASSERT(id[i] != VQEC_NAT_BINDID_INVALID);        
    }

    test_nat_stunproto_destroy();
    test_vqec_clear_nat_inject_desc();
}


/**---------------------------------------------------------------------------
 * stun-protocol: open binding with collision.
 *---------------------------------------------------------------------------*/  
static void
test_nat_stunproto_open_collide (void)
{
    vqec_error_t err;
    vqec_nat_bind_desc_t desc;
    vqec_nat_bindid_t id;
    vqec_nat_init_params_t nat_params;

    memset(&nat_params, 0, sizeof(nat_params));
    nat_params.max_bindings = VQEC_NAT_MAX_BINDINGS;
                                /* don't allow NAT timers to pop */
    nat_params.refresh_interval = VQEC_NATPROTO_LONG_REFRESH_INTERVAL;
    nat_params.max_paksize = VQEC_NATPROTO_MAXPAKSIZE;

    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_STUN,
                               &nat_params);
    CU_ASSERT(err == VQEC_OK);
    
    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;
    
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

                                /* use same 4-tuple */
    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;
    
                                /* collision must fail */
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id == VQEC_NAT_BINDID_INVALID); 

                                /* change any tuple of the 4-tuple */
    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;
    desc.remote_port++;
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;
    desc.remote_addr++;
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR + 1;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR ;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR + 1;
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

    test_nat_stunproto_destroy();    
    test_vqec_clear_nat_inject_desc();
}


/**---------------------------------------------------------------------------
 * stun-protocol: open binding and query state.
 *---------------------------------------------------------------------------*/  
static void
test_nat_stunproto_query_refresh (void)
{
#define MAX_NATPROTO_ERR_LEN 256
#define NATPROTO_REFRESH_TEST_TIMEOUT_SECS 15
#define NATPROTO_MAX_RETRY_COUNT 9
    vqec_error_t err;
    vqec_nat_bind_desc_t desc;
    vqec_nat_bindid_t id;
    vqec_nat_bind_data_t data;
    pthread_t tid;
    int32_t status, retry_cnt = 0;
    FILE *fp;
    pid_t pid, cpid;
    char *str, buf[MAX_NATPROTO_ERR_LEN], *token, *next_token;
    vqec_nat_init_params_t nat_params;

    memset(&nat_params, 0, sizeof(nat_params));
    nat_params.max_bindings = VQEC_NAT_MAX_BINDINGS;
                                /* don't allow NAT timers to pop */
    nat_params.refresh_interval = VQEC_NATPROTO_LONG_REFRESH_INTERVAL;
    nat_params.max_paksize = VQEC_NATPROTO_MAXPAKSIZE;
    
    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_STUN,
                               &nat_params);
    CU_ASSERT(err == VQEC_OK);
    
    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;
    
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

    memset(&data, 0, sizeof(data));
                                /* query without an update */
    CU_ASSERT(vqec_nat_query_binding(id, &data, FALSE));
                                /* no NAT messages sent yet */
    CU_ASSERT(!data.is_map_valid); 
    CU_ASSERT(data.state == VQEC_NAT_BIND_STATE_UNKNOWN);
                                /* test "internal" unknown state map */
    CU_ASSERT(data.desc.internal_addr == data.ext_addr);
    CU_ASSERT(data.desc.internal_port == data.ext_port);

    memset(&data, 0, sizeof(data));
                                /* query with an update */
    CU_ASSERT(vqec_nat_query_binding(id, &data, TRUE));
                                /* invalid channel identifier - inject fails */
    CU_ASSERT(!data.is_map_valid); 
                                /* test "internal" unknown state map */
    CU_ASSERT(data.desc.internal_addr == data.ext_addr);
    CU_ASSERT(data.desc.internal_port == data.ext_port);

                                /* allow for RFC retrial-refresh */

                                /* fork the parent */
    pid = fork();
    if (!pid) {
                                /* child process */

                                /* reopen stderr in child */
        fp = freopen("natlog", "w", stderr);
        CU_ASSERT(fp != NULL);
        CU_ASSERT(
            !vqec_pthread_create(&tid, test_stunproto_run_libevent, NULL)); 
        (void)pthread_detach(tid);
        sleep(NATPROTO_REFRESH_TEST_TIMEOUT_SECS);
        fflush(fp);
        fclose(fp);
        vqec_event_loopexit(NULL);
        exit(0);

    } else {
                                /* parent process */
        
                                /* wait for child to exit */
        cpid = waitpid(pid, &status, 0);
        
                                /* parse syslog log file */
        fp = fopen("natlog", "r");
        CU_ASSERT(fp != NULL);
        while ((str = fgets(buf, sizeof(buf), fp))) {
            token = strtok_r(str,
                             "socket", &next_token);
            if (token) {
                retry_cnt++;
            }
        }
    }

                                /* first attempt at query time */
    CU_ASSERT(retry_cnt == NATPROTO_MAX_RETRY_COUNT - 1);
    fclose(fp);
    test_nat_stunproto_destroy(); 
    test_vqec_clear_nat_inject_desc();
}


/**---------------------------------------------------------------------------
 * stun-protocol: test for periodic updates.
 *---------------------------------------------------------------------------*/  
static void
test_nat_stunproto_periodic_update (void)
{
#define VQEC_NATPROTO_SHORT_REFRESH_INTERVAL 1
#define NATPROTO_TIMER_TEST_TIMEOUT_SECS 5
#define NATPROTO_MIN_RETRIES 4  /* at least 4 retries in 5 secs */
    vqec_error_t err;
    vqec_nat_bind_desc_t desc;
    vqec_nat_bindid_t id;
    pthread_t tid;
    int32_t status, retry_cnt = 0;
    FILE *fp;
    pid_t pid, cpid;
    char *str, buf[MAX_NATPROTO_ERR_LEN], *token, *next_token;
    vqec_nat_init_params_t nat_params;

    memset(&nat_params, 0, sizeof(nat_params));
    nat_params.max_bindings = VQEC_NAT_MAX_BINDINGS;
    nat_params.refresh_interval = VQEC_NATPROTO_SHORT_REFRESH_INTERVAL;
    nat_params.max_paksize = VQEC_NATPROTO_MAXPAKSIZE;
        
    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_STUN,
                               &nat_params);
    CU_ASSERT(err == VQEC_OK);
    
    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;   
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR + 1;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR + 1;
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

                                /* allow for timer retrial-refresh */

                                /* fork the parent */
    pid = fork();
    if (!pid) {
                                /* child process */

                                /* reopen stderr in child */
        fp = freopen("natlog", "w", stderr);
        CU_ASSERT(fp != NULL);
        CU_ASSERT(
            !vqec_pthread_create(&tid, test_stunproto_run_libevent, NULL)); 
        (void)pthread_detach(tid);
        sleep(NATPROTO_TIMER_TEST_TIMEOUT_SECS);
        fflush(fp);
        fclose(fp);
        vqec_event_loopexit(NULL);
        exit(0);

    } else {
                                /* parent process */
        
                                /* wait for child to exit */
        cpid = waitpid(pid, &status, 0);
        
                                /* parse log file */
        fp = fopen("natlog", "r");
        CU_ASSERT(fp != NULL);
        while ((str = fgets(buf, sizeof(buf), fp))) {
            token = strtok_r(str,
                             "socket", &next_token);
            if (token) {
                retry_cnt++;
            }
        }
    }

                                /* 2 open bindings * 4 */
    CU_ASSERT(retry_cnt >= (NATPROTO_MIN_RETRIES*2));
    fclose(fp);
    test_nat_stunproto_destroy();
    test_vqec_clear_nat_inject_desc();
}


/**---------------------------------------------------------------------------
 * Build a server response.
 *---------------------------------------------------------------------------*/  
static char resp_buf[VQEC_NATPROTO_MAXPAKSIZE];
static uint32_t resp_len;
static void
test_nat_build_response (struct test_vqec_nat_inject_t  *nats)
{
  uint32_t ret_code;
  stun_message_t stun_msg;
  uint8_t *msg;
  uint32_t len;

    memset(resp_buf, 0, sizeof(resp_buf));
    resp_len = 0;

    ret_code = stun_message_parse((uint8_t *)nats->buf, nats->len, 
                                  &stun_msg, false);
    CU_ASSERT(ret_code == 0);
    if (stun_msg.stun_header.message_type == STUN_BINDING_REQ) {
        stun_msg.mapped_address.content.family = STUN_ADDRESS_FAMILY_IPV4;
        stun_msg.mapped_address.content.port = ntohs(VQEC_NATPROTO_BASE_ADDR);
        stun_msg.mapped_address.content.addr.ipv4_addr.s_addr = 
            VQEC_NATPROTO_BASE_ADDR;
        stun_msg.mapped_address.is_valid = true;
        msg = stun_generate_binding_response(&stun_msg, &len);
        if (msg && len && (len < VQEC_NATPROTO_MAXPAKSIZE)) {
            memcpy(resp_buf, msg, len);
            resp_len = len;
        }
        
        if (msg) {       
            free(msg);
        }
    }    
}


/**---------------------------------------------------------------------------
 * stun-protocol: eject a packet to the protocol from a separate context.
 *---------------------------------------------------------------------------*/  
static void *
test_stunproto_run_eject (void *arg)
{    
    in_addr_t source_ip= VQEC_NATPROTO_BASE_ADDR;
    in_port_t source_port= VQEC_NATPROTO_BASE_ADDR;

    vqec_nat_eject_to_binding(*(vqec_nat_bindid_t *)arg, 
                              resp_buf, 0,
                              source_ip, source_port);
    CU_ASSERT(TRUE);
    vqec_nat_eject_to_binding(*(vqec_nat_bindid_t *)arg, 
                              NULL, resp_len,
                              source_ip, source_port);
    CU_ASSERT(TRUE);

    vqec_nat_eject_to_binding(*(vqec_nat_bindid_t *)arg, 
                              resp_buf, resp_len,
                              source_ip, source_port);
    CU_ASSERT(TRUE);

    return ((void *)0);
}


/**---------------------------------------------------------------------------
 * stun-protocol: eject a packet to the protocol.
 *---------------------------------------------------------------------------*/  
static void
test_nat_stunproto_eject (void)
{
    vqec_error_t err;
    vqec_nat_bind_desc_t desc;
    vqec_nat_bindid_t id;
    vqec_nat_bind_data_t data;
    struct test_vqec_nat_inject_t nats;
    pthread_t tid;
    vqec_nat_init_params_t nat_params;

    memset(&nat_params, 0, sizeof(nat_params));
    nat_params.max_bindings = VQEC_NAT_MAX_BINDINGS;
                                /* don't allow timer to pop */
    nat_params.refresh_interval = VQEC_NATPROTO_LONG_REFRESH_INTERVAL;
    nat_params.max_paksize = VQEC_NATPROTO_MAXPAKSIZE;
    
    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_STUN,
                               &nat_params);
    CU_ASSERT(err == VQEC_OK);

    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;   
    desc.remote_addr = VQEC_NATPROTO_BASE_ADDR;
    desc.remote_port = VQEC_NATPROTO_BASE_ADDR;   
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

                                /* allow NAT inject to succeed */
    test_vqec_nat_inject_force(TRUE);
    memset(&data, 0, sizeof(data));
                                /* query with an update */
    CU_ASSERT(vqec_nat_query_binding(id, &data, TRUE));
                                /* invalid channel identifier - inject fails */
    CU_ASSERT(!data.is_map_valid); 
                                /* test "internal" unknown state map */
    CU_ASSERT(data.desc.internal_addr == data.ext_addr);
    CU_ASSERT(data.desc.internal_port == data.ext_port);
    
                                /* get the contents of data sent */
    test_vqec_get_nat_inject_desc(&nats);
    CU_ASSERT(nats.inject_cnt == 1);

                                /* Build a server response */
    test_nat_build_response(&nats);
    CU_ASSERT(resp_len != 0);
    
                                /* thread to eject back-in */
    CU_ASSERT(
        !vqec_pthread_create(&tid, test_stunproto_run_eject, &id)); 
    pthread_join(tid, NULL);
    CU_ASSERT(vqec_nat_query_binding(id, &data, FALSE));
    CU_ASSERT(data.is_map_valid); 

    test_nat_stunproto_destroy(); 
    test_vqec_nat_inject_force(FALSE);
    test_vqec_clear_nat_inject_desc();
}


/**---------------------------------------------------------------------------
 * stun-protocol: display.
 *---------------------------------------------------------------------------*/  
static void 
test_nat_stunproto_print (void)
{
    vqec_error_t err;
    vqec_nat_bind_desc_t desc;
    vqec_nat_bindid_t id;
    vqec_nat_init_params_t nat_params;

    memset(&nat_params, 0, sizeof(nat_params));
    nat_params.max_bindings = VQEC_NAT_MAX_BINDINGS;
                                /* don't allow timer to pop */
    nat_params.refresh_interval = VQEC_NATPROTO_LONG_REFRESH_INTERVAL;
    nat_params.max_paksize = VQEC_NATPROTO_MAXPAKSIZE;
    
    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_STUN,
                               &nat_params);
    CU_ASSERT(err == VQEC_OK);

    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 
    vqec_nat_fprint_binding(id);

    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR + 1;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR + 1;    
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

    vqec_nat_fprint_all_bindings();    
    
    test_nat_stunproto_destroy();
    test_vqec_clear_nat_inject_desc();
}


/**---------------------------------------------------------------------------
 * nat-protocol: destruction.
 *---------------------------------------------------------------------------*/  
static void 
test_nat_natproto_destroy (void)
{
    vqec_nat_module_deinit();
    CU_ASSERT(TRUE);
}


/**---------------------------------------------------------------------------
 * nat-protocol: instantiation.
 *---------------------------------------------------------------------------*/  
static void 
test_nat_natproto_create (void)
{
    vqec_error_t err;
    vqec_nat_init_params_t nat_params;

    memset(&nat_params, 0, sizeof(nat_params));
    nat_params.max_bindings = 0; /* invalid */
    nat_params.refresh_interval = VQEC_NATPROTO_REFRESH_INTERVAL;  /* ignored */
    nat_params.max_paksize = VQEC_NATPROTO_MAXPAKSIZE;    
    
    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_NULL,
                               &nat_params);
    CU_ASSERT(err != VQEC_OK);

    nat_params.max_bindings = VQEC_NATPROTO_INVALID_BINDINGS; 
    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_NULL,
                               &nat_params);
    CU_ASSERT(err != VQEC_OK);

    nat_params.max_bindings = VQEC_NAT_MAX_BINDINGS; 
    nat_params.max_paksize = 0; /* invalid */
    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_NULL,
                               &nat_params);
    CU_ASSERT(err != VQEC_OK);

    nat_params.max_bindings = VQEC_NAT_MAX_BINDINGS; 
    nat_params.refresh_interval = VQEC_NATPROTO_REFRESH_INTERVAL;  /* ignored */
    nat_params.max_paksize = VQEC_NATPROTO_MAXPAKSIZE;
    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_NULL,
                               &nat_params);
    CU_ASSERT(err == VQEC_OK);

    test_nat_natproto_destroy();
}


/**---------------------------------------------------------------------------
 * nat-protocol: exhaust bind identifiers.
 *---------------------------------------------------------------------------*/  
static void
test_nat_natproto_exhaust_ids (void)
{
    vqec_error_t err;
    uint32_t i;
    vqec_nat_bind_desc_t desc;
    vqec_nat_bindid_t id[VQEC_NAT_MAX_BINDINGS], last_id;
    vqec_nat_bind_data_t data;
    vqec_nat_init_params_t nat_params;

    memset(&nat_params, 0, sizeof(nat_params));
    nat_params.max_bindings = VQEC_NAT_MAX_BINDINGS;
    nat_params.refresh_interval = 0;  /* ignored */
    nat_params.max_paksize = VQEC_NATPROTO_MAXPAKSIZE;

    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_NULL,
                               &nat_params);
    CU_ASSERT(err == VQEC_OK);

    for (i = 0; i < VQEC_NAT_MAX_BINDINGS; i++) {
        memset(&desc, 0, sizeof(desc));
        desc.internal_addr = i + VQEC_NATPROTO_BASE_ADDR;
        desc.internal_port = i + VQEC_NATPROTO_BASE_ADDR;
        
        id[i] = vqec_nat_open_binding(&desc);
        CU_ASSERT(id[i] != VQEC_NAT_BINDID_INVALID);        
    }

    desc.internal_addr = i + VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = i + VQEC_NATPROTO_BASE_ADDR;
    last_id = vqec_nat_open_binding(&desc);
    CU_ASSERT(last_id == VQEC_NAT_BINDID_INVALID);    

    for (i = 0; i < VQEC_NAT_MAX_BINDINGS; i++) {
        vqec_nat_close_binding(id[i]);
        CU_ASSERT(!vqec_nat_query_binding(id[i], &data, FALSE));
    }

    for (i = 0; i < VQEC_NAT_MAX_BINDINGS; i++) {
        memset(&desc, 0, sizeof(desc));
        desc.internal_addr = i + VQEC_NATPROTO_BASE_ADDR;
        desc.internal_port = i + VQEC_NATPROTO_BASE_ADDR;
        
        id[i] = vqec_nat_open_binding(&desc);
        CU_ASSERT(id[i] != VQEC_NAT_BINDID_INVALID);        
    }

    test_nat_natproto_destroy();
}


/**---------------------------------------------------------------------------
 * nat-protocol: open binding with collision.
 *---------------------------------------------------------------------------*/  
static void
test_nat_natproto_open_collide (void)
{
    vqec_error_t err;
    vqec_nat_bind_desc_t desc;
    vqec_nat_bindid_t id;
    vqec_nat_init_params_t nat_params;

    memset(&nat_params, 0, sizeof(nat_params));
    nat_params.max_bindings = VQEC_NAT_MAX_BINDINGS;
    nat_params.refresh_interval = 0;  /* ignored */
    nat_params.max_paksize = VQEC_NATPROTO_MAXPAKSIZE;

    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_NULL,
                               &nat_params);
    CU_ASSERT(err == VQEC_OK);
    
    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;
    
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

                                /* use same 4-tuple */
    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;
    
                                /* collision must fail */
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id == VQEC_NAT_BINDID_INVALID); 

                                /* change any tuple of the 4-tuple */
    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;
    desc.remote_port++;
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;
    desc.remote_addr++;
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR + 1;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR ;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR + 1;
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

    test_nat_natproto_destroy();    
}


/**---------------------------------------------------------------------------
 * nat-protocol: open binding and query state.
 *---------------------------------------------------------------------------*/  
static void
test_nat_natproto_query (void)
{
    vqec_error_t err;
    vqec_nat_bind_desc_t desc;
    vqec_nat_bindid_t id;
    vqec_nat_bind_data_t data;
    vqec_nat_init_params_t nat_params;

    memset(&nat_params, 0, sizeof(nat_params));
    nat_params.max_bindings = VQEC_NAT_MAX_BINDINGS;
    nat_params.refresh_interval = 0;  /* ignored */
    nat_params.max_paksize = VQEC_NATPROTO_MAXPAKSIZE;
    
    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_NULL,
                               &nat_params);
    CU_ASSERT(err == VQEC_OK);
    
    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;
    
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

    memset(&data, 0, sizeof(data));
                                /* query without an update */
    CU_ASSERT(vqec_nat_query_binding(id, &data, FALSE));
                                /* no NAT messages sent yet */
    CU_ASSERT(data.is_map_valid); 
    CU_ASSERT(data.state == VQEC_NAT_BIND_STATE_NOTBEHINDNAT);
                                /* test not-behind state map */
    CU_ASSERT(data.desc.internal_addr == data.ext_addr);
    CU_ASSERT(data.desc.internal_port == data.ext_port);

    test_nat_natproto_destroy(); 
}


/**---------------------------------------------------------------------------
 * nat-protocol: display.
 *---------------------------------------------------------------------------*/  
static void 
test_nat_natproto_print (void)
{
    vqec_error_t err;
    vqec_nat_bind_desc_t desc;
    vqec_nat_bindid_t id;
    vqec_nat_init_params_t nat_params;

    memset(&nat_params, 0, sizeof(nat_params));
    nat_params.max_bindings = VQEC_NAT_MAX_BINDINGS;
    nat_params.refresh_interval = 0;  /* ignored */
    nat_params.max_paksize = VQEC_NATPROTO_MAXPAKSIZE;
    
    err = vqec_nat_module_init(VQEC_SM_NAT,
                               VQEC_NAT_PROTO_NULL,
                               &nat_params);
    CU_ASSERT(err == VQEC_OK);

    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR;
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 
    vqec_nat_fprint_binding(id);

    memset(&desc, 0, sizeof(desc));
    desc.internal_addr = VQEC_NATPROTO_BASE_ADDR + 1;
    desc.internal_port = VQEC_NATPROTO_BASE_ADDR + 1;    
    id = vqec_nat_open_binding(&desc);
    CU_ASSERT(id != VQEC_NAT_BINDID_INVALID); 

    vqec_nat_fprint_all_bindings();    
    
    test_nat_natproto_destroy();
}


/**---------------------------------------------------------------------------
 * Unit-test methods array. 
 *---------------------------------------------------------------------------*/  
CU_TestInfo test_array_nat[] = {
    {"nat_stunproto_create", test_nat_stunproto_create},
    {"nat_stunproto_exhaust_ids", test_nat_stunproto_exhaust_ids},
    {"nat_stunproto_open_collide", test_nat_stunproto_open_collide},
    {"nat_stunproto_query_refresh", test_nat_stunproto_query_refresh},
    {"nat_stunproto_periodic_update", test_nat_stunproto_periodic_update},
    {"nat_stunproto_eject", test_nat_stunproto_eject},
    {"nat_stunproto_destroy", test_nat_stunproto_destroy},
    {"nat_stunproto_print", test_nat_stunproto_print},
    {"nat_natproto_create", test_nat_natproto_create},
    {"nat_natproto_exhaust_ids", test_nat_natproto_exhaust_ids},
    {"nat_natproto_open_collide", test_nat_natproto_open_collide},
    {"nat_natproto_query", test_nat_natproto_query},
    {"nat_natproto_print", test_nat_natproto_print},

    CU_TEST_INFO_NULL,
};

