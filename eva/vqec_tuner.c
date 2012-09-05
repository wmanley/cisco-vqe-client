/*
 * Copyright (c) 2006-2010 by Cisco Systems, Inc.
 * All rights reserved.
 */
/*
 * vqec_tuner.c - Implementation of vqec_tuner. 
 * 
 * VQE-C "tuner" is a conceptual abstraction defined to encompass the
 * pairing of an input and output stream.  
 */
/* included for unit testing purposes */
#ifdef _VQEC_UTEST_INTERPOSERS
#include "test_vqec_utest_interposers.h"
#endif

#include "vqe_port_macros.h"
#include "vqec_tuner.h"
#include "vqec_error.h"
#include "vqec_cli_interface.h"
#include "vqec_debug.h"
#include "vqec_dp_api.h"
#include "vqec_ifclient.h"
#include "vqec_assert_macros.h"
#include "vqec_drop.h"
#include "vqec_igmp.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

/*
 * Internal tuner object
 */
typedef struct vqec_tuner_t_ {
    char name[VQEC_MAX_NAME_LEN+1];    /*!< name */
    vqec_tunerid_t id;                 /*!< tuner id */
    vqec_dp_tunerid_t dp_id;           /*!< associated dataplane tuner's ID */
    vqec_chanid_t chanid;              /*!< bound channel id */
    int32_t context_id;                /*!< context-id for the decoder */
    int output_sock_fd;                /*!< fd of output sock of kernel dp */
    int output_sock_buf;               /*!< so_rcvbuf of output sock */
    int output_sock_enable;            /*!< output sock needs to be created */
} vqec_tuner_t;

/*
 * Stats that are tracked by the tuner module, across all tuners
 */
typedef struct vqec_tuner_mgr_stats_t_ {
    uint64_t  channel_change_requests; /*!<
                                        *!< number of channel changes
                                        *!< which have been requested
                                        */
    uint64_t  queue_drops;             /*!<
                                        *!< number of times a post-repair
                                        *!< stream packet was dropped due
                                        *!< to enqueue failure at a tuner
                                        */
} vqec_tuner_mgr_stats_t;

/*
 * Database of tuner objects within the tuner module
 */
typedef struct vqec_tuner_mgr_t_ {
    int32_t max_tuners;                /*!< max tuner instances */ 
    vqec_tuner_mgr_stats_t historical_stats_cumulative;
    vqec_tuner_mgr_stats_t historical_stats;
                                       /*!<
                                        *!< Stats collected from all
                                        *!< previously-tuned tuners.
                                        *!< These get bumped only upon
                                        *!< a tuner being unbound.
                                        */
    vqec_tuner_t *tuners[];            /*!< array of tuner ptrs */
} vqec_tuner_mgr_t;

/*
 * All tuners are stored within the tuner manager.
 */
 static vqec_tuner_mgr_t *s_vqec_tuner_mgr = NULL;

/**
 * Returns TRUE if the supplied tuner ID falls within the range supported
 * by the tuner manager, FALSE otherwise.  Included in this check is
 * verification that the tuner manager is initialized (i.e., an uninitialized
 * tuner manager supports no valid IDs).
 */
#define vqec_tuner_id_is_valid(id) \
    (s_vqec_tuner_mgr && ((id) >= 0) && ((id) < s_vqec_tuner_mgr->max_tuners))

/**
 * Returns a pointer to the tuner corresponding to ID, or NULL if none exists.
 */
#define vqec_tuner_get_tuner_by_id(id) \
    (vqec_tuner_id_is_valid((id)) ? s_vqec_tuner_mgr->tuners[(id)] : NULL)

/**
 * Get the output socket fd associated with the tuner.
 *
 * @param[in] id  ID of the tuner.
 * @return        fd associated with the tuner.
 */
int vqec_tuner_get_output_sock_fd (vqec_tunerid_t id)
{
    vqec_tuner_t *t;

    t = vqec_tuner_get_tuner_by_id(id);
    if (t && (t->output_sock_fd > 0)) {
        return (t->output_sock_fd);
    }
    return (VQEC_DP_OUTPUT_SHIM_INVALID_FD);
}

/**
 * Create tuner. It will allocate memory for tuner object and zero the memory.
 *
 * @param[out] vqec_tuner_t * Pointer of allocated tuner.
 */
static vqec_tuner_t *
vqec_tuner_create_tuner (void) 
{
    vqec_tuner_t * tuner;

    tuner = malloc(sizeof(vqec_tuner_t));
    if (!tuner) {
        syslog_print(VQEC_MALLOC_FAILURE, "tuner malloc error");
        return NULL;
    }
    
    memset(tuner, 0, sizeof(vqec_tuner_t));
    return tuner;
}

/**
 * Validate a string as potential name for a tuner.
 * Valid name strings have the following characteristics:
 *    1) Are at least one character long (a NULL or empty string is invalid)
 *    2) Have only alphanumeric characters
 *
 * Note that if the name string is not NULL-terminated, then only up to the 
 * first VQEC_MAX_TUNER_NAMESTR_LEN characters of the string are considered.
 *
 * @param[in] name name of the tuner, NOT necessarily valid (e.g. may be NULL,
 * have invalid chars, or may not be NULL-terminated)
 */
