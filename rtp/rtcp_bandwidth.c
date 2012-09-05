/**------------------------------------------------------------------------
 * @brief
 * Specification of RTCP bandwidth and calculation of the RTCP interval.
 *
 * @file
 * rtcp_bandwidth.c
 *
 * March 2007, Mike Lague.
 *
 * Copyright (c) 2007-2008 by cisco Systems, Inc.
 * All rights reserved.
 *-------------------------------------------------------------------------
 */

#include "rtcp_bandwidth.h"
#include <stdlib.h>

/*
 * rtcp_set_role_bw_info
 *
 * Set up per-session RTCP bandwidth information for a role
 * (sender or receiver), from configured RTCP bandwidth parameters.
 *
 * The rules for computing "total role bandwidth" implement part of
 * the precedence rules in Section 4. of RFC 3556. 
 */

static void rtcp_set_role_bw_info (rtcp_bw_t      per_member_bw,
                                   rtcp_bw_t      role_bw,
                                   rtcp_bw_t      other_role_bw,
                                   rtcp_bw_kbps_t sess_bw,
                                   double         rtcp_dflt_role_pct,
                                   rtcp_bw_role_t *role)
{
    double tot_rtcp_bw = 0.;
    double tot_role_bw = 0.;

    role->cfg_per_member_bw = per_member_bw;
    /* 
     * reported per-member bandwidth is specified (if at all)
     * via the RTCP Bandwidth Indication subreport of the RSI message,
     * and not through configuration.
     */
    role->rpt_per_member_bw = RTCP_BW_UNSPECIFIED;

    if (role_bw != RTCP_BW_UNSPECIFIED) {
        /*
         * per-role total bandwidth (RR or RS) is specified:
         * just use it.
         */
        role->tot_role_bw = role_bw;
    } else if (sess_bw != RTCP_BW_UNSPECIFIED) {
        /* 
         * Ssession bandwidth (AS) is specified:
         * and per-role bandwidth (RR or RS) is unspecified.
         * 
         * 1. get default total RTCP bandwidth from session bandwidth, 
         *    first converting session bandwidth from kbps to bps.
         * 2. if the other role's bw (RS or RR) is specified
         *        role_bw = total RTCP bandwidth - other role's bandwidth
         *        (but not less than zero)
         *    else 
         *        role_bw = per-role dflt pct of sess bw
         * 3. role bw is no greater than RTCP_MAX_BW; 
         *    round up to nearest bps.
         */
        tot_rtcp_bw = (double)(sess_bw) * 1000. * D_RTCP_DFLT_BW_PCT;
        if (other_role_bw != RTCP_BW_UNSPECIFIED) {
            tot_role_bw = tot_rtcp_bw - (double)(other_role_bw);
            if (tot_role_bw < 0.) {
                tot_role_bw = 0.;
            }
        } else {
            tot_role_bw = tot_rtcp_bw * rtcp_dflt_role_pct;
        }
        role->tot_role_bw = tot_role_bw > D_RTCP_MAX_BW ?
            RTCP_MAX_BW : (uint32_t)(tot_role_bw + 0.5);
    } else {
        /* AS and per-role bandwidth (RR or RS) are both unspecified */
        role->tot_role_bw = RTCP_BW_UNSPECIFIED;
    }
}    
                                   
/*
 * rtcp_set_bw_info
 *
 * Set up per-session RTCP bandwidth information, 
 * from configured RTCP bandwidth parameters.
 * 
 */
void rtcp_set_bw_info (rtcp_bw_cfg_t *cfg,
                       rtcp_bw_info_t *info)
{
    rtcp_bw_t rr_bw; 
    rtcp_bw_t rs_bw;
    rtcp_bw_kbps_t as_bw;

    /*
     * according to RFC 3556, the media level version of a parameter 
     * (RR/RS/AS), if specified, takes precedence over the
     * session level version of the parameter; it is possible that
     * the parameter has not been specified at either level.
     */
    rr_bw = cfg->media_rr_bw != RTCP_BW_UNSPECIFIED ?
        cfg->media_rr_bw : cfg->sess_rr_bw;
    rs_bw = cfg->media_rs_bw != RTCP_BW_UNSPECIFIED ?
        cfg->media_rs_bw : cfg->sess_rs_bw;
    as_bw = cfg->media_as_bw != RTCP_BW_UNSPECIFIED ?
        cfg->media_as_bw : cfg->sess_as_bw;

    rtcp_set_role_bw_info(cfg->per_rcvr_bw,
                          rr_bw,  /* this role's bw */
                          rs_bw,  /* the other role's bw */
                          as_bw,  /* session bw */
                          D_RTCP_DFLT_RCVR_BW_PCT, /* dflt % of RTCP bw,
                                                      for receivers */
                          &(info->rcvr));

    rtcp_set_role_bw_info(cfg->per_sndr_bw,
                          rs_bw,  /* this role's bw */
                          rr_bw,  /* the other role's bw */
                          as_bw,  /* session bw */
                          D_RTCP_DFLT_SNDR_BW_PCT, /* dflt % of RTCP bw,
                                                      for senders */
                          &(info->sndr));
}

