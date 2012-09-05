/*
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "vqes_fbt_addr.h"
#include "utils/vqe_caps.h"

#define SYSLOG_DEFINITION
#include <log/vqes_cp_syslog_def.h>
#include <log/vqes_fbt_addr_syslog_def.h>
#undef SYSLOG_DEFINITION


#define CHECK(exp)                                      \
    do {                                                \
        if (!exp) fprintf(stderr, "Failed: %m\n");      \
    } while(0)

int main (int argc, char ** argv)
{
    char *addr_str;
    vqes_fbt_status_t addr_status;
    struct in_addr addr;
    int ret;

    vqe_cap_ret_t cap_ret;

    /* Initialize the logging facility */
    syslog_facility_open(LOG_VQES_CP, LOG_CONS);
    syslog_facility_filter_set(LOG_VQES_CP, LOG_INFO);

    /* Drop privileges as soon as possible to avoid anything bad from
     * happening
     */
    cap_ret = vqe_keep_cap(499, 499, VQE_CAP_CAPNETADMIN_EIP);
    if (cap_ret != VQE_CAP_OK) {
        fprintf(stderr, "Capability drop failure: %s\n", 
                vqe_ret_tostr(cap_ret));
    }

    /* Init */
    printf("Doing vqes_nl_init()\n");
    ret = vqes_fbt_nl_init();
    CHECK(ret == 0);

    int scope;
    /* Add global scope RT addresses */
    addr_str = "5.4.3.2";
    scope = RT_SCOPE_UNIVERSE;
    printf("To add adress: %s, scope: %s\n", 
           addr_str, vqes_fbt_addr_scope_to_str(scope));
    addr.s_addr = inet_addr(addr_str);
    ret = vqes_fbt_addr_add(addr, scope);
    CHECK(ret == 0);

    addr_str = "5.4.3.3";
    scope = RT_SCOPE_UNIVERSE;
    printf("To add adress: %s, scope: %s\n", 
           addr_str, vqes_fbt_addr_scope_to_str(scope));
    addr.s_addr = inet_addr(addr_str);
    ret = vqes_fbt_addr_add(addr, scope);
    CHECK(ret == 0);

    addr_str = "5.4.3.4";
    scope = RT_SCOPE_UNIVERSE;
    printf("To add adress: %s, scope: %s\n", 
           addr_str, vqes_fbt_addr_scope_to_str(scope));
    addr.s_addr = inet_addr(addr_str);
    ret = vqes_fbt_addr_add(addr, scope);
    CHECK(ret == 0);

    /* Add host scope RT addresses */
    addr_str = "6.5.3.2";
    scope = RT_SCOPE_HOST;
    printf("To add adress: %s, scope: %s\n", 
           addr_str, vqes_fbt_addr_scope_to_str(scope));
    addr.s_addr = inet_addr(addr_str);
    ret = vqes_fbt_addr_add(addr, scope);
    CHECK(ret == 0);

    addr_str = "6.5.3.3";
    scope = RT_SCOPE_HOST;
    printf("To add adress: %s, scope: %s\n", 
           addr_str, vqes_fbt_addr_scope_to_str(scope));
    addr.s_addr = inet_addr(addr_str);
    ret = vqes_fbt_addr_add(addr, scope);
    CHECK(ret == 0);

    addr_str = "6.5.3.4";
    scope = RT_SCOPE_HOST;
    addr.s_addr = inet_addr(addr_str);
    printf("To add adress: %s, scope: %s\n", 
           addr_str, vqes_fbt_addr_scope_to_str(scope));
    ret = vqes_fbt_addr_add(addr, scope);
    CHECK(ret == 0);

    /* Delete */
    addr_str = "5.4.3.4";
    addr.s_addr = inet_addr(addr_str);
    printf("To delete adress: %s\n", addr_str);
    ret = vqes_fbt_addr_del(addr);
    CHECK(ret == 0);

    addr_str = "6.5.3.4";
    addr.s_addr = inet_addr(addr_str);
    printf("To delete adress: %s\n", addr_str);
    ret = vqes_fbt_addr_del(addr);
    CHECK(ret == 0);
    printf("Deleted adress: %s\n", addr_str);

    /* Get address status */
    addr_str = "5.5.3.4";
    addr.s_addr = inet_addr(addr_str);
    printf("To get status for %s\n", addr_str);
    ret = vqes_fbt_addr_get_status(addr, &addr_status);
    CHECK(ret == 0);
    printf("get_status(): addr_status for %s = %s\n",
           addr_str,  vqes_fbt_addr_status_to_str(addr_status));

    addr_str = "6.5.3.3";
    addr.s_addr = inet_addr(addr_str);
    printf("To get status for %s\n", addr_str);
    ret = vqes_fbt_addr_get_status(addr, &addr_status);
    CHECK(ret == 0);
    printf("get_status(): addr_status for %s = %s\n",
           addr_str, vqes_fbt_addr_status_to_str(addr_status));

    printf("Sleeping for 3 seconds...\n");
    sleep(3);

    /* Flush all with netlink */
    printf("To flushed all FBT addresses with netlink\n");
    ret = vqes_fbt_addr_flush_all();
    CHECK(ret == 0);

    /* Deinit */
    printf("Doing vqes_nl_deinit()\n");
    ret = vqes_fbt_nl_deinit();
    CHECK(ret == 0);

    return 0;
}
