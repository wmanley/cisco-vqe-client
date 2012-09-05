/* $Id$
 * $Source$
 *------------------------------------------------------------------
 * rtcp_interval.c
 * 
 * A program to compute the RTCP interval.
 * 
 * Copyright (c) 2006-2008 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 * $Log$
 *------------------------------------------------------------------
 * $Endlog$
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "../../include/sys/event.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include "../../include/utils/vam_types.h"
#include <netinet/ip.h>
#include "sample_rtp_session.h"
#include "sample_rtp_dataplane.h"
#include "sample_rtp_utils.h"


struct intvl_data {
    int      order;
    uint32_t interval;
};

static void print_usage (void) 
{
    printf("Usage: rtcp_interval <options> \n"
           "options:\n"
           "-l <level>         \"media\" or \"session\"\n"
           "-A <value>         b=AS:<value> (in kbps)\n"
           "-R <value>         b=RR:<value> (in bps)\n"
           "-S <value>         b=RS:<value> (in bps)\n"
           "-r <value>         a=fmtp:<pt> per-rcvr-bw=<value> (in bps)\n"
           "-s <value>         a=fmtp:<pt> per-sndr-bw=<value> (in bps)\n"
           "-m <members>       number of members\n"
           "-x <senders>       number of senders\n"
           "-p <avg-pkt-size>  average RTCP packet size (bytes)\n"
           "-w                 if present, we_sent is TRUE\n"
           "-i                 if present, initial is TRUE\n"
           "-n                 no. of times to compute interval\n"
           "-j                 test jittering of interval\n"
           "-v                 verbose output\n"
           "-J <num>           re-seed after <num> calculations\n"
           "-t                 skip duplicate detection\n"
           );
}

static inline
int kbits_to_octets (int kbits) {
    return (kbits * 1000 / 8);
}

static int print_bw (char *s, uint32_t bw)
{
    if (bw != RTCP_BW_UNSPECIFIED) {
        printf("%s=%u ", s, bw);
        return (1);
    } else {
        return (0);
    }
}
            
static void print_params (rtcp_bw_cfg_t *cfg, rtp_session_t *sess)
{
    int num_spec;

    printf("  Per-member bandwidth: ");
    num_spec = 0;
    num_spec += print_bw("per-rcvr", cfg->per_rcvr_bw);
    num_spec += print_bw("per-sndr", cfg->per_sndr_bw);
    if (num_spec == 0) {
        printf("<none>");
    }
    printf("\n");

    printf("  Media level bandwidth: ");
    num_spec = 0;
    num_spec += print_bw("AS", cfg->media_as_bw);
    num_spec += print_bw("RR", cfg->media_rr_bw);
    num_spec += print_bw("RS", cfg->media_rs_bw);
    if (num_spec == 0) {
        printf("<none>");
    }
    printf("\n");

    printf("  Session level bandwidth: ");
    num_spec = 0;
    num_spec += print_bw("AS", cfg->sess_as_bw);
    num_spec += print_bw("RR", cfg->sess_rr_bw);
    num_spec += print_bw("RS", cfg->sess_rs_bw);
    if (num_spec == 0) {
        printf("<none>");
    }
    printf("\n");

    printf("  members=%d senders=%d avg_pkt_size=%d\n",
           sess->rtcp_nmembers, 
           sess->rtcp_nsenders, 
           (uint32_t)(sess->rtcp_stats.avg_pkt_size));
    printf("  we_sent=%s initial=%s\n",
           sess->we_sent ? "TRUE" : "FALSE",
           sess->initial ? "TRUE" : "FALSE");
}

/*
 * print_interval
 */
static void print_interval (uint32_t interval, boolean duplicate,
                            int order1, int order2)
{
    uint32_t seconds = interval / ONESEC;
    uint32_t usecs = interval % ONESEC;
#define DUPBUFSIZ 80
    char buff[DUPBUFSIZ];

    if (duplicate) {
        snprintf(buff, sizeof(buff), "DUPLICATE %d-%d (dist %d)",
                 order1, order2, abs(order1-order2));
    } else {
        buff[0] = '\0';
    }

    printf ("%06d.%06d secs (%d min %d sec)   %s\n",
            seconds, usecs, seconds/60, seconds%60, buff);
}

/*
 * intvlcompar
 *
 * Helper function for qsort of generated intervals.
 */
static int intvlcompar (const void *p1, const void *p2) 
{
    struct intvl_data *i1 = ((struct intvl_data *)p1);
    struct intvl_data *i2 = ((struct intvl_data *)p2);

    if (i1->interval < i2->interval) {
        return (-1);
    } else if (i1->interval > i2->interval) {
        return (1);
    } else {
        return (0);
    }
}

/*
 * dup_detect_print
 *
 * detect duplicates among intervals
 * optionally print intervals
 */ 
static int dup_detect_print (struct intvl_data *intervals, 
                             int count,
                             boolean print,
                             int *sets,
                             int *min_dist,
                             int *avg_dist)
{
    int i;
    int j = count - 1;
    boolean within_duplicate = FALSE;
    boolean duplicate = FALSE;
    int duplicates = 0;
    int duplicate_sets = 0;
    int distance = 0;
    int dist_count = 0;
    int min_distance = count;
    uint64_t avg_distance = 0;

    for (i = 0; i < j; i++) {
        if (intervals[i].interval == intervals[i+1].interval) {
            within_duplicate = duplicate = TRUE;
            duplicates++;
            distance = abs(intervals[i].order - intervals[i+1].order);
            if (distance < min_distance) {
                min_distance = distance;
            }
            avg_distance += distance;
            dist_count++;
        } else if (within_duplicate) {
            duplicates++;
            within_duplicate = FALSE;
            duplicate_sets++;
        }
        if (print) {
            print_interval(intervals[i].interval, duplicate,
                           intervals[i].order,
                           intervals[i+1].order);
            if (!within_duplicate) {
                duplicate = FALSE;
            }
        }
    }
    if (within_duplicate) {
        duplicates++;
        duplicate_sets++;
    }
    if (print) {
        print_interval(intervals[i].interval, duplicate, 0, 0);
    }

    if (sets) {
        *sets = duplicate_sets;
    }
    if (min_dist) {
        *min_dist = min_distance;
    }
    if (avg_dist) {
        *avg_dist = dist_count > 0 ? (uint32_t)(avg_distance/dist_count) : 0;
    } 
    return (duplicates);
}


