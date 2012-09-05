/*
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */

#ifdef HAVE_ELOG

#include <utils/vam_types.h>
#include "vqec_assert_macros.h"

#ifndef HAVE_STRLFUNCS
#include <utils/strl.h>
#endif

static boolean monitor_elog = FALSE;

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>                   /* for htons, htonl, ntohs ntohl */
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <stdarg.h>
#include <time.h>
#include "elog.h"
#include "elog_private.h"
#include "cpel.h"

#include "test_cpel.c"
#include "vqec_elog.h"
#include "vqe_port_macros.h"

elog_shmem_t *elog_shmem;

int ticks_per_us_set;
unsigned long ticks_per_us=1;             /* just in case... */

int copy_events(FILE *ofp)
{
    cpel_section_header_t sh;
    event_section_header_t eh;
    unsigned long number_of_events;
    int i;
    event_entry_t e;
    unsigned long thistime32, prevtime32;
    unsigned long long thistime;
    unsigned long long wraps;
    int index, start_index;
    elog_shmem_t *sp = elog_get_shmem();

    if (sp->flags & ELOG_FLAG_WRAPPED){
        number_of_events = sp->nevents;
        start_index = sp->curindex;
    } else {
        if (sp->curindex == 0) {
            CONSOLE_PRINTF("No data collected, need to turn collection on?\n");
            return (-1);
        }
        number_of_events = sp->curindex;
        start_index = 0;
    }

    if (fwrite(cpel_bndl, sizeof(cpel_bndl), 1, ofp) != 1) {
        CONSOLE_PRINTF("Error writing canned cpel sections\n");
        return(1);
    }

    sh.section_type = ntohl(CPEL_SECTION_EVENT);
    sh.data_length = ntohl(number_of_events * sizeof(e) +
                           sizeof(event_section_header_t));

    if (fwrite(&sh, sizeof(sh), 1, ofp) != 1)
        return(1);
    
    memset(&eh, 0, sizeof(eh));
    strlcpy(eh.string_table_name, "FileStrtab", 11);
    eh.number_of_events = ntohl(number_of_events);
    eh.clock_ticks_per_second = ntohl(ticks_per_us*1000000);
    
    if (fwrite(&eh, sizeof(eh), 1, ofp) != 1)
        return(1);

    prevtime32 = sp->events[start_index].time[0];
    wraps = 0;
    
    for (i = 0; i < number_of_events; i++) {
        index = (i + start_index) % sp->nevents;
        thistime32 = sp->events[index].time[0];
        if (thistime32 < prevtime32) {
            wraps += 0x100000000ULL;
            prevtime32 = thistime32;
        }
        thistime = (unsigned long long) thistime32 + wraps;
        e.time[0] = ntohl((unsigned long)(thistime>>32));
        e.time[1] = ntohl((unsigned long)(thistime & 0xFFFFFFFF));
        e.track = ntohl(sp->events[index].pid);
        e.event_code = ntohl(sp->events[index].code);
        e.event_datum = ntohl(sp->events[index].datum);
        
        if (fwrite(&e, sizeof(e), 1, ofp) != 1)
            return(1);
    }
    return(0);
}

void fatal(char *s)
{
    CONSOLE_PRINTF(s);
    exit(1);
}

void clock_calibrate(void)
{
    struct timespec ts, tsrem;
    unsigned long ticks_before, ticks_after;
    
    ts.tv_sec = 0;
    ts.tv_nsec = 500000000LL; /* 0.1 sec */

    
    ticks_before = arch_get_timestamp();

    while (nanosleep(&ts, &tsrem) < 0) {
        ts = tsrem;
    }

    ticks_after = arch_get_timestamp();

    ticks_per_us = (ticks_after - ticks_before) / 500000;

    CONSOLE_PRINTF("Calibrated: %ld ticks_per_microsecond\n", ticks_per_us);
}

/*
 * status_report
 */
void status_report(void)
{
    elog_shmem_t *sp = elog_get_shmem();
    char *ts;

    if (elog_not_mapped()) {
        CONSOLE_PRINTF("Region not mapped yet?\n");
    }

    CONSOLE_PRINTF("%d events, %d clients, capture is %s, current index %d\n",
           sp->nevents, sp->num_clients, sp->enable ? "enabled" : "disabled", 
           sp->curindex);
    
    CONSOLE_PRINTF("flags:");
    if (sp->flags & ELOG_FLAG_SHARED) {
        CONSOLE_PRINTF(" shared");
    }
    if (sp->flags & ELOG_FLAG_RESIZE) {
        CONSOLE_PRINTF(" resize");
    }
    if (sp->flags & ELOG_FLAG_WRAPPED) {
        CONSOLE_PRINTF(" wrapped");
    }
    if (!sp->flags) {
        CONSOLE_PRINTF(" (none)");
    }
    CONSOLE_PRINTF("\n");

    switch(sp->trigger_state) {
    case ELOG_TRIGGER_STATE_NONE:
        ts = "none";
        break;
    case ELOG_TRIGGER_STATE_WAIT:
        ts = "wait";
        break;
    case ELOG_TRIGGER_STATE_RUN:
        ts = "run";
        break;
    case ELOG_TRIGGER_STATE_DONE:
        ts = "done";
        break;
    default:
        ts = "BUG!";
        break;
    }
    CONSOLE_PRINTF("trigger state: %s, trigger index %d, trigger position %d%%\n",
           ts, sp->trigger_index, sp->trigger_position);
    CONSOLE_PRINTF("region name %s\n", sp->name);
}

void vqec_monitor_elog_set (boolean b) {
    clock_calibrate();
    monitor_elog = b;
    elog_on_or_off(b);
}

boolean vqec_monitor_elog_get () {
    return monitor_elog;
}

void vqec_monitor_elog_reset () {
    elog_reset();
}

void vqec_monitor_elog_dump (char * outputfile) {
    FILE *ofp;

    VQEC_ASSERT(outputfile);

    clock_calibrate();

    ofp = fopen (outputfile, "w");

    if (ofp == 0) {
        CONSOLE_PRINTF("Couldn't create %s\n", outputfile);
        return;
    }

    elog_on_or_off(0);
    if (!copy_events(ofp)) {
        CONSOLE_PRINTF("Writing events to file failed!");
    }

    fclose(ofp);
}

#endif /* HAVE_ELOG */
