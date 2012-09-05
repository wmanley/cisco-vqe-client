/********************************************************************
 * vqes_syslog_limit.c
 * 
 * Helper functions for VQE-S syslog rate limiting
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/
#if HAVE_SYSLOG    /* RL stuffs are here only when using syslog */

#include "utils/vam_types.h"
#include "log/vqes_syslog_limit.h"

/* Define PRINTF(args...) as printf(args) to print out debug messages */
#define PRINTF(args...)

/* Suppressed Message List (SML)
 * 
 * Maintain a TAILQ style list for the suppressed message types. This list is 
 * checked whenever a message is emitted to syslogd. For each message type on 
 * the list, if the removal_time (i.e. msg's last_time + msg's rate_limit)
 * of the message is in the past, we are going to write a warning level log 
 * to report the suppressed message instances; and then remove the message 
 * struct off the SML.
 *
 * The suppressed messages on the SML are sorted by the removal time: the more 
 * toward the list head the sooner the message is to be removed off the SML. 
 * The sorted SML is to make it efficient while checking the SML for any 
 * expired message structs.
 */
static sml_t sml;

/* Need to init the list if this flag is FALSE */
static boolean list_inited = FALSE;

/* Init the list */
static void init_sml_ul (void)
{
    VQE_TAILQ_INIT(&sml.head);
    list_inited = TRUE;
}

/* Function:    add_to_sml_ul
 * Description: Add a message type struct to the SML, starting from the tail
 *              of the SML and is ascent sorted by the removal time: 
 *              the more toward the head the sooner the struct is to be 
 *              removed off of the SML. Note check_sml() starts from the head.
 *
 *              This routine is unlocked, which means that the callers shall 
 *              call vqes_syslog_lock_lock() before calling this routine.
 * Parameters:  p_msg - the message that is currently being suppressed
 * Returns:     none - as we don't plan to do anything different even if we 
 *                     fail to add to the list
 * Side Effects: The p_chain struct inside p_msg is modified after p_msg is
 *               added onto the list
 */
void add_to_sml_ul (syslog_msg_def_t *p_msg)
{
    syslog_msg_def_t *entry = NULL;

    if (!list_inited) {
        init_sml_ul();
    }

    /* Add the message struct from the list tail and have it sorted by the 
     * removal time, which is (last_time + rate_limit). The smaller the 
     * removal time, the closer the message is to the head.
     */
    VQE_TAILQ_FOREACH_REVERSE(entry, &sml.head, sml_head_, list_obj) {
        /* Insert p_msg after an entry-on-the-list if:
         *     p_msg's (last_time+rate_limt) >= the entry's
         *
         * else continue with the message structs on the SML
         */
        if (TIME_CMP_A(ge, TIME_ADD_A_R(p_msg->limiter->last_time,
                                        MSECS(p_msg->limiter->rate_limit)),
                       TIME_ADD_A_R(entry->limiter->last_time,
                                    MSECS(entry->limiter->rate_limit)))) {
            
            break;
        }
    }
    if (entry) {
        VQE_TAILQ_INSERT_AFTER(&sml.head, entry, p_msg, list_obj);
    } else {

        /* At this point entry being NULL suggests that we have reached the 
         * head without having p_msg inserted. p_msg should be added to the 
         * head in this case.
         */
        VQE_TAILQ_INSERT_HEAD(&sml.head, p_msg, list_obj);
    }
    sml.num_entries++;

    p_msg->on_sup_list = TRUE;

    PRINTF("Added \"%s\" to sml\n", p_msg->msg_name);
}

/* Function:    remove_from_sml_ul
 * Description: Remove a message type struct from the head of the SML.
 *              This routine is unlocked, which means that the callers shall 
 *              call vqes_syslog_lock_lock() before calling this routine.
 * Parameters:  p_msg - the message that has just emitted a message instance
 * Returns:     none - as we don't plan to do anything different even if we 
 *                     fail to remove from the list
 * Side Effects: The p_chain struct inside p_msg is modified after p_msg is
 *               added onto the list
 */
void remove_from_sml_ul (syslog_msg_def_t *p_msg)
{
    syslog_msg_def_t *p_msg_on_list;

    if (!list_inited) {
        return;
    }

    /* Locate the message on the list and remove it */
    if (sml.num_entries > 0) {
        VQE_TAILQ_FOREACH(p_msg_on_list, &sml.head, list_obj) {
            if (p_msg_on_list == p_msg) {
                VQE_TAILQ_REMOVE(&sml.head, p_msg_on_list, list_obj);
                sml.num_entries --;

                p_msg->on_sup_list = FALSE;

                PRINTF("Removed \"%s\" from sml\n", 
                       p_msg_on_list->msg_name);
                break;
            } /* if this is the message we'd like to remove */
        } /* for each msg on the list */
    } /* if list not empty */
}