static boolean
vqec_tuner_is_valid_name (const char *name)
{
    boolean status = TRUE;  /* assume string is valid */
    int32_t i = 0;

    if (!name || (name[0] == '\0')) {
        status = FALSE;
        goto done;
    }
        
    while (i < VQEC_MAX_TUNER_NAMESTR_LEN) {
        if (name[i] == '\0') {
            goto done;
        } else if (!isalnum(name[i])) {
            status = FALSE;
            goto done;
        }
        i++;
    }    
    
done:
    return (status);
}

/**
 * Looks up a tuner given a valid tuner name.
 * NOTE:  The given name need not be NULL-terminated--only the first
 *        VQEC_MAX_TUNER_NAMESTR_LEN characters are used in the lookup.
 *
 * @param[in] name name of tuner to find, MUST be valid.
 */
static vqec_tunerid_t
vqec_tuner_get_id_by_name_internal (const char *name)
{
    int32_t i;

    for (i = 0; i < s_vqec_tuner_mgr->max_tuners; i++) {
        if (s_vqec_tuner_mgr->tuners[ i ] &&
            (strncmp(name, s_vqec_tuner_mgr->tuners[ i ]->name, 
                     VQEC_MAX_TUNER_NAMESTR_LEN) == 0)) {
            return s_vqec_tuner_mgr->tuners[i]->id;
        }
    }

    return VQEC_TUNERID_INVALID;
}

/*
 * Unbinds a tuner from the channel to which it is currently bound, if any.
 *
 * @param[in] tuner  Tuner whose channel is to be unbound.
 */
void
vqec_tuner_unbind_chan_internal (vqec_tuner_t *tuner)
{
    vqec_dp_output_shim_tuner_status_t output_shim_tuner_status;
    vqec_dp_error_t status_dp;

    /* Nothing to do if the channel is not bound */
    if (tuner->chanid == VQEC_CHANID_INVALID) {
        return;
    }

    /*
     * Prior to unbinding the tuner's channel, roll the DP's tuner-specific
     * stats into the global/historical counters so they aren't lost.
     */
    status_dp = vqec_dp_output_shim_get_tuner_status(tuner->dp_id,
                                                     &output_shim_tuner_status,
                                                     FALSE);
    if (status_dp == VQEC_DP_ERR_OK) {
        s_vqec_tuner_mgr->historical_stats.queue_drops +=
            output_shim_tuner_status.qdrops;
    }

    /* Update the cumulative historical stats */
    status_dp = vqec_dp_output_shim_get_tuner_status(tuner->dp_id,
                                                     &output_shim_tuner_status,
                                                     TRUE);
    if (status_dp == VQEC_DP_ERR_OK) {
        s_vqec_tuner_mgr->historical_stats_cumulative.queue_drops +=
            output_shim_tuner_status.qdrops;
    }

    (void)vqec_chan_unbind(tuner->chanid, tuner->dp_id);
    tuner->chanid = VQEC_CHANID_INVALID;

    /* deactivate created output sock in dp and close it */
    if (tuner->output_sock_fd > 0) {
        vqec_dp_output_shim_add_tuner_output_socket(
            tuner->dp_id,
            VQEC_DP_OUTPUT_SHIM_INVALID_FD);
        close(tuner->output_sock_fd);
        tuner->output_sock_fd = 0;
    }
}

/**
 * Frees a tuner, given a pointer to it.  All resources held by the tuner
 * are released.
 *
 * @param[in] tuner Pointer of tuner to delete.
 */
void vqec_tuner_destroy_internal (vqec_tuner_t *tuner) 
{
    if (!tuner) {
        return;
    }

    vqec_tuner_unbind_chan_internal(tuner);

    vqec_dp_output_shim_destroy_dptuner(tuner->dp_id);
    free(tuner);
}


static boolean
vqec_tuner_create_output_socket (vqec_tuner_t *t)
{
#define SOCKET_ERR_STRLEN 80
    int sockfd;
    char errbuf[SOCKET_ERR_STRLEN];
    char logbuf[SOCKET_ERR_STRLEN];

    if (!t || t->dp_id == VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID) {
        syslog_print(VQEC_ERROR, "invalid args to create output socket");
        return (FALSE);
    }

    /* create output socket */
    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        syslog_print(VQEC_ERROR, "error in creating the output socket");
        return (FALSE);
    }
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0) {
        if (!strerror_r(errno, errbuf, SOCKET_ERR_STRLEN)) {
            snprintf(logbuf, SOCKET_ERR_STRLEN,
                     "fcntl error in creating output socket: %s", errbuf);
            syslog_print(VQEC_ERROR, logbuf);
        }
        return (FALSE);
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &t->output_sock_buf,
                   sizeof(t->output_sock_buf)) == -1) {
        syslog_print(VQEC_ERROR, "error in setsockopt() of the output "
                     "socket");
        return (FALSE);
    }

    vqec_dp_output_shim_add_tuner_output_socket(t->dp_id, sockfd);
    t->output_sock_fd = sockfd;

    return (TRUE);
}

