/*------------------------------------------------------------------
 * VAM Common stuff for IPC
 *
 * June 2006, Josh Gahm
 *
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __VAM_IPC_H__
#define __VAM_IPC_H__

typedef enum vam_ipc_proj_id_ {
    VAM_IPC_PROJ_INVALID = 0,
    VAM_IPC_CM_REQ,
    VAM_IPC_CM_RSP,
    VAM_IPC_MP_REQ,
    VAM_IPC_MP_RSP,
    VAM_IPC_MP_SHM,
    VAM_IPC_OBM_SHM,   /* shm used in output bandwidth management */
} vam_ipc_proj_id_t;

#define VAMIPC_KEY_FILE_DEF ".vamipc_key_file"

/*
 * vam_ipc_get_def_file_name
 * 
 * Returns the name of the default file that will be 
 * used to form the ipc keys.
 */
static inline const char * vam_ipc_get_def_file_name(void) 
{
    return "/var/tmp/" VAMIPC_KEY_FILE_DEF;
}


/* 
 * vam_ipc_get_key
 *
 * Behaves exactly like ftok except (i) if a NULL pointer
 * is passed as the first argument, the default value
 * of ~/VAMIPC_KEY_FILE_DEF is used (ii) if the file passed,
 * or defaulted, does not exist, the call will create it.
 * Like ftok, returns an ipc key on success, -1 on failure.
 */

key_t vam_ipc_get_key(const char * ipcf, int proj_id);

#if 0
/**
 * vam_shm_ret_t
 * @brief
 * Enumerate the return codes. Check vam_shm_ret_to_str() for descriptions.
 */
typedef enum vam_shm_ret_ {
    VAM_SHM_RET_OK = 0,
    VAM_SHM_RET_NO_KEY,
    VAM_SHM_RET_EACCES,
    VAM_SHM_RET_EINVAL,
    VAM_SHM_RET_ENOENT,
    VAM_SHM_RET_ENOMEM,
    VAM_SHM_RET_EEXIST,
    VAM_SHM_RET_EFAULT,
    VAM_SHM_RET_EIDRM,
    VAM_SHM_RET_EPERM,
    VAM_SHM_RET_EOVERFLOW,
    VAM_SHM_RET_UNSPEC,
} vam_shm_ret_t;

/**
 * vam_shm_ret_to_str
 * @brief
 *  Converts a return code to a descriptive string.
 * @param[in]  ret  - a return code
 * @return  
 *  Ptr to a descriptive string
 */
static inline const char *
vam_shm_ret_to_str (vam_shm_ret_t ret)
{
    switch (ret) {
    case VAM_SHM_RET_OK:
        return "OK";
    case VAM_SHM_RET_EACCES:
        return "EACCES - Permission denied";
    case VAM_SHM_RET_EINVAL:
        return "EINVAL - Invalid argument";
    case VAM_SHM_RET_ENOENT:
        return "ENOENT - No such file or directory";
    case VAM_SHM_RET_ENOMEM:
        return "ENOMEM - Out of memory";
    case VAM_SHM_RET_EEXIST:
        return "EEXIST - File exists";
    case VAM_SHM_RET_EFAULT:
        return "EFAULT - Bad adress";
    case VAM_SHM_RET_EIDRM:
        return "EIDRM - Identifier removed";
    case VAM_SHM_RET_EPERM:
        return "EPERM - Operation not permitted";
    case VAM_SHM_RET_EOVERFLOW:
        return "OVERFLOW - Value too large for defined data type";
    case VAM_SHM_RET_NO_KEY:
        return "Failed to generate a key";
    case VAM_SHM_RET_UNSPEC:
        return "Unspecified return code";
    default:
        return "Unknown";
    }
}
#endif

#endif /* __VAM_IPC_H__ */
