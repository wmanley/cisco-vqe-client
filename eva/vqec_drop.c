/*
 * vqec_drop.c 
 * routines for forcing dropped packets.
 *
 * Copyright (c) 2006-2009 by Cisco Systems, Inc.
 * All rights reserved.
 */
#include "vqec_drop.h"
#include "vqec_assert_macros.h"
#include "vqec_error.h"
#include "vqec_dp_api.h"
#include "vqec_syscfg.h"

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif


void vqec_set_drop_interval(vqec_dp_input_stream_type_t session,
                            uint32_t continuous_drops, uint32_t drop_interval) 
{
    vqec_dp_drop_sim_state_t state;
    vqec_dp_error_t err = VQEC_DP_ERR_OK;
    
    memset(&state, 0, sizeof(state));
    err = vqec_dp_get_drop_params(session, &state);
    if (err != VQEC_DP_ERR_OK) {
        goto done;
    }

    state.desc.num_cont_drops = continuous_drops;
    state.desc.interval_span = drop_interval;
    err = vqec_dp_set_drop_params(session, &state);
done:
    return;
}

void vqec_set_drop_ratio (vqec_dp_input_stream_type_t session,
                          uint32_t drop_ratio)
{
    vqec_dp_drop_sim_state_t state;
    vqec_dp_error_t err;

    memset(&state, 0, sizeof(state));
    err = vqec_dp_get_drop_params(session, &state);
    if (err != VQEC_DP_ERR_OK) {
        goto done;
    }

    state.desc.drop_percent = drop_ratio;
    err = vqec_dp_set_drop_params(session, &state);
done:
    return;
}

void vqec_set_drop_enable(vqec_dp_input_stream_type_t session,
                          boolean enable)
{
    vqec_dp_drop_sim_state_t state;
    vqec_dp_error_t err;

    memset(&state, 0, sizeof(state));
    err = vqec_dp_get_drop_params(session, &state);
    if (err != VQEC_DP_ERR_OK) {
        goto done;
    }

    state.en = enable;
    err = vqec_dp_set_drop_params(session, &state);
done:
    return;
}

void vqec_drop_dump_stream (vqec_dp_input_stream_type_t session)
{
    vqec_dp_drop_sim_state_t state;
    char *session_str;
    vqec_dp_error_t err;

#define VQEC_DROP_SESSION_STR_MAXLEN 7
    switch (session) {
    case VQEC_DP_INPUT_STREAM_TYPE_PRIMARY:
        session_str = "primary";
        break;
    case VQEC_DP_INPUT_STREAM_TYPE_REPAIR:
        session_str = "repair";
        break;
    default:
        goto done;
    }

    memset(&state, 0, sizeof(state));
    err = vqec_dp_get_drop_params(session, &state);
    if (err != VQEC_DP_ERR_OK) {
        goto done;
    }

    if (state.en) {
        CONSOLE_PRINTF("%s stream dropping:%*s   enabled (using %s)\n",
                       session_str,
                       VQEC_DROP_SESSION_STR_MAXLEN-strlen(session_str)+1, " ",
                       state.desc.interval_span ? "interval" : "percentage");
    } else {
        CONSOLE_PRINTF("%s stream dropping:%*s   disabled\n",
                       session_str,
                       VQEC_DROP_SESSION_STR_MAXLEN-strlen(session_str)+1,
                       " ");
    }
    CONSOLE_PRINTF(" dropping:                  %d\n", 
                   state.desc.num_cont_drops);
    CONSOLE_PRINTF(" interval:                  %d\n", 
                   state.desc.interval_span);
    CONSOLE_PRINTF(" percentage:                %d%%\n", 
                   state.desc.drop_percent);
done:
    return;
}    


void vqec_drop_dump (void) 
{
    vqec_drop_dump_stream(VQEC_DP_INPUT_STREAM_TYPE_PRIMARY);
    vqec_drop_dump_stream(VQEC_DP_INPUT_STREAM_TYPE_REPAIR);
}


/* added for use by RCC */

#if HAVE_FCC
static boolean g_vqec_rcc;
static boolean g_vqec_rcc_set = FALSE;

/**
 * Retrieves the global settting of RCC, which is based on system
 * configuration but may be overriden by the demo CLI knob (if set).
 *
 * param[out] demo_set        - TRUE:  the demo CLI knob is set
 *                              FALSE: the demo CLI knob is unset
 * param[out] boolean         - TRUE:  RCC is globally enabled
 *                              FALSE: RCC is globally disabled
 */
boolean vqec_get_rcc (boolean *demo_set)
 {
    boolean value;

    if (demo_set) {
        *demo_set = g_vqec_rcc_set;
    }
    if (g_vqec_rcc_set) {
        value = g_vqec_rcc;
    } else {
        value = vqec_syscfg_get_ptr()->rcc_enable;
    }
    return (value);
}

void vqec_set_rcc (boolean b) {
    if (g_vqec_rcc != b) {
        g_vqec_rcc = b;
    }
    g_vqec_rcc_set = TRUE;
}

/*
 * vqec_rcc_show()
 *
 * Displays information about rcc.
 *
 * Params:
 *   @param[in]  cli  CLI context on which output should be displayed
 */
void
vqec_rcc_show (struct vqec_cli_def *cli)
{
    rel_time_t delay;
    boolean rcc_enable, demo_set;

    VQEC_ASSERT(cli);

    rcc_enable = vqec_get_rcc(&demo_set);
    vqec_cli_print(cli, "rcc:                  %s%s",
                   rcc_enable ? "enabled" : "disabled", 
                   demo_set ? " (set by CLI)" : "");
    if (VQEC_DP_ERR_OK == vqec_dp_get_app_cpy_delay(&delay)) {
        vqec_cli_print(cli, "app delay:            %lld", 
                       TIME_GET_R(msec, delay));
    }

}


/* add support for fast fill */

static boolean g_vqec_fast_fill;
static boolean g_vqec_fast_fill_set = FALSE;

/**
 * Retrieves the global settting of fast fill, which is based on system
 * configuration but may be overriden by the demo CLI knob (if set).
 *
 * param[out] demo_set        - TRUE:  the demo CLI knob is set
 *                              FALSE: the demo CLI knob is unset
 * param[out] boolean         - TRUE:  fast fill is globally enabled
 *                              FALSE: fast fill is globally disabled
 */
boolean vqec_get_fast_fill (boolean *demo_set) {
    boolean value;

    if (demo_set) {
        *demo_set = g_vqec_fast_fill_set;
    }
    if (g_vqec_fast_fill_set) {
        value = g_vqec_fast_fill;
    } else {
        value = vqec_syscfg_get_ptr()->fastfill_enable;
    }
    return (value);
}

void vqec_set_fast_fill (boolean b) {
    if (g_vqec_fast_fill != b) {
        g_vqec_fast_fill = b;
    }
    g_vqec_fast_fill_set = TRUE;
}

/*
 * vqec_fast_fill_show()
 *
 * Displays information about fast fill.
 *
 * Params:
 *   @param[in]  cli  CLI context on which output should be displayed
 */
void
vqec_fast_fill_show (struct vqec_cli_def *cli)
{
    boolean fast_fill_enable, demo_set;

    VQEC_ASSERT(cli);

    fast_fill_enable = vqec_get_fast_fill(&demo_set);
    vqec_cli_print(cli, "fast fill:            %s%s",
                   fast_fill_enable ? "enabled" : "disabled",
                   demo_set ? " (demo)" : "");
}


#endif  /* HAVE_FCC */