/* Function:    check_sml
 * Description: Check the Suppressed Message List for any staled suppressed 
 *              messages, i.e. limiter->last_time is at least limiter->limit 
 *              msec in the past. Remove them and print a suppression report 
 *              for each removed message.
 *          
 *              As the SML is ascend sorted by the removal time, we traverse 
 *              the list from head and process the messages until we find an 
 *              entry whose removal time is in the future.
 *                 
 *              An application may hook this routine to a timer to actively 
 *              maintain the SML.
 * Parameters:  now - current system time. The routine will get the current
 *              system time if the passed in value is ABS_TIME_0.
 * Returns:     none
 * Side Effects: The sml is updated after removing "staled" messages off the 
 *               list.
 */

/* Unlocked version: callers should do vqes_syslog_lock_lock() before calling
 * this routine.
 */
void check_sml_ul (abs_time_t now)
{
    syslog_msg_def_t *p_msg = NULL;
    abs_time_t now_ts = now;

    if (!list_inited) {
        return;
    }
    
    if (VQE_TAILQ_EMPTY(&sml.head)) {
        return;
    }

    if (TIME_CMP_A(eq, now_ts, ABS_TIME_0)) {
        now_ts = get_sys_time();
    }

    VQE_TAILQ_FOREACH(p_msg, &sml.head, list_obj) {
        /* p_msg is always not NULL inside the loop */
        msg_limiter *lt = p_msg->limiter;

        if (!lt) {
            /* Should not be here: all messages on the SML should have limiter 
             * setup already. 
             */
            continue;
        }

        if (TIME_CMP_A(gt, TIME_ADD_A_R(lt->last_time, MSECS(lt->rate_limit)),
                       now_ts)) {
            /* As the SML is ascend sorted with the removal time, we can break
             * as soon as we hit an entry whose removal time is in the future.
             */
            break;

        } else {
            /* The msg has stayed long enough on the SML: log the suppression
             * message and remove it off the SML.
             */
            if (lt->suppressed > 0) {
                vqes_syslog_ul(LOG_WARNING,
                               "Messages of the form \"<%s-%d-%s> %s\" were "
                               "suppressed %llu times\n",
                               syslog_convert_to_upper(p_msg->facility->fac_name),
                               p_msg->priority,
                               p_msg->msg_name,
                               p_msg->msg_format,
                               lt->suppressed);
                
                lt->suppressed = 0;
            } 

            VQE_TAILQ_REMOVE(&sml.head, p_msg, list_obj);
            sml.num_entries --;

            p_msg->on_sup_list = FALSE;

            PRINTF("While checking smlist: removed \"%s\" from sml\n", 
                   p_msg->msg_name);
        }
    } /* for each msg */
}

/* Locked routine to check the SML */
void check_sml (abs_time_t now)
{
    vqes_syslog_lock_lock();
    check_sml_ul(now);
    vqes_syslog_lock_unlock();
}


/* Function:    get_first_sup_msg
 * Description: Return the first suppresed message on the SML
 * Parameters:  none
 * Returns:     ptr to the first one
 * Side Effects: none
 */
syslog_msg_def_t *get_first_sup_msg (void)
{
    syslog_msg_def_t *ret_msg = NULL;

    vqes_syslog_lock_lock();

    if (!VQE_TAILQ_EMPTY(&sml.head)) {
        ret_msg = VQE_TAILQ_FIRST(&sml.head);
    }

    vqes_syslog_lock_unlock();

    return ret_msg;
}

/* Function:    get_next_sup_msg
 * Description: Return the next suppresed message on the SML after the one
 *              as specified in the input parameter.
 * Parameters:  a suppressed message
 * Returns:     ptr to the next one
 * Side Effects: none
 */
syslog_msg_def_t *get_next_sup_msg (syslog_msg_def_t *msg)
{
    syslog_msg_def_t *ret_msg = NULL;
    
    vqes_syslog_lock_lock();
    ret_msg = VQE_TAILQ_NEXT(msg, list_obj);
    vqes_syslog_lock_unlock();

    return ret_msg;
}

/* Function:    sml_num_entries
 * Description: Return the num_entries currently on the SML
 * Parameters:  none
 * Returns:     num_entries
 * Side Effects: none
 */
uint32_t sml_num_entries (void)
{
    uint32_t ne;
    vqes_syslog_lock_lock();
    ne = sml.num_entries;
    vqes_syslog_lock_unlock();

    return ne;
}

#endif /* #if HAVE_SYSLOG */
