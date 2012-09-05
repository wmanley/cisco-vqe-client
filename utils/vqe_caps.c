/*------------------------------------------------------------------
 * Linux Capability functions
 *
 * May 2008, Eric Friedrich
 *
 * Copyright (c) 2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include "vqe_caps.h"

/*
 * See documentation in vqe_caps.h
 */
vqe_cap_ret_t vqe_keep_cap (uid_t new_uid, 
                            gid_t new_gid, 
                            const char *cap_string)
{
    boolean retval = FALSE;
    cap_t caps; 

    /* Create cap object from text string, be sure to keep CAP_SETUID */
    caps = cap_from_text(cap_string);
    if (!caps) {
        retval = VQE_CAP_TEXTCAP;
        goto bail;
    }

    /* Set option to keep capabilities through a UID/GID change */
    if (prctl(PR_SET_KEEPCAPS, 1) != 0) {
        retval = VQE_CAP_PRCTL;
        goto bail;
    }

    /* Drop capabilities to a lower user/group */
    if (setgid(new_gid) != 0) {
        retval = VQE_CAP_SETGID;
        goto bail;
    }

    if (setuid(new_uid) != 0) {
        retval = VQE_CAP_SETUID;
        goto bail;
    }

    /* Set capabilities to given set */
    if (cap_set_proc(caps) == -1) {
        retval = VQE_CAP_SETCAP;
        goto bail;
    }
    
bail:
    if (caps)
        cap_free(caps);

    return (retval);
}
