/**------------------------------------------------------------------------
 * @brief
 * Functions for RTCP memory initialization/allocation/deallocation.
 *
 * @file
 * rtcp_memory.c
 *
 * April 2007, Mike Lague.
 *
 * Copyright (c) 2007, 2010 by cisco Systems, Inc.
 * All rights reserved.
 *-------------------------------------------------------------------------
 */

#include "rtcp_memory.h"
#include "rtp_session.h"

/*
 * rtcp_cfg_memory
 *
 * Set up RTCP memory parameters.
 *
 * Parameters:
 * memory            -- ptr to class-specific memory info.
 * name              -- string to help identify memory zones:
 *                      zone names will be of the form "<name>-SE" 
 *                      (for sessions), "<name>-MB" (for members), etc.
 * session_size      -- size in bytes of per-session data, which may be
 *                      class-specific (e.g., sizeof(rtp_era_la_t)).
 * max_sessions      -- max. no. of sessions
 * client_member_size  -- size in bytes of per-member data, which may be
 *                      class-specific (e.g., sizeof(era_la_member_t)). This
 *                      pool is dedicated to client members.
 * client_max_members  -- max. no. of members, associated with clients
 * channel_member_size -- size in bytes of per-member data, for members 
 *                        dedicated to provisioned channels
 *
 *
 */

void 
rtcp_cfg_memory (rtcp_memory_t *memory,
                 char *name,
                 int session_size,
                 int max_sessions,
                 int client_member_size,
                 int client_max_members,
                 int channel_member_size,
                 int channel_max_members)
{
    rtcp_mem_pool_t *pool;

    if (!memory) {
        return;
    }

    memset(memory, 0, sizeof(rtcp_memory_t));

    memory->name = name;
    memory->flags = ZONE_FLAGS_STATIC;
    memory->session_size = session_size;
    memory->allocated = 0;

    pool = &memory->pool[RTCP_OBJ_SESSION];
    pool->chunk_size = session_size;
    pool->max_chunks = max_sessions;
    pool->zone_suffix = "-SE";

    pool = &memory->pool[RTCP_OBJ_CLIENT_MEMBER];
    pool->chunk_size = client_member_size;
    pool->max_chunks = client_max_members;
    pool->zone_suffix = "-MB";

    pool = &memory->pool[RTCP_OBJ_CHANNEL_MEMBER];
    pool->chunk_size = channel_member_size;
    pool->max_chunks = channel_max_members;
    pool->zone_suffix = "-CM";

    pool = &memory->pool[RTCP_OBJ_SDES];
    pool->chunk_size = RTP_MAX_SDES_LEN + 1;
    pool->max_chunks = 
        (client_max_members + channel_max_members) * RTCP_MAX_SDES_ITEMS;
    pool->zone_suffix = "-SD";

    pool = &memory->pool[RTCP_OBJ_SENDER_INFO];
    pool->chunk_size = RTP_MAX_SENDERS_CACHED * (sizeof(rtcp_rr_t));
    pool->max_chunks = client_max_members + channel_max_members;
    pool->zone_suffix = "-SI";
}

/*
 * rtcp_init_memory
 *
 * One-time memory zone initialization, for a derived class.
 *
 * Parameters:
 * memory              -- ptr to (class-specific) memory info.
 *
 * Returns:            TRUE if successful, FALSE otherwise.
 */

boolean 
rtcp_init_memory (rtcp_memory_t *memory)
{

    int i;
    rtcp_mem_pool_t *pool;
    boolean ok = TRUE;

    if (!memory) {
        return (FALSE);
    }

    for (i = 0, pool = &memory->pool[0]; i < RTCP_NUM_OBJTYPES; i++, pool++) {
        if ((pool->chunk_size) > 0 && (pool->max_chunks > 0)) {
            strncpy(pool->zone_name, memory->name, RTCP_MAX_ZONE_NAME-4);
            pool->zone_name[RTCP_MAX_ZONE_NAME-5] = '\0';
            strncat(pool->zone_name, pool->zone_suffix, 4);
            pool->zone_name[RTCP_MAX_ZONE_NAME-1] = '\0';
            pool->zone = zone_instance_get_loc(pool->zone_name,
                                               memory->flags,
                                               pool->chunk_size,
                                               pool->max_chunks,
                                               NULL, NULL);
            ok = pool->zone ? TRUE : FALSE;
            if (!ok) {
                break;
            }
        }
    }

    if (!ok) {
        RTP_SYSLOG(RTCP_MEM_INIT_FAILURE, pool->zone_name);
        rtcp_free_memory(memory);
    }
    return (ok);
}