/**
 * Create a new tuner and store it within the tuner manager.
 *
 * If no name is specified, the tuner will be allocated a name corresponding 
 * to its array position, e.g., "10".  If name is specified, it must not be
 * an empty string, and should consist of alphanumeric characters only, 
 * excluding spaces; length is truncated to VQEC_MAX_TUNER_NAMESTR_LEN chars
 * (including the trailing \0).
 * 
 * [Invariant] Tuner module must have been initialized prior to this call.
 * @param[out] id ID of the newly created tuner, or VQEC_TUNERID_INVALID
 *                 if no tuner was created.
 * @param[in] name Name of the created tuner.  Note:  only up to the 
 * first VQEC_MAX_TUNER_NAMESTR_LEN characters of this string are considered.
 * @param[out] vqec_error_t VQEC_OK on success, failure code otherwise.
 */
vqec_error_t
vqec_tuner_create (vqec_tunerid_t *id, 
                   const char *name) 
{
    vqec_error_t status = VQEC_OK;
    char tmp_name[VQEC_MAX_NAME_LEN];
    vqec_tunerid_t candidate_id;
    vqec_dp_tunerid_t dp_id = VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID;
    int i;
    vqec_tuner_t *tuner = NULL;
    vqec_syscfg_t v_cfg;

    if (!s_vqec_tuner_mgr) {
        status = VQEC_ERR_NOTINITIALIZED;
        goto done;
    }
    
    /* Validate an ID can be returned */
    if (!id) {
        status = VQEC_ERR_INVALIDARGS;
        goto done;
    }
    /* Validate the name, if one was provided */
    if (name) {
        if (!vqec_tuner_is_valid_name(name)) {
            status = VQEC_ERR_INVALIDARGS;
            goto done;
        }
        if (vqec_tuner_get_id_by_name_internal(name) != VQEC_TUNERID_INVALID) {
            status = VQEC_ERR_EXISTTUNER;
            goto done;
        }
    }

    vqec_syscfg_get(&v_cfg);

    /* Try to find an unused ID slot */
    candidate_id = VQEC_TUNERID_INVALID;
    for (i = 0; i < s_vqec_tuner_mgr->max_tuners; i++) {
        if (!s_vqec_tuner_mgr->tuners[ i ]) {
            candidate_id = i;
            break;
        }
    }       
    if (candidate_id == VQEC_TUNERID_INVALID) {
        status = VQEC_ERR_MAXLIMITTUNER;
        goto done;
    }

    /*
     * If a name was not provided, generate one based on the ID just found
     * and validate its uniqueness.  If a tuner have the auto-generated
     * name is found, then the new request is considered a duplicate.
     */
    if (!name) {
        snprintf(tmp_name, VQEC_MAX_NAME_LEN, "%d", candidate_id);
        if (vqec_tuner_get_id_by_name_internal(tmp_name) !=
            VQEC_TUNERID_INVALID) {
            status = VQEC_ERR_EXISTTUNER;
            goto done;
        }
        name = tmp_name;
    }

    /* Create the control plane tuner */
    tuner = vqec_tuner_create_tuner();
    if (!tuner) {                
        status = VQEC_ERR_CREATETUNER;
        goto done;
    }
    /* Create the data plane tuner */
    if (vqec_dp_output_shim_create_dptuner(candidate_id,
                                           &dp_id,
                                           (char *)name, 
                                           VQEC_DP_TUNER_MAX_NAME_LEN) !=
        VQEC_DP_ERR_OK) {
        status = VQEC_ERR_CREATETUNER;
        goto done;
    }

    /*
     * Supplied names have been validated, but are not necessarily 
     * NULL-terminated.  So zero the target string and copy up the 
     * maximum string length supported, excluding the NULL terminator.
     */
    memset(tuner->name, 0, VQEC_MAX_TUNER_NAMESTR_LEN);
    strncpy(tuner->name, name, VQEC_MAX_TUNER_NAMESTR_LEN);
    /* Initialize the remaining fields in the tuner itself */
    tuner->id = candidate_id;
    tuner->dp_id = dp_id;
    tuner->chanid = VQEC_CHANID_INVALID;
    tuner->output_sock_buf = v_cfg.so_rcvbuf;
    tuner->output_sock_enable = v_cfg.deliver_paks_to_user;

    /* Insert tuner into the database */
    s_vqec_tuner_mgr->tuners[candidate_id] = tuner;

    *id = candidate_id;

done:
    if (status != VQEC_OK) {
        if (id) {
            *id = VQEC_TUNERID_INVALID;
        }
        if (dp_id != VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID) {
            (void)vqec_dp_output_shim_destroy_dptuner(dp_id);
        }
        if (tuner) {
            vqec_tuner_destroy_internal(tuner);
        }
        snprintf(s_debug_str, DEBUG_STR_LEN, ": %s", vqec_err2str(status));
    } else {
        snprintf(s_debug_str, DEBUG_STR_LEN, ": tuner %d created", *id);
    }
    VQEC_DEBUG(VQEC_DEBUG_TUNER, "%s(name=%s)%s\n", __FUNCTION__, name, 
               s_debug_str);
    return (status);
}

