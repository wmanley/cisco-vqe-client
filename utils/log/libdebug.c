/********************************************************************
 * libdebug.c
 *
 * Generic functions that implement the VQE Debug Library API.
 * Taken from DC-OS utils/debug/libdebug.c
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

#include <log/syslog.h>
#include <log/libdebug.h>

#define MAXLINE 1024

/* VQE-S debug logging doesn't support VTY yet */
#ifdef SUPPORT_VTY
static int num_elem = -1;
#endif

/*
 * To set the flag for a given element.
 */
int32_t
debug_set_flag(debug_item_t *debugarr, int index, char *mod, char *vty)
{

#ifdef SUPPORT_VTY
    int ret, ind, bitmask, free_slot = -1, vty_pos = -1;

    /* 
     * If num_elem is not initialized, then do that in the first
     * call to set an element. This is a kludge to avoid changing the
     * debug_init() function's footprint.
     */
    if (num_elem == -1) {
        for (num_elem = 0; debugarr[num_elem].var != NULL; num_elem++);
    }

    bitmask = (1 << index);

    for (ind = 0; ind < MAXPATH; ind++) {
        if (debug_bm[ind].vtyname[0] == '\0') {
            if (free_slot == -1) {
                free_slot = ind;
            }
        } else if ((ret = strcmp(debug_bm[ind].vtyname, vty)) == 0) {
            vty_pos = ind;
            break;
        }
    }

    if ((vty_pos == -1) && (free_slot == -1)) {
        return DEBUG_SET_NO_SLOT;
    }

    *debugarr[index].var = TRUE;
    if (vty_pos != -1) {
        debug_bm[vty_pos].bitmask |= bitmask;
    } else {
        debug_bm[free_slot].bitmask = bitmask;
        strcpy(debug_bm[free_slot].vtyname, vty);
        debug_send_vty_message(-1, MTS_OPC_VSH_ADD_VTY, mod, vty);
    }

#else

    *debugarr[index].var = TRUE;

#endif /* SUPPORT_VTY */

#ifdef DEBUG_SET
    printf("After setting flag: debugarr[%d]: var=%d, desc=%s\n",
           index, *debugarr[index].var, debugarr[index].desc);
#endif /* ifdef DEBUG_SET */

    return SUCCESS;
}


/*
 * To reset the flag for a given element.
 */
int32_t
debug_reset_flag(debug_item_t *debugarr, int index, char *mod, char *vty)
{
#ifdef SUPPORT_VTY
    int ret, ind, bitmask, vty_pos = -1;

    /* 
     * If num_elem is not initialized, then do that in the first
     * call to set an element. This is a kludge to avoid changing the
     * debug_init() function's footprint.
     */
    if (num_elem == -1) {
        for (num_elem = 0; debugarr[num_elem].var != NULL; num_elem++);
    }

    bitmask = (1 << index);

    for (ind = 0; ind < MAXPATH; ind++) {
        if (debug_bm[ind].vtyname[0] != '\0') {
            if ((ret = strcmp(debug_bm[ind].vtyname, vty)) == 0) {
                vty_pos = ind;
                break;
            }
        }
    }

    if (vty_pos == -1) {
        return DEBUG_RESET_NO_VTY;
    }

    *debugarr[index].var = FALSE;
    if (debugarr[index].filter) {
        VQE_FREE(debugarr[index].filter);
    }
    debug_bm[ind].bitmask &= ~bitmask;

    /* If no other element is present for this vty, blow it away */
    if (debug_bm[ind].bitmask == 0) { 
        debug_send_vty_message(-1, MTS_OPC_VSH_DEL_VTY, mod, vty);
        bzero(debug_bm[ind].vtyname, MAXNAME);
    }

#else

    *debugarr[index].var = FALSE;
    if (debugarr[index].filter) {
        VQE_FREE(debugarr[index].filter);
    }
#endif

#ifdef DEBUG_SET
    printf("After resetting flag: debugarr[%d]: var=%d, desc=%s\n",
           index, *debugarr[index].var, debugarr[index].desc);
#endif /* ifdef DEBUG_SET */

    return SUCCESS;
}

/* Get the state of a flag, and the associated filter */
int32_t debug_get_flag_state(debug_item_t *debugarr, int32_t index, 
                             boolean *state,
                             debug_filter_item_t **pp_filter)
{
    *state = *debugarr[index].var;

    if (debugarr[index].filter) {
        if (pp_filter) {
            *pp_filter = debugarr[index].filter;
        }
    } else {
        if (pp_filter) {
            *pp_filter = NULL;
        }
        return DEBUG_FILTER_NOT_SET;
    }

    return SUCCESS;
}


/*
 * To install or change the filter for a debug element. Unsetting a filter
 * should use debug_reset_filter() function.
 */