/*
 * rtcp_get_intvl_calc_params
 *
 * Retrieve parameters for RTCP interval calculation,
 * from RTCP bandwidth information and other per-session information.
 */
void rtcp_get_intvl_calc_params (rtcp_bw_info_t *bw_info,
                                 rtcp_intvl_calc_sess_info_t *sess_info,
                                 rtcp_intvl_calc_params_t *params)
{
    boolean sender = sess_info->we_sent;
    rtcp_bw_role_t *this_role;
    rtcp_bw_role_t *other_role;
    double per_member_bw=0.0;
    boolean per_member_bw_specified;
    rtcp_bw_t role_bw;
    rtcp_bw_t other_role_bw;
    double    tot_role_bw;

    if (sender) {
        this_role = &bw_info->sndr;
        other_role = &bw_info->rcvr;
    } else {
        this_role = &bw_info->rcvr;
        other_role = &bw_info->sndr;
    }

    /* 
     * configured per-member bandwidth takes precedence over
     * reported per-member bandwidth (received via the RTCP
     * Bandwidth Indication subreport of the RSI message)
     */
    if (this_role->cfg_per_member_bw != RTCP_BW_UNSPECIFIED) {
        per_member_bw = (double)(this_role->cfg_per_member_bw);
        /* 
         * Configured per-member bandwidth is now defined as the amount
         * granted to a receiver if the avg pkt size is the default;
         * if the actual avg pkt size differs from the default, 
         * we scale the per-member bandwidth proportionally.
         */
        per_member_bw *= (sess_info->rtcp_avg_size_sent 
                          / D_RTCP_DFLT_AVG_PKT_SIZE);
        per_member_bw_specified = TRUE;
    } else if (this_role->rpt_per_member_bw != RTCP_BW_UNSPECIFIED) {
        /* convert from 16:16 fixed point value in kbps to bps */
        per_member_bw = rtcp_bw_bi_to_dbps(this_role->rpt_per_member_bw);
        per_member_bw_specified = TRUE;
    } else {
        per_member_bw_specified = FALSE;
    }
        
    if (per_member_bw_specified) {
        /*
         * Per-member bandwidth is specified: use it, 
         * and set other parameters for interval calculation accordingly;
         * for avg pkt size, consider only RTCP compound msgs sent 
         * by this participant.
         */
        params->members = 1;
        params->senders = sender ? 1 : 0;
        /* convert bps to bytes/sec */        
        params->rtcp_bw = per_member_bw / 8.; 
        params->rtcp_sender_bw_fraction = sender ? 1. : 0. ;
        params->rtcp_avg_size = sess_info->rtcp_avg_size_sent;
    } else {
        /*
         * If per-member bandwidth is not available, 
         * use configured RTCP bandwidth (RR + RS),
         * where RR and/or RS may take defaults derived according to RFC 3556.
         * For "sender fraction", we use RS/RS+RR, or zero, if RR+RS == 0.
         */
        params->members = sess_info->members;
        params->senders = sess_info->senders;
        role_bw = this_role->tot_role_bw != RTCP_BW_UNSPECIFIED ?
            this_role->tot_role_bw : 0;
        other_role_bw = other_role->tot_role_bw != RTCP_BW_UNSPECIFIED ?
            other_role->tot_role_bw : 0;
        tot_role_bw = (double)role_bw + (double)other_role_bw;
        /* convert bps to bytes/sec */
        params->rtcp_bw = tot_role_bw / 8.;
        /* sender bw fraction = RS/RR+RS (or 0, if RR+RS == 0) */
        params->rtcp_sender_bw_fraction = (role_bw + other_role_bw) ?
                   (double)(sender ? role_bw : other_role_bw) / tot_role_bw 
                   : 0. ;
        params->rtcp_avg_size = sess_info->rtcp_avg_size;
    }
    params->we_sent = sender;
    params->initial = sess_info->initial;

    return;
}

/*
 * rtcp_td_interval
 *
 * Compute the RTCP "deterministic" interval (Td)
 * from the given input parameters.
 *
 * This is similar to the rtcp_interval() function given in 
 * Appendix A.7 of RFC 3550, but with these differences:
 * - This function does not "randomize" the interval, but
 *   but instead computes the "deterministic" interval:
 *   to get the randomized interval, apply rtcp_jitter_interval
 *   to the output of this function, e.g., 
 *   use rtcp_jitter_interval(rtcp_td_interval(...)).
 * - This function specifies rtcp_sender_bw_fraction as an input
 *   parameter, instead of as a constant (RTCP_SENDER_BW_FRACTION). 
 *   This is so that RFC 3556, which allows for separate specification 
 *   of receiver and sender RTCP bandwidth, may be used with the 
 *   RFC 3550 algorithm, as suggested in the commments of rtcp_interval().
 * - This function checks for rtcp_bw of zero, which is possible 
 *   when using RFC 3556; in this case the minimum RTCP interval is returned.
 * - This function conforms to local coding conventions for indentation
 *   and variable initialization.
 * - This function optionally returns the actual RTCP bandwidth,
 *   and the actual number of members, used in the calculation.
 * - for clarity, "we_sent" and "initial" are typed as boolean, not int.
 *
 * Returns:
 *   RTCP interval, in seconds.
 *   (optionally, i.e., if pointers are non-NULL), this returns:
 *   . RTCP bandwidth actually used in the calculation (in bytes/sec)
 *   . Number of members actually used in the calculation.
 */
