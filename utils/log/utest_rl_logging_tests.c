/* Rate Limited Logging Unit Test
 *
 * February 2008, Donghai Ma
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "add-ons/include/CUnit/Basic.h"
#include "log/vqes_syslog_limit.h"
#include "utils/vam_time.h"

#include "log/vqes_cp_syslog_def.h"
#include "msgen/msgen_syslog_def.h"

/* sa_ignore DISABLE_RETURNS CU_assertImplementation*/


/* Helper function to check if two abs_time_t are close, within the range of
 * the passed-in micro seconds.
 */
static boolean time_close (abs_time_t t1, abs_time_t t2, uint32_t microsec)
{
    rel_time_t diff = TIME_SUB_A_A(t2, t1);
    if (TIME_CMP_R(gt, diff, TIME_MK_R(usec, microsec)))
        return FALSE;
    else
        return TRUE;
}


/* Areas that need test:
 *   0. msgdef checking
 *   1. rate limiting at different rates
 *   2. message suppression log
 *   3. test locking
 *
 *   Should cover the following API routines:
 *          extern void add_to_sml_ul(syslog_msg_def_t *p_msg);
 *          extern void remove_from_sml_ul(syslog_msg_def_t *p_msg);
 *          extern void check_sml_ul(abs_time_t ts_now);
 *          extern void check_sml(abs_time_t ts_now);
 *          extern syslog_msg_def_t *get_first_sup_msg(void);
 *          extern syslog_msg_def_t *get_next_sup_msg(syslog_msg_def_t *msg);
 */

void
test_vqes_rl_logging_msgdef_check (void)
{
    /* Check the internal msg_limiter structs are initialized correctly at
     * init time, for pre-defined MSGEN messages
     */

    /* Messages defined by syslog_msgdef() macro should have NULL msg_limiter 
     * struct
     */
    /* MSGEN_INFO_MSG_1 */
    CU_ASSERT_STRING_EQUAL(syslog_msg_MSGEN_INFO_MSG_1.msg_name,
                           "MSGEN_INFO_MSG_1");
    CU_ASSERT_PTR_NULL(syslog_msg_MSGEN_INFO_MSG_1.limiter);
    CU_ASSERT_EQUAL(syslog_msg_MSGEN_INFO_MSG_1.on_sup_list, FALSE);

    
    /* Messages defined by syslog_msgdef_limit() macro should have valid
     * msg_limiter structs
     */
    /*  MSGEN_INFO_MSG_RL_2: MSGDEF_LIMIT_FAST */
    CU_ASSERT_STRING_EQUAL(syslog_msg_MSGEN_INFO_MSG_RL_2.msg_name,
                           "MSGEN_INFO_MSG_RL_2");
    CU_ASSERT_EQUAL(syslog_msg_MSGEN_INFO_MSG_RL_2.on_sup_list, FALSE);
    CU_ASSERT_PTR_NOT_NULL(syslog_msg_MSGEN_INFO_MSG_RL_2.limiter);
    CU_ASSERT_EQUAL(syslog_msg_MSGEN_INFO_MSG_RL_2.limiter->rate_limit, 
                    MSGDEF_LIMIT_FAST);
    CU_ASSERT(TIME_CMP_A(eq, 
                         syslog_msg_MSGEN_INFO_MSG_RL_2.limiter->last_time, 
                         ABS_TIME_0));
    CU_ASSERT_EQUAL(syslog_msg_MSGEN_INFO_MSG_RL_2.limiter->suppressed, 0);


    /* MSGEN_ERR_MSG_RL_2:  MSGDEF_LIMIT_MEDIUM */
    CU_ASSERT_STRING_EQUAL(syslog_msg_MSGEN_ERR_MSG_RL_2.msg_name,
                           "MSGEN_ERR_MSG_RL_2");
    CU_ASSERT_EQUAL(syslog_msg_MSGEN_ERR_MSG_RL_2.on_sup_list, FALSE);
    CU_ASSERT_PTR_NOT_NULL(syslog_msg_MSGEN_ERR_MSG_RL_2.limiter);
    CU_ASSERT_EQUAL(syslog_msg_MSGEN_ERR_MSG_RL_2.limiter->rate_limit, 
                    MSGDEF_LIMIT_MEDIUM);
    CU_ASSERT(TIME_CMP_A(eq, 
                         syslog_msg_MSGEN_ERR_MSG_RL_2.limiter->last_time, 
                         ABS_TIME_0));
    CU_ASSERT_EQUAL(syslog_msg_MSGEN_ERR_MSG_RL_2.limiter->suppressed, 0);

    /* */
    vqes_syslog(LOG_WARNING, "Test message");
}    