int32_t
debug_set_filter(debug_item_t *debugarr, int elem, debug_filter_item_t *filter)
{
    /* Return if the filter ptr is NULL */
    if (! filter) {
        return DEBUG_FILTER_NOT_SET;
    }
    
    if (*(debugarr[elem].var) != TRUE) {
        return DEBUG_FILTER_FLAG_FALSE;
    }

    if (filter->type >= DEBUG_MAX_FILTER_TYPE) {
        return DEBUG_FILTER_BAD_TYPE;
    }

    if (debugarr[elem].filter != NULL) {
        if (filter->type != debugarr[elem].filter->type) {
            debugarr[elem].filter->type = filter->type;
            /* Fall thru to set the filter value */
        }
    } else { /* No filter set */
        debugarr[elem].filter = 
            (debug_filter_item_t *) VQE_MALLOC(sizeof(debug_filter_item_t));
        if (debugarr[elem].filter == NULL)
            return DEBUG_FILTER_NO_MEM;
        debugarr[elem].filter->type = filter->type;
    }

    switch (filter->type) {
    case DEBUG_FILTER_TYPE_CHANNEL:
    case DEBUG_FILTER_TYPE_STB_IP:
        debugarr[elem].filter->val = filter->val;
        break;

    case DEBUG_MAX_FILTER_TYPE:
    default:
        return DEBUG_FILTER_BAD_TYPE;
    }


#ifdef DEBUG_SET
    printf("After setting filter: debugarr[%d]: var=%d, desc=%s\n",
           elem, *debugarr[elem].var, debugarr[elem].desc);
    if (debugarr[elem].filter)  {
        printf("After setting filter: debugarr[%d].filter=%d:0x%llx\n",
               elem, debugarr[elem].filter->type, debugarr[elem].filter->val);
    }
#endif /* ifdef DEBUG_SET */

    return SUCCESS;
}

/*
 * To clear the filter for the debug flag.
 */
void
debug_reset_filter(debug_item_t *debugarr, int elem)
{

#ifdef DEBUG_SET
    if (debugarr[elem].filter)  {
        printf("Before resetting filter: debugarr[%d].filter=%d:0x%llx\n",
               elem, debugarr[elem].filter->type, debugarr[elem].filter->val);
    }
#endif /* ifdef DEBUG_SET */

    if (debugarr[elem].filter) {
        VQE_FREE(debugarr[elem].filter);
        debugarr[elem].filter = NULL;
    }

#ifdef DEBUG_SET
    if (! debugarr[elem].filter)  {
        printf("Filter reset successful for debugarr[%d]\n", elem);
    }
#endif /* ifdef DEBUG_SET */
}


/*
 * Check the flag and return TRUE or FALSE based on the flag's value. If
 * the "fhandle" is non-NULL, return TRUE only if the filter also matches
 */
boolean
debug_check_element(debug_item_t *debugarr, int elem, 
                    debug_filter_item_t *filter)
{
    boolean filter_match = TRUE;

    /* 
     * 1. If the filter is not passed or if the filter is not defined
     *         Return TRUE or FALSE based on the debug element's flag setting
     * 
     * 2. If the filter is passed, filter is defined, and filter_type does not 
     *    match
     *         Return FALSE regardless of the debug element's flag setting.
     *
     * 3. If the filter is passed, filter is defined, type matches but value
     *    does not match
     *         Return FALSE regardless of the debug element's flag setting
     *
     * 4. If all the above filter conditions meet
     *        Return TRUE or FALSE based on the debug element's flag setting
     */

    if ((filter != NULL) && (debugarr[elem].filter != NULL)) {

        if (filter->type == debugarr[elem].filter->type) {
            switch (filter->type) {
            case DEBUG_FILTER_TYPE_CHANNEL:
            case DEBUG_FILTER_TYPE_STB_IP:
                if (filter->val != debugarr[elem].filter->val) {
                    filter_match = FALSE;
                }
                break;

            case DEBUG_MAX_FILTER_TYPE:
            default:
                filter_match = FALSE;
            }
        } else { /* Type does not match*/
            filter_match = FALSE;
        }
    } /* If the filter is not passed or the filter is not defined */
    
    if (filter_match && *(debugarr[elem].var)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

void
buginf_func(boolean predicate, char *module, char *func, 
            boolean cond_debug, char *string, ...)
{
    char fmt[MAXLINE], *sep;
    va_list args;

    /*sa_ignore NO_INIT_CHK*/
    va_start(args, string);

    if (! predicate) {
        return;
    }

    /* 
     * Just append the module name in front of the format string. 
     * Syslogd will extract the module name.
     */
    if(func)
        sep = " ";
    else
        sep = "";
    if (cond_debug) {
        snprintf(fmt,sizeof(fmt), "<#%s#>%s%s%s", module, func, sep, string);
    } else {
        snprintf(fmt,sizeof(fmt), "<{%s}>%s%s%s", module, func, sep, string);
    }

    if(VQE_GETENV_SYSCALL("USE_STDERR"))
        VQE_VFPRINTF(stderr, fmt, args);       
    else
        vqes_vsyslog(LOG_DEBUG, fmt, args);  /* The priority matters only when 
                                              * setlogmask() is set.
                                              */
    va_end(args);
}

void
buginf(boolean predicate, char *module, boolean cond_debug, char *string, ...)
{
    char fmt[MAXLINE];
    va_list args;

    if (! predicate) {
        return;
    }

    /* 
     * Just append the module name in front of the format string. 
     * Syslogd will extract the module name.
     */
     
    if (cond_debug) {
        snprintf(fmt,sizeof(fmt), "<#%s#>%s", module, string);
    } else {
        snprintf(fmt,sizeof(fmt), "<{%s}>%s", module, string);
    }

    /*sa_ignore NO_INIT_CHK*/
    va_start(args, string);

    if(VQE_GETENV_SYSCALL("USE_STDERR"))
        VQE_VFPRINTF(stderr, fmt, args);
    else
        vqes_vsyslog(LOG_DEBUG, fmt, args);  /* The priority matters only when 
                                              * setlogmask() is set.
                                              */
    va_end(args);
}