double rtcp_td_interval (int members,
                         int senders,
                         double rtcp_bw,
                         double rtcp_sender_bw_fraction,
                         boolean we_sent,
                         double rtcp_avg_size,
                         boolean initial,
                         double *act_rtcp_bw,
                         int *act_members)
{
    /*
     * Minimum average time between RTCP packets from this site (in
     * seconds).  This time prevents the reports from `clumping' when
     * sessions are small and the law of large numbers isn't helping
     * to smooth out the traffic.  It also keeps the report interval
     * from becoming ridiculously small during transient outages like
     * a network partition.
     */
    double const _RTCP_MIN_TIME = 5.;
    /*
     * [rtcp_sender_bw_fraction is the]
     * "fraction of the RTCP bandwidth to be shared among active
     * senders.  ([The default value of] this fraction was chosen 
     * so that in a typical session with one or two active senders, 
     * the computed report time would be roughly equal to the 
     * minimum report time so that we don't unnecessarily 
     * slow down receiver reports.)  The receiver fraction 
     * must be 1 - the sender fraction."
     */
    double rtcp_rcvr_bw_fraction = 1. - rtcp_sender_bw_fraction;

    double t = 0.;              /* interval */
    double rtcp_min_time = _RTCP_MIN_TIME;
    int n = 0;                  /* no. of members for computation */

    /*
     * Very first call at application start-up uses half the min
     * delay for quicker notification while still allowing some time
     * before reporting for randomization and to learn about other
     * sources so the report interval will converge to the correct
     * interval more quickly.
     */

    if (initial) {
        rtcp_min_time /= 2;
    }
    /*
     * Dedicate a fraction of the RTCP bandwidth to senders unless
     * the number of senders is large enough that their share is
     * more than that fraction.
     */
    n = members;
    if (senders <= members * rtcp_sender_bw_fraction) {
        if (we_sent) {
            rtcp_bw *= rtcp_sender_bw_fraction;
            n = senders;
        } else {
            rtcp_bw *= rtcp_rcvr_bw_fraction;
            n -= senders;
        }
    }

    /*
     * The effective number of sites times the average packet size is
     * the total number of octets sent when each site sends a report.
     * Dividing this by the effective bandwidth gives the time
     * interval over which those packets must be sent in order to
     * meet the bandwidth target, with a minimum enforced.  In that
     * time interval we send one report so this time is also our
     * average time between reports.
     */
    if (rtcp_bw > 0) {
        t = rtcp_avg_size * n / rtcp_bw;
    }
    if (t < rtcp_min_time) {
        t = rtcp_min_time;
    }
    if (act_rtcp_bw) {
        *act_rtcp_bw = rtcp_bw;
    }
    if (act_members) {
        *act_members = n;
    }

    return t;
}

/*
 * rtcp_jitter_init
 *
 * Initialize the randomization function used by rtcp_jitter_interval.
 * Typically, this consists of seeding the random number generator.
 */
void rtcp_jitter_init (rtcp_jitter_data_t *jdata) 
{
    /* 
     * seed the random number generator with 
     * the output of an MD5-based random number generator 
     */
    *jdata = rtp_random32(RANDOM_GENERIC_TYPE);
}
 
/*
 * rtcp_jitter_interval
 *
 * Compute the randomized (jittered) RTCP interval (T),
 * given the deterministic RTCP interval (Td).
 *
 * This is very similar to the randomization done in 
 * rtcp_interval(), given in Appendix A.7 of RFC 3550,
 * except that rand_r(), seeded with the output of an 
 * MD5-based random number generator, is used
 * instead of drand48().
 */
double rtcp_jitter_interval (double td_interval, 
                             rtcp_jitter_data_t *jdata)
{
    double t = td_interval ;
    double random_fraction = 0.;
    /* 
     * To compensate for "timer reconsideration" converging to a
     * value below the intended average.
     */
    double const COMPENSATION = 2.71828 - 1.5;

    /*
     * To avoid traffic bursts from unintended synchronization with
     * other sites, we then pick our actual next report interval as a
     * random number uniformly distributed between 0.5*t and 1.5*t.
     */
    random_fraction = (double)rand_r(jdata) / (double)(RAND_MAX + 1.0);
    t = t * (random_fraction + 0.5);
    t = t / COMPENSATION;

    return (t);
}