/*
 * rtcp_free_memory
 *
 * Frees memory held by RTCP, for a derived class.
 *
 * Parameters:
 * memory              -- ptr to (class-specific) memory info.
 */

void 
rtcp_free_memory (rtcp_memory_t *memory)
{
    int i;
    rtcp_mem_pool_t *pool;

    if (!memory) {
        return;
    }

    for (i = 0, pool = &memory->pool[0]; i < RTCP_NUM_OBJTYPES; i++, pool++) {
        if (pool->zone) {
            (void)zone_instance_put(pool->zone);
            pool->zone = NULL;
        }
    }
}

/*
 * rtcp_memory_allocated
 *
 * Returns current status of allocations.
 * 
 * Parameters:
 * memory          -- ptr to memory info struct.
 * allocations     -- ptr to int: if non-NULL, 
 *                    no. of object allocations is placed here.
 *
 * Returns:        -- no. of "blocks" allocated
 */
int
rtcp_memory_allocated (rtcp_memory_t *memory, int *allocations)
{
    int i;
    rtcp_mem_pool_t *pool;

    if (allocations) {
        *allocations = 0;
    }
    if (memory) {
        if (allocations) {
            for (i = 0, pool = &memory->pool[0]; 
                 i < RTCP_NUM_OBJTYPES; 
                 i++, pool++) {
                *allocations += pool->pool_stats.allocations;
            }
        }
        return (memory->allocated);
    } else {
        return (0);
    }
}

/*
 * rtcp_new_object
 *
 * Allocates memory for an RTCP object.
 *
 * Parameters:
 * memory              -- ptr to (class-specific) memory info.
 * type                -- a value identifying the type of object.
 *
 * Returns:            if successful, a non-NULL pointer to the object;
 *                     else NULL
 */

void *
rtcp_new_object (rtcp_memory_t *memory, 
                 rtcp_mem_obj_t type)
{
    void *ptr = NULL;

    if (!memory) {
        return (NULL);
    }
    if (!(type >= RTCP_MIN_OBJ && type <= RTCP_MAX_OBJ)) {
        return (NULL);
    }

    ptr = zone_acquire(memory->pool[type].zone);
    if (ptr) {
        memory->pool[type].pool_stats.allocations++;
        if (memory->pool[type].pool_stats.allocations > 
            memory->pool[type].pool_stats.allocations_hw) {
            memory->pool[type].pool_stats.allocations_hw = 
                memory->pool[type].pool_stats.allocations;
        }

        memory->allocated += zm_elem_size(memory->pool[type].zone);
    } else {
        memory->pool[type].pool_stats.allocations_failed++;
    }

    return (ptr);
}


/*
 * rtcp_delete_object
 *
 * Deallocates memory for an RTCP object.
 *
 * Parameters:
 * memory              -- ptr to (class-specific) memory info.
 * type                -- a value identifying the type of object.
 * object              -- a pointer to the object.
 */

void 
rtcp_delete_object(rtcp_memory_t *memory, 
                   rtcp_mem_obj_t type,
                   void *object)
{
    if (!memory) {
        return;
    }
    if (!(type >= RTCP_MIN_OBJ && type <= RTCP_MAX_OBJ)) {
        return;
    }

    zone_release(memory->pool[type].zone, object);
    memory->pool[type].pool_stats.allocations--;
    memory->allocated -= zm_elem_size(memory->pool[type].zone);
}