/**
 *
 * Delete a tuner given its tuner id, and remove it from the tuner manager.
 *
 * @param[in] id Tuner id to delete.
 * @param[out] vqec_error_t Returns VQEC_OK if the tuner is found,
 * otherwise returns failure code.
 */
vqec_error_t
vqec_tuner_destroy (vqec_tunerid_t id) 
{    
    vqec_tuner_t *tuner;
    vqec_error_t status = VQEC_OK;

    /* Check if supplied ID is not valid or does not exist */
    tuner = vqec_tuner_get_tuner_by_id(id);
    if (!tuner) {
        status = VQEC_ERR_NOSUCHTUNER;
        goto done;
    }

    /* Tuner exists, so remove it from the database and destroy it */
    
    /* destroy the igmp-proxy state associated with the tuner */
    vqec_igmp_destroy(id);
    vqec_tuner_destroy_internal(tuner);
    s_vqec_tuner_mgr->tuners[id] = NULL;

 done:
    VQEC_DEBUG(VQEC_DEBUG_TUNER, "%s(id=%d)%s\n", __FUNCTION__, id,
               vqec_err2str_complain_only(status));
    return (status);
}

/**
 * Bind a channel to a tuner
 * @param[in] id ID of the tuner whose channel is to change
 * @param[in] chanid - ID of the channel to bind to the tuner
 * @param[in] bp - bind params
 * @param[in] chan_event_cb: chan event cb function
 * @param[out] success or error code.
 */