int main (int argc, char **argv)
{
    int c;

    rtcp_bw_cfg_t bw_cfg;
    rtp_session_t sess;

    uint32_t avg_pkt_size = 100; 
    uint32_t members = 2;
    uint32_t senders = 1;
    boolean we_sent = FALSE;
    boolean initial = FALSE;
    boolean jitter = FALSE;
    boolean verbose = FALSE;
    boolean media_lvl = TRUE;
    int repeat_count = 1;
    int i;
    int duplicates = 0;
    int duplicate_sets = 0;
    int min_distance = 0;
    int avg_distance = 0;
    int reseed = LONG_MAX;
    boolean dup_detect = TRUE;

    struct intvl_data *intervals = NULL;  /* ptr to array of intervals:
                                             malloc'ed based on -n */

    rtcp_init_bw_cfg(&bw_cfg);
    memset(&sess, 0, sizeof(sess));

    while ((c = getopt(argc, argv, "l:A:R:S:r:s:p:m:x:vwin:jJ:t")) != EOF) {
        switch (c) {
        case 'l':
            if (strncmp(optarg, "media", strlen(optarg)) == 0) {
                media_lvl = TRUE;
            } else if (strncmp(optarg, "session", strlen(optarg)) == 0) {
                media_lvl = FALSE;
            } else {
                print_usage();
                return (-1);
            }
            break;
        case 'A':
            if (media_lvl) {
                bw_cfg.media_as_bw = atoi(optarg);
            } else {
                bw_cfg.sess_as_bw = atoi(optarg);
            }
            break;
        case 'R':
            if (media_lvl) {
                bw_cfg.media_rr_bw = atoi(optarg);
            } else {
                bw_cfg.sess_rr_bw = atoi(optarg);
            }
            break;
        case 'S':
            if (media_lvl) {
                bw_cfg.media_rs_bw = atoi(optarg);
            } else {
                bw_cfg.sess_rs_bw = atoi(optarg);
            }
            break;
        case 'r':
            bw_cfg.per_rcvr_bw = atoi(optarg);
            break;
        case 's':
            bw_cfg.per_sndr_bw = atoi(optarg);
            break;
        case 'p':
            avg_pkt_size = atoi(optarg);
            break;
        case 'm':
            members = atoi(optarg);
            break;
        case 'x':
            senders = atoi(optarg);
            break;
        case 'v':
            verbose = TRUE;
            break;
        case 'w':
            we_sent = TRUE;
            break;
        case 'i':
            initial = TRUE;
            break;
        case 'n':
            repeat_count = atoi(optarg);
            if (repeat_count < 0 || repeat_count > LONG_MAX) {
                print_usage();
                return (-1);
            }
            break;
        case 'j':
            jitter = TRUE;
            break;
        case 'J':
            reseed = atoi(optarg);
            if (reseed < 0 || reseed > LONG_MAX) {
                print_usage();
                return (-1);
            }
            break;
        case 't':
            dup_detect = FALSE;
            break;
        default:
            print_usage();
            return (-1);
        }
    }

    intervals = malloc(repeat_count * sizeof(struct intvl_data));
    if (!intervals) {
        fprintf(stderr, "Malloc failure\n");
        return (-1);
    }

    rtcp_set_bw_info(&bw_cfg, &sess.rtcp_bw);
    sess.rtcp_stats.avg_pkt_size = (double)avg_pkt_size;
    sess.rtcp_stats.avg_pkt_size_sent = (double)avg_pkt_size;
    sess.rtcp_nmembers = members;
    sess.rtcp_nsenders = senders;
    sess.we_sent = we_sent;
    sess.initial = initial;
    
    for (i = 0; i < repeat_count; i++) {
        if (i % reseed == 0) { 
            /* 
             * normally, this should be done via rtcp_jitter_init, 
             * but since we may do this rapidly, we need to change
             * the type value for rtp_random32()
             */
            sess.intvl_jitter = rtp_random32(i);
        }
        intervals[i].order = i;
        intervals[i].interval = 
            rtcp_report_interval_base(&sess, sess.we_sent, jitter);
        if (repeat_count <= 10) {
            print_interval(intervals[i].interval, FALSE, 0, 0);
            if (verbose) {
                print_params(&bw_cfg, &sess);
            }
        }
    }

    if (dup_detect && (repeat_count > 1)) {
        qsort(intervals, repeat_count, sizeof(struct intvl_data), intvlcompar);
        duplicates = dup_detect_print(intervals, repeat_count,
                                      verbose, /* print intervals (or not) */
                                      &duplicate_sets,
                                      &min_distance,
                                      &avg_distance);
        if (duplicates) {
            fprintf(stderr, 
                    "DUPLICATES detected: %d in %d sets "
                    "(dist: min %d, avg %d)\n", 
                    duplicates, duplicate_sets, 
                    min_distance, avg_distance);
        }
    }
        
    free(intervals);
    return(0);
}