void
test_vqes_rl_logging_check (void)
{
#define TS_BUF_SIZE 64
    int i;
    char ts_buff[TS_BUF_SIZE], ts_buff2[TS_BUF_SIZE];
    abs_time_t now = get_sys_time();
    uint32_t msg_rl;
    syslog_msg_def_t *p_msg = NULL;

    /* Test #1
     *
     * Send a rate limited msg back-to-back. The second one will be suppressed.
     * Check msg's limiter struct for the supressed counter and last_time 
     * timestamp.
     */
    syslog_print(MSGEN_ERR_MSG_RL_2, "Test#1: blah blah");
    CU_ASSERT(time_close(syslog_msg_MSGEN_ERR_MSG_RL_2.limiter->last_time,
                         now, 10));
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_2.on_sup_list == FALSE);
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_2.limiter->suppressed == 0);

    printf("Compare now=%s, limiter->last_time=%s\n",
           abs_time_to_str(now, ts_buff, TS_BUF_SIZE),
           abs_time_to_str(syslog_msg_MSGEN_ERR_MSG_RL_2.limiter->last_time,
                           ts_buff2, TS_BUF_SIZE));
    
    syslog_print(MSGEN_ERR_MSG_RL_2, "Test#1: blah blah again");
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_2.on_sup_list == TRUE);
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_2.limiter->suppressed == 1);
    
    /* Fast backward msg's limiter->last_time to (now+MSGDEF_LIMIT_xxx),
     * then send the same msg again. Now the suppressed message should be 
     * removed off the SML list; and the suppressed counter should be reset 
     * to 0.
     * 
     * Note at the end of all these, the limiter->last_time will be set to 
     * the actual current timestamp.
     */
    now = get_sys_time();

    printf("Compare now=%s, limiter->last_time=%s\n",
           abs_time_to_str(now, ts_buff, TS_BUF_SIZE),
           abs_time_to_str(syslog_msg_MSGEN_ERR_MSG_RL_2.limiter->last_time,
                           ts_buff2, TS_BUF_SIZE));

    msg_rl = syslog_msg_MSGEN_ERR_MSG_RL_2.limiter->rate_limit;
    syslog_msg_MSGEN_ERR_MSG_RL_2.limiter->last_time = 
        TIME_SUB_A_R(now, TIME_MK_R(msec, msg_rl + 1));
    syslog_print(MSGEN_ERR_MSG_RL_2, "Test#1: blah blah blah ");
    
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_2.on_sup_list == FALSE);
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_2.limiter->suppressed == 0);

    /* last_time timestamp is reset to real wall time */
    CU_ASSERT(time_close(syslog_msg_MSGEN_ERR_MSG_RL_2.limiter->last_time,
                         now, 10));

    
    /* Test #2
     *
     * Repeat the first part of Test #1. This time we have to wait for 
     * syslog_msg_MSGEN_ERR_MSG_RL_1.limiter->rate_limit (can't trick 
     * check_sml_ul() when it is called from the syslog_print() macro), 
     * before we send a _different_ message to trigger the removal of 
     * the suppressed msg from the SML.
     */
    now = get_sys_time();
    syslog_print(MSGEN_ERR_MSG_RL_1, "Test#2: blah blah");
    CU_ASSERT(time_close(syslog_msg_MSGEN_ERR_MSG_RL_1.limiter->last_time,
                         now, 10));
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_1.on_sup_list == FALSE);
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_1.limiter->suppressed == 0);

    syslog_print(MSGEN_ERR_MSG_RL_1, "Test#2: blah blah again");
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_1.on_sup_list == TRUE);
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_1.limiter->suppressed == 1);

    /* Send a different msg to trigger SML cleanup */
    msg_rl = syslog_msg_MSGEN_ERR_MSG_RL_1.limiter->rate_limit;
    usleep(msg_rl*1000);
    syslog_print(MSGEN_ERR_MSG_RL_2, "Test#2: blah blah blah ");
    
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_1.on_sup_list == FALSE);
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_1.limiter->suppressed == 0);

    /* last_time timestamp is reset to real wall time */
    CU_ASSERT(time_close(syslog_msg_MSGEN_ERR_MSG_RL_1.limiter->last_time,
                         now, 10));


    /* Test #3 
     *
     * Repeat the first part of Test #1. This time we explicitly do check_sml()
     * to remove the suppressed msg off the SML list, after fast-forwarding
     * the limiter->last_time. Also do a check_sml() before 
     */
    syslog_print(MSGEN_INFO_MSG_RL_2, "Test#3: blah blah");
    CU_ASSERT(time_close(syslog_msg_MSGEN_INFO_MSG_RL_2.limiter->last_time,
                         now, 10));
    CU_ASSERT(syslog_msg_MSGEN_INFO_MSG_RL_2.on_sup_list == FALSE);
    CU_ASSERT(syslog_msg_MSGEN_INFO_MSG_RL_2.limiter->suppressed == 0);

    syslog_print(MSGEN_INFO_MSG_RL_2, "Test#3: blah blah again");
    CU_ASSERT(syslog_msg_MSGEN_INFO_MSG_RL_2.on_sup_list == TRUE);
    CU_ASSERT(syslog_msg_MSGEN_INFO_MSG_RL_2.limiter->suppressed == 1);

    /* Suppress a different msg */
    syslog_print(MSGEN_ERR_MSG_RL_1, "Test#3: blah blah");
    CU_ASSERT(time_close(syslog_msg_MSGEN_ERR_MSG_RL_1.limiter->last_time,
                         now, 10));
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_1.on_sup_list == FALSE);
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_1.limiter->suppressed == 0);

    syslog_print(MSGEN_ERR_MSG_RL_1, "Test#3: blah blah again");
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_1.on_sup_list == TRUE);
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_1.limiter->suppressed == 1);

    /* Check the SML: there should be two suppressed messages on the list */
    i = 0;
    for (p_msg = get_first_sup_msg(); 
         p_msg;
         p_msg = get_next_sup_msg(p_msg)) {
        i++;
    }
    CU_ASSERT_EQUAL(i, 2);
    CU_ASSERT_EQUAL(sml_num_entries(), 2);

    /* Fast forward */
    msg_rl = MSGDEF_LIMIT_MEDIUM;
    now = TIME_ADD_A_R(get_sys_time(), TIME_MK_R(msec, msg_rl + 1));

    /* check_sml() to trigger cleanup SML list */
    check_sml(now);
    
    CU_ASSERT(syslog_msg_MSGEN_INFO_MSG_RL_2.on_sup_list == FALSE);
    CU_ASSERT(syslog_msg_MSGEN_INFO_MSG_RL_2.limiter->suppressed == 0);

    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_1.on_sup_list == FALSE);
    CU_ASSERT(syslog_msg_MSGEN_ERR_MSG_RL_1.limiter->suppressed == 0);

    /* Check the SML: now there should be no suppressed message on the list */
    i = 0;
    for (p_msg = get_first_sup_msg(); 
         p_msg;
         p_msg = get_next_sup_msg(p_msg)) {
        i++;
    }
    CU_ASSERT_EQUAL(i, 0);
    CU_ASSERT_EQUAL(sml_num_entries(), 0);

    /* Test #4
     * 
     * Send one message. Fast forward last_time to (now+MSGDEF_LIMIT_XXXX);
     * send the message again. This time the message should not be suppressed.
     */
    now = get_sys_time();
    syslog_print(MSGEN_INFO_MSG_RL_1, "Test#4: blah blah");
    CU_ASSERT(time_close(syslog_msg_MSGEN_INFO_MSG_RL_1.limiter->last_time,
                         now, 10));
    CU_ASSERT(syslog_msg_MSGEN_INFO_MSG_RL_1.on_sup_list == FALSE);
    CU_ASSERT(syslog_msg_MSGEN_INFO_MSG_RL_1.limiter->suppressed == 0);

    /* Fast backward the msg last check timestamp */
    msg_rl = syslog_msg_MSGEN_INFO_MSG_RL_1.limiter->rate_limit;
    syslog_msg_MSGEN_INFO_MSG_RL_1.limiter->last_time = 
        TIME_SUB_A_R(now, TIME_MK_R(msec, msg_rl + 1));

    /* */
    syslog_print(MSGEN_INFO_MSG_RL_1, "Test#4: blah blah again");
    CU_ASSERT(syslog_msg_MSGEN_INFO_MSG_RL_1.on_sup_list == FALSE);
    CU_ASSERT(syslog_msg_MSGEN_INFO_MSG_RL_1.limiter->suppressed == 0);


    /* Test #5
     * 
     * Send two _different_ rate limited messages back-to-back. Make sure the 
     * second message is not suppressed, i.e. rate limiting is on a per-message
     * basis.
     */
    syslog_print(MSGEN_WARN_MSG_RL_1, "Test#5: blah blah");
    CU_ASSERT(syslog_msg_MSGEN_WARN_MSG_RL_1.on_sup_list == FALSE);
    CU_ASSERT(syslog_msg_MSGEN_WARN_MSG_RL_1.limiter->suppressed == 0);

    syslog_print(MSGEN_WARN_MSG_RL_2, "Test#5: blah blah again");
    CU_ASSERT(syslog_msg_MSGEN_WARN_MSG_RL_2.on_sup_list == FALSE);
    CU_ASSERT(syslog_msg_MSGEN_WARN_MSG_RL_2.limiter->suppressed == 0);
    

    /* Test #6 (regression test)
     * 
     * No rate limiting on a non-rate-limited message, i.e. messages defined 
     * with msgdef() macro.
     */
    syslog_print(MSGEN_INFO_MSG_2, "Test#6: blah blah ");
    CU_ASSERT(syslog_msg_MSGEN_INFO_MSG_2.on_sup_list == FALSE);    
    syslog_print(MSGEN_INFO_MSG_2, "Test#6: blah blah again ");
    CU_ASSERT(syslog_msg_MSGEN_INFO_MSG_2.on_sup_list == FALSE);

}

