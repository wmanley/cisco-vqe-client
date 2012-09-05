/*------------------------------------------------------------------
 * Linux Capability functions
 *
 * May 2008, Eric Friedrich
 *
 * Copyright (c) 2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */


#ifndef __VQE_CAPS_H__
#define __VQE_CAPS_H__

#include <sys/capability.h> /* Must compile w/ -lcap */
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>
#include "vam_types.h"

typedef enum vqe_cap_ret_ {
    VQE_CAP_OK,
    VQE_CAP_PRCTL,
    VQE_CAP_TEXTCAP,
    VQE_CAP_SETCAP,
    VQE_CAP_SETUID,
    VQE_CAP_SETGID,
    VQE_CAP_OTHER
}vqe_cap_ret_t;

/* Grants effective, inherited, and permitted CAP_NET_ADMIN capability */
#define VQE_CAP_CAPNETADMIN_EIP "cap_net_admin=eip"

/* Grants effective, inherited, and permitted CAP_NET_RAW capability */
#define VQE_CAP_CAPNETRAW_EIP "cap_net_raw=eip"

/**
 * Drop privileges to given user/group while setting the given capability.
 * The cap_string param should be formatted according to libcap's 
 * cap_set_proc rules.
 *
 * @param new_uid - UID to drop to
 * @param new_gid - Group ID to drop to
 * @param cap_string - Capabilities to add/drop
 * @return Reason for failure
 */
vqe_cap_ret_t vqe_keep_cap(uid_t new_uid, 
                           gid_t new_gid, 
                           const char *cap_string);

static inline char* vqe_ret_tostr (vqe_cap_ret_t retval)
{
    switch (retval) {
    case VQE_CAP_OK:
        return "Capability Set OK";
    case VQE_CAP_PRCTL:
        return "prctl() failed";
    case VQE_CAP_TEXTCAP:
        return "Bad Capability String";
    case VQE_CAP_SETCAP:
        return "Setting Capability Failed";
    case VQE_CAP_SETUID:
        return "Changing User ID Failed";
    case VQE_CAP_SETGID:
        return "Changing Group ID Failed";
    case VQE_CAP_OTHER:
        return "Unknown Failure";
    default:
        return "Invalid Error Code";
    }
}

#endif /* __VQE_CAPS_H__ */