vqec_error_t
vqec_tuner_bind_chan (vqec_tunerid_t id,
                      vqec_chanid_t chanid,
                      const vqec_bind_params_t *bp,
                      const vqec_ifclient_chan_event_cb_t *chan_event_cb) 
{
    vqec_tuner_t *tuner;
    vqec_error_t status = VQEC_OK;

    /* Check if supplied tuner ID is not valid or does not exist */
    tuner = vqec_tuner_get_tuner_by_id(id);
    if (!tuner) {
        status = VQEC_ERR_NOSUCHTUNER;
        goto done;
    }
    /*
     * Explicitly reject VQEC_CHANID_INVALID.  Callers must use the
     * vqec_tuner_unbind() API instead for this operation.
     */
    if (chanid == VQEC_CHANID_INVALID) {
        status = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    if (tuner->chanid == chanid) {
        /* Tuner is already associated with the requested channel */
        goto done;
    }

    /* Record the channel change request */
    s_vqec_tuner_mgr->historical_stats.channel_change_requests++;
    s_vqec_tuner_mgr->historical_stats_cumulative.channel_change_requests++;

    /* Unbind the tuner from its current channel if necessary */
    vqec_tuner_unbind_chan_internal(tuner);

    /* create output socket for tuner */
    if (tuner->output_sock_enable) {
        if (!vqec_tuner_create_output_socket(tuner)) {
            syslog_print(VQEC_ERROR, "cannot create output socket");
            status = VQEC_ERR_INTERNAL;
            goto done;
        }
    }

    /* Bind the new channel to the tuner */
    if (vqec_chan_bind(chanid, tuner->dp_id, bp, chan_event_cb) 
        != VQEC_CHAN_ERR_OK) {
        status = VQEC_ERR_INTERNAL;
        goto done;
    }

    /* Record the tuner's bound channel */
    tuner->chanid = chanid;

done:
    VQEC_DEBUG(VQEC_DEBUG_TUNER, "%s(id=%d,chanid=0x%08x,bp=%p)%s\n",
               __FUNCTION__, id, chanid, bp,
               vqec_err2str_complain_only(status));
    return (status);
}

/**
 * Unbind a tuner from its currently-bound channel.
 *
 * @param[in] id ID of tuner
 * @param[out] vqec_error_t Returns VQEC_OK or error code upon failure.
 */
vqec_error_t
vqec_tuner_unbind_chan (vqec_tunerid_t id) 
{
    vqec_tuner_t *tuner;
    vqec_error_t status = VQEC_OK;
   
    /* Check if supplied tuner ID is not valid or does not exist */
    tuner = vqec_tuner_get_tuner_by_id(id); 
    if (!tuner) {
        status = VQEC_ERR_NOSUCHTUNER;
        goto done;
    }

    vqec_tuner_unbind_chan_internal(tuner);

 done:
    VQEC_DEBUG(VQEC_DEBUG_TUNER, "%s(id=%d)%s\n", __FUNCTION__, id,
               vqec_err2str_complain_only(status));
    return (status);
}


/**
 * Retrieve a tuner from a tuner manager by tuner name.
 * NOTE:  Only up to the first VQEC_MAX_TUNER_NAMESTR_LEN characters of 
 *        the string are considered.
 *
 * @param[in] name name of the tuner, NOT necessarily valid (e.g. may be NULL,
 * have invalid chars, or may not be NULL-terminated)
 * @param[out] vqec_tunerid_t ID of the found tuner, or VQEC_TUNERID_INVALID
 * if not found.
 */
vqec_tunerid_t
vqec_tuner_get_id_by_name (const char *name) 
{
    if (!s_vqec_tuner_mgr) {
        return (VQEC_TUNERID_INVALID);
    }

    if (!name || !vqec_tuner_is_valid_name(name)) {
        return (VQEC_TUNERID_INVALID);
    }
    
    return (vqec_tuner_get_id_by_name_internal(name));
}

/**
 * Returns the tuner name associated with the given tuner ID, or 
 * NULL if the supplied tuner ID does not exist.
 *
 * @param[in] id ifclient ID of the tuner
 * @param[out] char * Name of associated tuner
 */
const char *
vqec_tuner_get_name (vqec_tunerid_t id)
{
    vqec_tuner_t *tuner = vqec_tuner_get_tuner_by_id(id);
    
    return (tuner ? tuner->name : NULL);
}

/**
 * Returns the dataplane tuner ID associated with a given tuner ID.
 *
 * @param[in] id ifclient ID of the tuner
 * @return       dataplane ID of the tuner identified by id.
 */
vqec_dp_tunerid_t
vqec_tuner_get_dptuner (vqec_tunerid_t id)
{
    vqec_tuner_t *tuner = vqec_tuner_get_tuner_by_id(id);
  
    return (tuner ? tuner->dp_id : VQEC_DP_OUTPUT_SHIM_INVALID_TUNER_ID);
}

/**
 * Returns the current active channel associated with a given tuner ID.
 *
 * @param[in] id ifclient ID of the tuner
 * @param[out] chanid currently tuned channel, or VQEC_CHANID_INVALID if none
 * @param[out] vqec_error_t VQEC_OK upon success or failure code otherwise.
 */
vqec_error_t
vqec_tuner_get_chan (vqec_tunerid_t id,
                     vqec_chanid_t *chanid)
{
    vqec_error_t status = VQEC_OK;
    vqec_tuner_t *tuner;
    
    if (!chanid) {
        status = VQEC_ERR_INVALIDARGS;
        goto done;
    }
    tuner = vqec_tuner_get_tuner_by_id(id);
    if (!tuner) {
        status = VQEC_ERR_NOSUCHTUNER;
        goto done;
    }
    *chanid = tuner->chanid;

done:
    if ((status != VQEC_OK) && chanid) {
        *chanid = VQEC_CHANID_INVALID;
    }
    return (status);
}

/*!
  Get the first tuner in the tuner list.
  @return Id of the first tuner - this is used for iteration, thereofore we 
  don't want to return pointers, since the iteration inherently is not
  a thread safe operation.
*/
vqec_tunerid_t
vqec_tuner_get_first_tuner_id (void) 
{
    int i;
    
    if (!s_vqec_tuner_mgr) {
        return (VQEC_TUNERID_INVALID);
    }

    for (i = 0; i < s_vqec_tuner_mgr->max_tuners; i++) {        
        if (s_vqec_tuner_mgr->tuners[ i ]) {
            return s_vqec_tuner_mgr->tuners[ i ]->id;
        }
    }

    return VQEC_TUNERID_INVALID;
}

/*!
  Get the next tuner
  @param[in] tuner id for the "current" tuner. To prevent accidental circular
  recursion,  "cur" must be a tuner id in the valid range.
  @return Id of the next tuner. 
 */
vqec_tunerid_t 
vqec_tuner_get_next_tuner_id (const vqec_tunerid_t cur) 
{
    int i;

    if (!s_vqec_tuner_mgr ||
        (cur == VQEC_TUNERID_INVALID) ||
        (cur >= s_vqec_tuner_mgr->max_tuners - 1)) {
        return (VQEC_TUNERID_INVALID);
    }

    for (i = cur + 1; i < s_vqec_tuner_mgr->max_tuners; i++) {
        if (s_vqec_tuner_mgr->tuners[ i ]) {
            return s_vqec_tuner_mgr->tuners[ i ]->id;
        }
    }

    return VQEC_TUNERID_INVALID;
}

/**
 * Retrieve the stats shown for the "show counters <tuner>" command.
 * These stats are a combination of:
 *    tuner's channel session:
 *       (channel stats since the tuner's channel became active)
 *    tuner session
 *       (tuner stats since the tuner was last bound to its channel)
 *
 * @param[in]  id           ID of tuner whose stats are requested
 * @param[out] stats        contains stats for tuner
 * @param[out] vqec_error_t VQEC_OK upon success, or error code upon failure
 */
vqec_error_t
vqec_tuner_get_stats_legacy (vqec_tunerid_t id, vqec_ifclient_stats_channel_t
                             *stats)
{
    vqec_tuner_t *tuner;
    vqec_chan_err_t status_channel;
    vqec_dp_error_t status_dp;
    vqec_dp_output_shim_tuner_status_t output_shim_tuner_status;

    if (!stats) {
        return VQEC_ERR_INVALIDARGS;
    }
    memset(stats, 0, sizeof(*stats));

    /* Check if supplied ID is not valid or does not exist */
    tuner = vqec_tuner_get_tuner_by_id(id);
    if (!tuner) {
        return VQEC_ERR_NOSUCHTUNER;
    }

    if (tuner->chanid != VQEC_CHANID_INVALID) {
        status_channel = vqec_chan_get_counters_channel(tuner->chanid, stats,
                                                        FALSE); 
        if (status_channel != VQEC_CHAN_ERR_OK) {
            return (VQEC_ERR_INTERNAL);
        }
    }
    /* Load the "sink" stats */
    status_dp =
        vqec_dp_output_shim_get_tuner_status(tuner->dp_id,
                                             &output_shim_tuner_status,
                                             FALSE);
    if (status_dp != VQEC_DP_ERR_OK) {
        return (VQEC_ERR_INTERNAL);
    }
    stats->tuner_queue_drops = output_shim_tuner_status.qdrops;
    /*
     * Note:  the vqec_ifclient_stats_t structure doesn't have a
     *        "tuner_queue_outputs" field, but this is what 
     *        "show counters <tuner>" displays.  So the post_repair_outputs
     *        field is used to export and display the tuner queue outputs
     *        in this special case.
     */
    stats->post_repair_outputs = output_shim_tuner_status.qoutputs;

    return (VQEC_OK);
}


/**
 * Get the global/historical tuner stats
 *
 * @param[out] stats         Structure in which tuner stats are loaded 
 * @param[out] vqec_error_t success or error code.
 */
vqec_error_t 
vqec_tuner_get_stats_global (vqec_ifclient_stats_t *stats, 
                             boolean cumulative)
{
    vqec_tuner_t *tuner;
    vqec_dp_error_t status_dp;
    vqec_dp_output_shim_tuner_status_t output_shim_tuner_status;
    vqec_error_t status = VQEC_OK;
    int i;

    if (!stats) {
        status = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    if (s_vqec_tuner_mgr) {
        if (cumulative) {
            stats->tuner_queue_drops = 
                 s_vqec_tuner_mgr->historical_stats_cumulative.queue_drops;
            stats->channel_change_requests =
        s_vqec_tuner_mgr->historical_stats_cumulative.channel_change_requests;
        } else {
            stats->tuner_queue_drops = 
                s_vqec_tuner_mgr->historical_stats.queue_drops;
            stats->channel_change_requests =
                s_vqec_tuner_mgr->historical_stats.channel_change_requests;
        }
        for (i=0; i < s_vqec_tuner_mgr->max_tuners; i++) {
            tuner = s_vqec_tuner_mgr->tuners[i];
            if (tuner && (tuner->chanid != VQEC_CHANID_INVALID)) {
                /* 
                 * Load the stats stored on each tuner for current 
                 * bindings 
                 */
                 status_dp = vqec_dp_output_shim_get_tuner_status(
                     tuner->dp_id,
                     &output_shim_tuner_status, cumulative);
                 if (status_dp != VQEC_DP_ERR_OK) {
                     status = VQEC_ERR_INTERNAL;
                     goto done;
                 }
                 stats->tuner_queue_drops += 
                     output_shim_tuner_status.qdrops;
            }
        }
    }
done:
    return (status);
} 

/**
 * Clear stats for all tuners
 */
void
vqec_tuner_clear_stats (void)
{    
    int32_t i;
    
    if (!s_vqec_tuner_mgr) {
        return;
    }

    
    memset(&s_vqec_tuner_mgr->historical_stats, 0,
           sizeof(vqec_tuner_mgr_stats_t));
    for (i=0; i < s_vqec_tuner_mgr->max_tuners; i++) {
        if (s_vqec_tuner_mgr->tuners[i]) {
            (void)vqec_dp_output_shim_clear_tuner_counters(
                s_vqec_tuner_mgr->tuners[i]->dp_id);
        }
    }
}

/**
 * Get the PAT after a rapid channel change has completed
 *.
 * @param[in] id identifies tuner.
 * @param[in] buf Pointer to the buffer in which PAT is copied.
 * @param[out] pat_len Actual length of the PAT copied.
 * @param[in] buf_len Length of the input content buffer.
 * @param[out] success or error code.
 */
vqec_error_t
vqec_tuner_get_pat (vqec_tunerid_t id,
                    uint8_t *buf, uint32_t *pat_len, uint32_t buf_len)
{
#ifdef HAVE_FCC

    vqec_error_t ret = VQEC_OK;
    vqec_tuner_t *tuner;
    vqec_chan_err_t chan_err;

    tuner = vqec_tuner_get_tuner_by_id(id);
    if (!tuner) {
        return VQEC_ERR_NOSUCHTUNER;
    }
    
    if (!buf ||
        !buf_len ||
        !pat_len) {
        return VQEC_ERR_INVALIDARGS;
    }
    *pat_len = 0;

    chan_err = vqec_chan_get_pat(tuner->chanid,
                                 buf, pat_len, buf_len);
    if (chan_err != VQEC_CHAN_ERR_OK) {
        ret = VQEC_ERR_INTERNAL;
    }
    return (ret);

#else /* HAVE_FCC */

    if (pat_len) {
        *pat_len = 0;
    }
    return (VQEC_OK);

#endif /* HAVE_FCC */ 
}

/**
 * Get the PMT after a rapid channel change has completed
 *.
 * @param[in] id identifies tuner.
 * @param[out] pid PID for the PMT section.
 * @param[in] buf Pointer to the buffer in which PMT is copied.
 * @param[out] pmt_len Actual length of the PMT copied.
 * @param[in] buf_len Length of the input content buffer.
 * @param[out] success or error code.
 */
vqec_error_t
vqec_tuner_get_pmt (vqec_tunerid_t id,
                    uint16_t *pid, uint8_t *buf, 
                    uint32_t *pmt_len, uint32_t buf_len)
{
#ifdef HAVE_FCC

    vqec_error_t ret = VQEC_OK;
    vqec_tuner_t *tuner;
    vqec_chan_err_t chan_err;

    tuner = vqec_tuner_get_tuner_by_id(id);
    if (!tuner) {
        return VQEC_ERR_NOSUCHTUNER;
    }

    if (!pid ||
        !buf ||
        !buf_len || 
        !pmt_len) {
        return VQEC_ERR_INVALIDARGS;
    }
    *pmt_len = 0;
    *pid = 0;
    
    chan_err = vqec_chan_get_pmt(tuner->chanid, pid,
                                 buf, pmt_len, buf_len);
    if (chan_err != VQEC_CHAN_ERR_OK) {
        ret = VQEC_ERR_INTERNAL;
    }
    return (ret);

#else /* HAVE_FCC */

    if (pmt_len) {
        *pmt_len = 0;
    }
    if (pid) {
        *pid = 0;
    }
    return (VQEC_OK);

#endif /* HAVE_FCC */ 
}

/**
 * 
 * Dump the tuner information for debugging.
 * @param[in] id identifies tuner to dump.
 * @param[in] 32-bit flag specifying cli display options
 * @param[out] vqec_error_t Returns VQE_OK or error code upon failure.
 */
vqec_error_t
vqec_tuner_dump (vqec_tunerid_t id, unsigned int options_flag)
{
    vqec_error_t status = VQEC_OK;
    vqec_tuner_t *tuner;
#define VQEC_TUNER_DUMP_CHAN_STR_LEN 8
    char tmp_str[VQEC_TUNER_DUMP_CHAN_STR_LEN];
    vqec_dp_chanid_t dp_chanid;
    boolean collect_stats = FALSE;

    (void)vqec_dp_debug_flag_get(VQEC_DP_DEBUG_COLLECT_STATS, &collect_stats);

    /* Check if supplied ID is not valid or does not exist */
    tuner = vqec_tuner_get_tuner_by_id(id);
    if (!tuner) {
        return VQEC_ERR_NOSUCHTUNER;
    }

    dp_chanid = vqec_chan_id_to_dpchanid(tuner->chanid);
    
    if (options_flag & DETAIL_MASK) {
        if (tuner->chanid == VQEC_CHANID_INVALID) {
            snprintf(tmp_str, VQEC_TUNER_DUMP_CHAN_STR_LEN, "<none>");
        } else {
            snprintf(tmp_str, VQEC_TUNER_DUMP_CHAN_STR_LEN, "0x%08x",
                     tuner->chanid);
        }
        CONSOLE_PRINTF("Tuner name:                 %s\n"
                       "  Tuner ID:                 %d\n"
                       "  DP Tuner ID:              %d\n"
                       "  Bound to Channel ID:      %s\n",
                       tuner->name,
                       tuner->id,
                       tuner->dp_id,
                       tmp_str);
    } else {
        CONSOLE_PRINTF("Tuner name:                 %s\n",
                       tuner->name);
    }

    if (tuner->chanid == VQEC_CHANID_INVALID) {
        return (status);
    }

    if (options_flag & (DETAIL_MASK | BRIEF_MASK | CHANNEL_MASK)) {
        vqec_chan_display_state(tuner->chanid, options_flag);
    }

    if (options_flag & (DETAIL_MASK | PCM_MASK | COUNTERS_MASK)) {
        vqec_cli_pcm_dump(dp_chanid, FALSE);
        vqec_cli_nll_dump(dp_chanid, FALSE);
    } else if (options_flag & BRIEF_MASK) {
        vqec_cli_pcm_dump(dp_chanid, TRUE);
        vqec_cli_nll_dump(dp_chanid, TRUE);
    }

    if (options_flag & (DETAIL_MASK | FEC_MASK | COUNTERS_MASK)) {
        vqec_cli_fec_dump(dp_chanid, FALSE);
    } else if (options_flag & BRIEF_MASK) {
        vqec_cli_fec_dump(dp_chanid, TRUE);
    }

#if HAVE_FCC
    if (options_flag & (DETAIL_MASK | RCC_MASK)) {
        vqec_chan_display_rcc_state(tuner->chanid, FALSE);
    } else if (options_flag & BRIEF_MASK) {
        vqec_chan_display_rcc_state(tuner->chanid, TRUE);
    }
#endif

    if (options_flag & (DETAIL_MASK | NAT_MASK)) {
        vqec_chan_show_nat_bindings(tuner->chanid); 
    }

    if ((options_flag & (DETAIL_MASK | LOG_MASK)) &&
        (collect_stats)) {
        vqec_cli_log_dump(dp_chanid, options_flag);
    }

    if (options_flag & DETAIL_MASK) {
        vqec_drop_dump();
        vqec_cli_input_shim_show_streams_tuner(tuner->id);
        vqec_cli_output_shim_show_tuner(tuner->dp_id);
        vqec_cli_output_shim_show_stream_tuner(tuner->id);
    }

    return (status);
}

/**
 *
 * Initialize the tuner module:
 *   - records the supplied maximum number of tuners that may be allocated
 *     (This value is assumed to be valid.) 
 *   - initializes tuner list
 *
 * @param[in] max_tuners  Maximum number of tuners to support.
 * @param[out] vqec_error_t VQEC_OK upon success, or else reason for 
 * failure.
 */
vqec_error_t
init_vqec_tuner_module (uint32_t max_tuners) 
{
    vqec_error_t status = VQEC_OK;

    if (!max_tuners || (max_tuners > VQEC_SYSCFG_MAX_MAX_TUNERS)) {
        status = VQEC_ERR_INVALIDARGS;
        goto done;
    }

    if (s_vqec_tuner_mgr) {
        status = VQEC_ERR_ALREADYINITIALIZED;
        goto done;
    }

    /* Allocate and zero the tuner manager database */
    s_vqec_tuner_mgr =
        (vqec_tuner_mgr_t *)calloc(1, 
                                   sizeof(vqec_tuner_mgr_t) + 
                                   (sizeof(vqec_tuner_t *) * max_tuners));
    if (!s_vqec_tuner_mgr) {
        syslog_print(VQEC_MALLOC_FAILURE, "tuner mgr creation failure");
        status = VQEC_ERR_MALLOC;
        goto done;
    }
    s_vqec_tuner_mgr->max_tuners = max_tuners;        

done:
    return (status);
}

/**
 *
 * De-initialize the tuner module:
 *   - resets the amount of tuners that can be supported to 0
 *   - destroys any existing tuners and removes them from the tuner database.
 */
void
vqec_tuner_module_deinit (void) 
{
    uint32_t i;

    if (!s_vqec_tuner_mgr) {
        goto done;
    }

    for (i = 0; i < s_vqec_tuner_mgr->max_tuners; i++) {
        if (s_vqec_tuner_mgr->tuners[i]) {
            vqec_tuner_destroy_internal(s_vqec_tuner_mgr->tuners[i]);
        }
    }
    free(s_vqec_tuner_mgr);
    s_vqec_tuner_mgr = NULL;
done:
    return;
}


/**---------------------------------------------------------------------------
 * Is a fastfill operation enabled for the tuner? Fastfill will be inactive
 * if the application requested no fastfill at bind time, or if the client or 
 * server had fastfill disabled via configuration. If called when the tuner
 * is not bound the method will return false.
 *
 * @param[in]  id Tuner identifier.
 * @param[out] uint8_t TRUE for fastfill enabled, FALSE otherwise.
 *----------------------------------------------------------------------------*/
uint8_t 
vqec_tuner_is_fastfill_enabled (vqec_tunerid_t id)
{
    vqec_tuner_t *tuner;

    /* Check if supplied ID is not valid or does not exist */
    tuner = vqec_tuner_get_tuner_by_id(id);
    if (!tuner) {
        return (FALSE);
    }

    /* Nothing to do if the channel is not bound */
    if (tuner->chanid == VQEC_CHANID_INVALID) {
        return (FALSE);
    }

    return (vqec_chan_is_fastfill_enabled(tuner->chanid));
}

/**---------------------------------------------------------------------------
 * Is a RCC operation enabled for the tuner?  RCC will be inactive
 * if the application requested no RCC at bind time, or if the client or 
 * server had RCC disabled via configuration.  If called when the tuner
 * is not bound the method will return FALSE.
 *
 * @param[in]  id Tuner identifier.
 * @param[out] uint8_t TRUE for RCC enabled, FALSE otherwise.
 *----------------------------------------------------------------------------
 */
uint8_t 
vqec_tuner_is_rcc_enabled (vqec_tunerid_t id)
{
    vqec_tuner_t *tuner;

    /* Check if supplied ID is not valid or does not exist */
    tuner = vqec_tuner_get_tuner_by_id(id);
    if (!tuner) {
        return (FALSE);
    }

    /* Nothing to do if the channel is not bound */
    if (tuner->chanid == VQEC_CHANID_INVALID) {
        return (FALSE);
    }

    return (vqec_chan_is_rcc_enabled(tuner->chanid));
}

