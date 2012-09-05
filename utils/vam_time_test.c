/*------------------------------------------------------------------
 * VAM Time representation test program
 *
 * May 2006, Josh Gahm
 *
 * Copyright (c) 2006-2011 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */


#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "../include/utils/vam_time.h"

int main(int argc, const char * argv[])
{
    abs_time_t now = get_sys_time();
    char buff[256];
    printf("Running vam_time_test at %s\n", 
           abs_time_to_str(now, buff, 256));

    abs_time_t test_time = TIME_ADD_A_R(ABS_TIME_0, 
                                        TIME_ADD_R_R(SECS(5*24*60*60), USECS(999999)));
    abs_time_t other_test_time;
    
    ASSERT(TIME_CMP_R(eq,SEC,TIME_MULT_R_I(USEC,1000000)), "1");
    ASSERT(TIME_CMP_R(eq,SEC,TIME_MULT_R_I(MSEC,1000)), "2");
    ASSERT(TIME_CMP_R(eq,SEC, USECS(1000000)), "3");
    ASSERT(TIME_CMP_R(eq,SEC, MSECS(1000)), "4");
    ASSERT(TIME_CMP_R(eq,TIME_SUB_A_A(now, now), REL_TIME_0), "5");

    other_test_time = TIME_MK_A(usec, ((uint64_t)5)*24*60*60*1000000 + 999999);

    abs_time_t other_test_time_2 = TIME_SUB_A_R(other_test_time, USECS(1<<31));

    ASSERT(TIME_CMP_A(eq, test_time, other_test_time), "6");
    ASSERT(TIME_CMP_A(gt, test_time, other_test_time_2), "7");
    ASSERT(TIME_CMP_A(lt, other_test_time_2, test_time), "8");
    ASSERT(TIME_CMP_A(ge, test_time, other_test_time_2), "9");
    ASSERT(TIME_CMP_A(le, other_test_time_2, test_time), "10");

    ASSERT(TIME_CMP_A(le, test_time, other_test_time), "11");
    ASSERT(TIME_CMP_A(ge, test_time, other_test_time), "12");   
    ASSERT(TIME_CMP_A(ne, other_test_time_2, test_time), "13");       

    rel_time_t r_test_time = TIME_SUB_A_A(test_time, ABS_TIME_0);
    rel_time_t r_other_test_time = TIME_SUB_A_A(other_test_time, ABS_TIME_0);
    rel_time_t r_other_test_time_2 = TIME_SUB_A_A(other_test_time_2, ABS_TIME_0);

    ASSERT(TIME_CMP_R(eq, r_test_time, r_other_test_time), "14");
    ASSERT(TIME_CMP_R(gt, r_test_time, r_other_test_time_2), "15");
    ASSERT(TIME_CMP_R(lt, r_other_test_time_2, r_test_time), "16");
    ASSERT(TIME_CMP_R(ge, r_test_time, r_other_test_time_2), "17");
    ASSERT(TIME_CMP_R(le, r_other_test_time_2, r_test_time), "18");

    ASSERT(TIME_CMP_R(le, r_test_time, r_other_test_time), "19");
    ASSERT(TIME_CMP_R(ge, r_test_time, r_other_test_time), "20");   
    ASSERT(TIME_CMP_R(ne, r_other_test_time_2, r_test_time), "21");       
       
    ASSERT(TIME_CMP_A(eq,
                      TIME_ADD_A_R(test_time, TIME_NEG_R(SEC)),
                      TIME_SUB_A_R(test_time, SEC)), "22");

    ASSERT(TIME_CMP_R(eq,
                      TIME_ADD_R_R(r_test_time, r_test_time),
                      TIME_MULT_R_I(r_test_time, 2)), "23");

    ASSERT(TIME_CMP_R(eq,
                      TIME_DIV_R_I(USECS(3*100000000), 3),
                      USECS(100000000)), "24");
    ASSERT(TIME_DIV_R_R(USECS(3*100000000),
                        USECS(100000000)) == 3,
           "25");

    ASSERT(TIME_CMP_R(eq,
                      TIME_MOD_R_R(USECS(3*100000000 + 57),
                                   USECS(100000000)),
                      USECS(57)), "26");

    ASSERT(TIME_CMP_R(eq,
                      TIME_LSHIFT_R(USECS(123), 8),
                      TIME_MULT_R_I(USECS(123),1<<8)),
           "27");

    ASSERT(TIME_CMP_R(eq,TIME_RSHIFT_R(USECS(123<<8), 8),
                      USECS(123)),
           "28");

    /*$$$ the following assumes we are on US ET */
    ASSERT(strcmp(abs_time_to_str(test_time, buff, 256),
                  "1970-01-05 19:00:00.999999") == 0,
           "29 %s", buff);

    ASSERT(strcmp(rel_time_to_str(USECS(5123456), buff, 256),
                  "5.123456") == 0,
           "30 %s", buff);

     ASSERT(strcmp(rel_time_to_str(USECS(-5123456), buff, 256),
                  "-5.123456") == 0,
           "30 %s", buff);

     ASSERT(TIME_CMP_R(gt,
                       USECS(3),
                       TIME_ABS_R(TIME_SUB_A_A(test_time, 
                                               TIME_MK_A(ntp,
                                                         TIME_GET_A(ntp, 
                                                                    test_time))))),
                       "31 %"PRId64"",
                       TIME_GET_R(usec, 
                                  TIME_SUB_A_A(test_time, 
                                             TIME_MK_A(ntp,
                                                       TIME_GET_A(ntp, 
                                                                  test_time)))));
     return 0;
}

    
           

    
                      
    
