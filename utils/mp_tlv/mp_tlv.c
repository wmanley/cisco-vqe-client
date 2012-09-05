/*------------------------------------------------------------------
* Mpeg Parser.  
* Definitions for TLV Data
* May 2006, Jim Sullivan
*
* Copyright (c) 2006-2009 by cisco Systems, Inc.
* All rights reserved.
*------------------------------------------------------------------
*/

#include <mp_tlv.h>
#include <utils/mp_mpeg.h>

/**
 * mp_set_cc_for_pid -
 *
 * This function is called by the VQE-S when building a TSRAP. 
 *
 * It Adds an entry to a 'pidlist' (an internal representation of a TSRAP 
 * PID-LIST TLV). Given a pointer to a pidlist, the newly supplied PID 
 * is added, along with the supplied Continuity Counter value.
 *
 * If the supplied PID is already present in the provided pidlist,
 * it will not be added, but the CC field of the existing entry will be
 * updated.
 *
 * If there is no more room in the pidlist for the new PID to be added,
 * an error is returned.  Otherwise, the supplied PID has been added to
 * (or updated in) the supplied pidlist.
 * 
 * 
 * @params pidlist_ptr - pointer to pidlist 
 * @params pid         - pid value to add to pidlist
 * @params cc          - Continuity Counter value to add to pidlist,
 *                       or used to update pidlist
 *
 * @returns status
 *    - TRUE, if successful.  FALSE otherwise.
 *
 *      When adding a PID, FALSE is returned if the PID cannot be added
 *      (no room in pidlist).
 */
boolean mp_set_cc_for_pid (mp_tlv_pidlist_t* pidlist_ptr,
                           uint16_t pid, uint8_t cc)
{
    int i; 
    boolean found = FALSE;

    for (i=0; i<pidlist_ptr->cnt; i++) {

        if (pid == pidlist_ptr->dat[i].pid) {
            found = TRUE;
            break; 
        }
    }

    if (found == TRUE) {
        /* found it in pidlist - update cc (overwrite prev value) */
        pidlist_ptr->dat[i].cc = cc; 
        return (TRUE); 
    }

    /* didn't find PID in pidlist */
    if (pidlist_ptr->cnt < MP_MAX_TSRAP_PIDS) {
        /* but there's room to add it */
        pidlist_ptr->dat[pidlist_ptr->cnt].pid = pid;
        pidlist_ptr->dat[pidlist_ptr->cnt].cc = cc; 
        pidlist_ptr->cnt += 1;
        return (TRUE);
    } else {
        /* and there's no room to add it */
        return (FALSE);
    }
}


/**
 * mp_get_cc_for_pid -
 *
 * This function is called by the VQE-C when decoding a TSRAP.
 *
 * It 
 *  o locates the pidlist entry for the supplied pid
 *  o returns the continuity counter of this entry
 *  o depending on value of decrement_flag, decrements the continuity
 *     counter of this entry
 * 
 * Note: MPEG2-TS Spec says CC is not changed when TS packet has only an
 *   adaptation_field and no payload
 * 
 * @params pidlist_ptr - pointer to pidlist 
 * @params pid         - pid value whose cc is to be returned (and decremented)
 * @params cc_ptr      - Continuity Counter value of entry (before decrement)
 * @params decrement_flag - TRUE if CC is to be decremented, FALSE if CC is to 
 *                            be left untouched
 * @returns status
 *
 *    - TRUE  if the entry is found and updated.
 *    - FALSE if the entry is not found.
 *
 */
boolean mp_get_cc_for_pid (mp_tlv_pidlist_t* pidlist_ptr,
                           uint16_t pid, uint8_t *cc_ptr,
                           boolean decrement_flag)
{
    int i; 
    boolean found = FALSE;

    for (i=0; i<pidlist_ptr->cnt; i++) {

        if (pid == pidlist_ptr->dat[i].pid) {
            found = TRUE;
            break; 
        }
    }

    if (found == FALSE) {
        return (FALSE);
    }

    *cc_ptr = pidlist_ptr->dat[i].cc;

    if (decrement_flag) {
        pidlist_ptr->dat[i].cc = MP_GET_PREV_MPEGCC(*cc_ptr);
    }

    return (TRUE);
}
