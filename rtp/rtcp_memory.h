/**-----------------------------------------------------------------
 * @brief
 * Declarations/definitions for RTCP memory data/functions.
 *
 * @file
 * rtcp_memory.h
 *
 * April 2007, Mike Lague.
 *
 * Copyright (c) 2007-2008, 2010 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef __RTCP_MEMORY_H__
#define __RTCP_MEMORY_H__

#include "../include/utils/vam_types.h"
#include "../include/utils/zone_mgr.h"

typedef enum rtcp_mem_obj_t_ {
    RTCP_OBJ_SESSION = 0,
    RTCP_MIN_OBJ = RTCP_OBJ_SESSION,  /* must be set to first object type */
    RTCP_OBJ_CLIENT_MEMBER = 1,
    RTCP_OBJ_CHANNEL_MEMBER = 2,
    RTCP_OBJ_SDES = 3,
    RTCP_OBJ_SENDER_INFO = 4,
    RTCP_MAX_OBJ = RTCP_OBJ_SENDER_INFO  /* must be set to last object type */
} rtcp_mem_obj_t;

#define RTCP_NUM_OBJTYPES  (RTCP_MAX_OBJ - RTCP_MIN_OBJ + 1)

typedef struct vqe_zone rtcp_mem_zone_t;

#define RTCP_MAX_ZONE_NAME  16

/*
 * rtp_member_id_subtype_t
 *
 * Defines the subtype of the RTCP object. This applies only to member objects.
 */
typedef enum rtcp_memory_subtype_t_{
    RTCP_CHANNEL_MEMBER = 1,        /* Channel members are associated with
                                      a provisioned channel. There are 
                                      typically 2 members channel members per
                                      channel. */
    RTCP_CLIENT_MEMBER              /* Client members are not created on a 
                                      per-channel basis. */
} rtcp_memory_subtype_t;

/*
 * Keeps statistics for an RTCP Memory Pool 
 */
typedef struct rtcp_mem_stats_t_ {
    uint64_t allocations;            /* # of current allocations */
    uint64_t allocations_hw;         /* Largest # of allocations ever */
    uint64_t allocations_failed;     /* # of RTCP memory allocations failed */
} rtcp_mem_stats_t;

typedef struct rtcp_mem_pool_t_ {
    int              chunk_size;
    int              max_chunks;
    char             *zone_suffix;
    char             zone_name[RTCP_MAX_ZONE_NAME];
    rtcp_mem_zone_t *zone;
    rtcp_mem_stats_t pool_stats;
} rtcp_mem_pool_t;

typedef struct rtcp_memory_t_ {
    char *name;
    int flags;
    int session_size;
    int allocated;
    rtcp_mem_pool_t pool[RTCP_NUM_OBJTYPES];
} rtcp_memory_t;


/*
 * function declarations
 */

void rtcp_cfg_memory(rtcp_memory_t *memory,
                     char *name,
                     int session_size,
                     int max_sessions,
                     int client_member_size,
                     int client_max_members,
                     int channel_member_size,
                     int channel_max_members);
boolean rtcp_init_memory(rtcp_memory_t *memory);
void rtcp_free_memory(rtcp_memory_t *memory);
int  rtcp_memory_allocated(rtcp_memory_t *memory, int *allocations);

void *rtcp_new_object(rtcp_memory_t *memory, 
                      rtcp_mem_obj_t type);
void rtcp_delete_object(rtcp_memory_t *memory, 
                        rtcp_mem_obj_t type,
                        void *object);
    
/*
 * session allocation/deallocation
 */

static inline void *
rtcp_new_session (rtcp_memory_t *memory) 
{
    return (rtcp_new_object(memory, RTCP_OBJ_SESSION));
}

static inline void
rtcp_delete_session (rtcp_memory_t *memory, void *session) 
{
    rtcp_delete_object(memory, RTCP_OBJ_SESSION, session);
}

/*
 * member allocation/deallocation
 */

static inline void *
rtcp_new_member (rtcp_memory_t *memory, rtcp_memory_subtype_t subtype) 
{
    if (subtype == RTCP_CLIENT_MEMBER) {
        return (rtcp_new_object(memory, RTCP_OBJ_CLIENT_MEMBER));

    } else if (subtype == RTCP_CHANNEL_MEMBER) {
        return (rtcp_new_object(memory, RTCP_OBJ_CHANNEL_MEMBER));

    } else {
        return (NULL);
    }
}

static inline void
rtcp_delete_member (rtcp_memory_t *memory,
                    rtcp_memory_subtype_t subtype, 
                    void *member) 
{
    rtcp_mem_obj_t obj_type = (subtype == RTCP_CLIENT_MEMBER) ? 
        RTCP_OBJ_CLIENT_MEMBER : RTCP_OBJ_CHANNEL_MEMBER;
    rtcp_delete_object(memory, obj_type, member);

}

#endif /* __RTCP_MEMORY_H__ */