/* Check adding messages to and checking the SML */
void
test_vqes_rl_sml_check (void)
{
    int i = 0;
    uint32_t time_btw_msgs_ms = 10;
    syslog_msg_def_t *p_rl_msg = NULL, *p_msg = NULL;
    char ts_buff[TS_BUF_SIZE];
    abs_time_t now = get_sys_time();

    /* How the test is performed:
     * 1. Add two slow msgs to the SML, with the second slow msg's last logging
     *    time set to (time_btw_msg_ms) milliseconds after the first one's;
     * 2. Add four medium msgs to the SML, with each subsequent msg's last 
     *    logging time set to (time_btw_msg_ms) milliseconds after the previous
     *    msg;
     * 3. Remove two medium msgs off of the SML: test the remove routine;
     * 4. Add two fast msgs, to the SML, with the second one's last logging
     *    time set to (time_btw_msg_ms) milliseconds after the first one's;
     * 5. As the SML is ascend sorted by the msg removal time,
     *       removal_time = last_log_time + msg_time_limit
     *    in the end the messages on the SML should look like this:
     *       Head -> fast#1 -> fast#2 -> medium#1 -> medium#2 -> 
     *              slow#1 -> slow#2 -> Tail
     * 6. Now fast forward clock and call check_sml_ul() to remove one message
     *    at a time off of the SML, in the following order:
     *                 set clock to                             move msg off
     *      ------------------------------------------------   --------------
     *       now() + MSGDEF_LIMIT_FAST                            fast#1
     *       now() + MSGDEF_LIMIT_FAST + time_btw_msgs_ms         fast#2
     *       now() + MSGDEF_LIMIT_MEDIUM                          medium#1
     *       now() + MSGDEF_LIMIT_MEDIUM + time_btw_msgs_ms       medium#2
     *       now() + MSGDEF_LIMIT_SLOW                            slow#1
     *       now() + MSGDEF_LIMIT_SLOW + time_btw_msgs_ms         slow#2
     */
    
    /* Slow msgs */
    /* slow#1: MSGEN_ERR_SLOW_MSG */
    p_rl_msg = &syslog_msg_MSGEN_ERR_SLOW_MSG;
    p_rl_msg->limiter->last_time = now;
    add_to_sml_ul(p_rl_msg);
    i ++;
    printf("Added slow#1-%s, last log time=%s, to remove in %d (ms), "
           "SML num_entries=%d; expect=%d\n", p_rl_msg->msg_name,
           abs_time_to_str(p_rl_msg->limiter->last_time, 
                           ts_buff, TS_BUF_SIZE),
           p_rl_msg->limiter->rate_limit, sml_num_entries(), i);
    CU_ASSERT_EQUAL(sml_num_entries(), i);

    /* slow#2: MSGEN_WARN_SLOW_MSG */
    p_rl_msg = &syslog_msg_MSGEN_WARN_SLOW_MSG;
    p_rl_msg->limiter->last_time = 
        TIME_ADD_A_R(now, TIME_MK_R(msec, time_btw_msgs_ms));
    add_to_sml_ul(p_rl_msg);
    i ++;
    printf("Added slow#2-%s, last log time=%s, to remove in %d (ms), "
           "SML num_entries=%d; expect=%d\n", p_rl_msg->msg_name,
           abs_time_to_str(p_rl_msg->limiter->last_time, 
                           ts_buff, TS_BUF_SIZE),
           p_rl_msg->limiter->rate_limit, sml_num_entries(), i);
    CU_ASSERT_EQUAL(sml_num_entries(), i);

    /* Medium msgs */
    /* medium#1: MSGEN_WARN_MSG_RL_1 */
    p_rl_msg = &syslog_msg_MSGEN_WARN_MSG_RL_1;
    p_rl_msg->limiter->last_time = now;
    add_to_sml_ul(p_rl_msg);
    i++;
    printf("Added medium#1-%s, last log time=%s, to remove in %d (ms), "
           "SML num_entries=%d; expect=%d\n", p_rl_msg->msg_name,
           abs_time_to_str(p_rl_msg->limiter->last_time, 
                           ts_buff, TS_BUF_SIZE),
           p_rl_msg->limiter->rate_limit, sml_num_entries(), i);
    CU_ASSERT_EQUAL(sml_num_entries(), i);

    /* medium#2: MSGEN_ERR_MSG_RL_1 */
    p_rl_msg = &syslog_msg_MSGEN_ERR_MSG_RL_1;
    p_rl_msg->limiter->last_time = 
        TIME_ADD_A_R(now, TIME_MK_R(msec, time_btw_msgs_ms));
    add_to_sml_ul(p_rl_msg);
    i ++;
    printf("Added medium#2-%s, last log time=%s, to remove in %d (ms), "
           "SML num_entries=%d; expect=%d\n", p_rl_msg->msg_name,
           abs_time_to_str(p_rl_msg->limiter->last_time, 
                           ts_buff, TS_BUF_SIZE),
           p_rl_msg->limiter->rate_limit, sml_num_entries(), i);
    CU_ASSERT_EQUAL(sml_num_entries(), i);

    /* medium#3: MSGEN_WARN_MSG_RL_2 */
    p_rl_msg = &syslog_msg_MSGEN_WARN_MSG_RL_2;
    p_rl_msg->limiter->last_time = 
        TIME_ADD_A_R(now, TIME_MK_R(msec, time_btw_msgs_ms*2));
    add_to_sml_ul(p_rl_msg);
    i ++;
    printf("Added medium#3-%s, last log time=%s, to remove in %d (ms), "
           "SML num_entries=%d; expect=%d\n", p_rl_msg->msg_name,
           abs_time_to_str(p_rl_msg->limiter->last_time, 
                           ts_buff, TS_BUF_SIZE),
           p_rl_msg->limiter->rate_limit, sml_num_entries(), i);
    CU_ASSERT_EQUAL(sml_num_entries(), i);

    /* remove medium#3: MSGEN_WARN_MSG_RL_2 */
    remove_from_sml_ul(p_rl_msg);
    i --;
    printf("Removed medium#3-%s from SML, SML num_entries=%d; expect=%d\n",
            p_rl_msg->msg_name, sml_num_entries(), i);
    CU_ASSERT_EQUAL(sml_num_entries(), i);
    
    /* medium#4: MSGEN_ERR_MSG_RL_2 */
    p_rl_msg = &syslog_msg_MSGEN_ERR_MSG_RL_2;
    p_rl_msg->limiter->last_time = 
        TIME_ADD_A_R(now, TIME_MK_R(msec, time_btw_msgs_ms*3));
    add_to_sml_ul(p_rl_msg);
    i ++;
    printf("Added medium#4-%s, last log time=%s, to remove in %d (ms), "
           "SML num_entries=%d; expect=%d\n", p_rl_msg->msg_name,
           abs_time_to_str(p_rl_msg->limiter->last_time, 
                           ts_buff, TS_BUF_SIZE),
           p_rl_msg->limiter->rate_limit, sml_num_entries(), i);
    CU_ASSERT_EQUAL(sml_num_entries(), i);

    /* remove medium#4: MSGEN_ERR_MSG_RL_2 */
    remove_from_sml_ul(p_rl_msg);
    i --;
    printf("Removed medium#4-%s from SML, SML num_entries=%d; expect=%d\n",
            p_rl_msg->msg_name, sml_num_entries(), i);
    CU_ASSERT_EQUAL(sml_num_entries(), i);

    /* Fast msgs */
    /* fast#1: MSGEN_INFO_MSG_RL_1 */
    p_rl_msg = &syslog_msg_MSGEN_INFO_MSG_RL_1;
    p_rl_msg->limiter->last_time = now;
    add_to_sml_ul(p_rl_msg);
    i ++;
    printf("Added fast#1-%s, last log time=%s, to remove in %d (ms), "
           "SML num_entries=%d; expect=%d\n", p_rl_msg->msg_name,
           abs_time_to_str(p_rl_msg->limiter->last_time, 
                           ts_buff, TS_BUF_SIZE),
           p_rl_msg->limiter->rate_limit, sml_num_entries(), i);
    CU_ASSERT_EQUAL(sml_num_entries(), i);

    /* fast#2: MSGEN_INFO_MSG_RL_2 */
    p_rl_msg = &syslog_msg_MSGEN_INFO_MSG_RL_2;
    p_rl_msg->limiter->last_time = 
        TIME_ADD_A_R(now, TIME_MK_R(msec, time_btw_msgs_ms));
    add_to_sml_ul(p_rl_msg);
    i ++;
    printf("Added fast#2-%s, last log time=%s, to remove in %d (ms), "
           "SML num_entries=%d; expect=%d\n", p_rl_msg->msg_name,
           abs_time_to_str(p_rl_msg->limiter->last_time, 
                           ts_buff, TS_BUF_SIZE),
           p_rl_msg->limiter->rate_limit, sml_num_entries(), i);
    CU_ASSERT_EQUAL(sml_num_entries(), i);
    
    /* Show the messages on SML in order */
    printf("\nMessages on SML:\n");
    for (p_msg = get_first_sup_msg(); 
         p_msg;
         p_msg = get_next_sup_msg(p_msg)) {
       printf("  msg=%s, last log time=%s, to remove in %d (ms)\n",
              p_msg->msg_name,
              abs_time_to_str(p_msg->limiter->last_time, ts_buff, TS_BUF_SIZE),
              p_msg->limiter->rate_limit);
   }

    /* Fast forward clock to remove msgs from the SML */
    now = get_sys_time();

    /*               set clock to                            remove msg
     *  ------------------------------------------------   --------------
     *    now() + MSGDEF_LIMIT_FAST                            fast#1
     *    now() + MSGDEF_LIMIT_FAST + time_btw_msgs_ms         fast#2
     *    now() + MSGDEF_LIMIT_MEDIUM                          medium#1
     *    now() + MSGDEF_LIMIT_MEDIUM + time_btw_msgs_ms       medium#2
     *    now() + MSGDEF_LIMIT_SLOW                            slow#1
     *    now() + MSGDEF_LIMIT_SLOW + time_btw_msgs_ms         slow#2
     */
    abs_time_t removal_time_a = 
        TIME_ADD_A_R(now, TIME_MK_R(msec, MSGDEF_LIMIT_MEDIUM));
    check_sml_ul(removal_time_a);

    /* The first three messages: fast#1, fast#2 and medium#1 should have 
     * been removed by now.
     */
    CU_ASSERT_EQUAL(sml_num_entries(), 3);
    
    printf("\nShould have removed the first three messages from SML. "
           "Leftover messages:\n");
    for (p_msg = get_first_sup_msg(); 
         p_msg;
         p_msg = get_next_sup_msg(p_msg)) {
       printf("  msg=%s, last log time=%s, to remove in %d (ms)\n",
              p_msg->msg_name,
              abs_time_to_str(p_msg->limiter->last_time, ts_buff, TS_BUF_SIZE),
              p_msg->limiter->rate_limit);
    }

    removal_time_a = 
        TIME_ADD_A_R(now, 
                     TIME_MK_R(msec, MSGDEF_LIMIT_MEDIUM + time_btw_msgs_ms));
    check_sml_ul(removal_time_a);
    CU_ASSERT_EQUAL(sml_num_entries(), 2);

    removal_time_a = 
        TIME_ADD_A_R(now, TIME_MK_R(msec, MSGDEF_LIMIT_SLOW-10));
    check_sml_ul(removal_time_a);
    CU_ASSERT_EQUAL(sml_num_entries(), 2);

    removal_time_a = 
        TIME_ADD_A_R(now, TIME_MK_R(msec, MSGDEF_LIMIT_SLOW));
    check_sml_ul(removal_time_a);
    CU_ASSERT_EQUAL(sml_num_entries(), 1);


    removal_time_a = 
        TIME_ADD_A_R(now, 
                     TIME_MK_R(msec, MSGDEF_LIMIT_SLOW + time_btw_msgs_ms));
    check_sml_ul(removal_time_a);
    CU_ASSERT_EQUAL(sml_num_entries(), 0);
    
}


CU_TestInfo test_vqes_rl_logging_array[] = {
    { "Rate Limited Logging: msgdef checking\n", 
      test_vqes_rl_logging_msgdef_check },
    { "Rate Limited Logging: rate limit checking\n",
      test_vqes_rl_logging_check },
    { "Rate Limited Logging: Suppressed Message List checking\n",
      test_vqes_rl_sml_check },

    CU_TEST_INFO_NULL,
};

int
test_vqes_rl_logging_suite_init (void)
{
    /* Initialize the logging facility */
    (void)unsetenv("USE_STDERR");
    syslog_facility_open(LOG_VQES_CP, LOG_CONS);
    syslog_facility_filter_set(LOG_VQES_CP, LOG_INFO);
    
    return 0;
}

int
test_vqes_rl_logging_suite_clean (void)
{
    vqes_closelog();
    return 0;
}

CU_SuiteInfo suites_vqes_rl_logging[] = {
    { "VQE_S Rate Limited Logging suite",
      test_vqes_rl_logging_suite_init,
      test_vqes_rl_logging_suite_clean,
      test_vqes_rl_logging_array },
    CU_SUITE_INFO_NULL,
};
