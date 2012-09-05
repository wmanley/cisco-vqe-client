/******************************************************************** 
 * vqec_syslog_stubs.c
 *
 * Adapted from GNU inetutils 1.4.2 syslog.c to support custom 
 * VQES syslog facilities
 *
 * Copyright (c) 2007-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *********************************************************************/

/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define __VQEC__ 1

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netdb.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include <stdarg.h>

/* VQES specific includes */
#include <log/syslog_facility_num.h>
#include <log/syslog_all_const.h>
#include <log/syslog.h>
#include <../eva/vqec_cli.h>
#include <utils/vqe_port_macros.h>

/* If we want to use default syslog */
#include <sys/syslog.h>

#define BUF_SIZE 1024

/* */
void
vqes_syslog_ul (int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vqes_vsyslog(pri, fmt, ap);
	va_end(ap);
}

/*
 * syslog, vsyslog --
 *	print message on log file; output is intended for syslogd(8).
 */
/* Forward declaration */
void vqes_vsyslog(int, const char*, va_list);
void vqes_vsyslog_cli(int, const char*, va_list); /* for cli-specific printing */

/*
 * The two functions below, vqes_syslog() and vqes_vsyslog() should be called
 * from all threads EXCEPT the CLI thread, as they will not use the cli_print()
 * function, and therefore do not support filtering.
 */

void
vqes_syslog (int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vqes_vsyslog(pri, fmt, ap);
	va_end(ap);
}

/*
 * vqec_remove_trailing_newline
 * this function should be used within the syslog module to
 * remove a trailing newline character from the format
 * string in anticipation of cli_print() adding
 * '\r\n' characters for the telnet session
 * @param[in] format: the format string to be modified
 * @return boolean: TRUE if string was modified,
 *                  FALSE if not
 */
boolean vqec_remove_trailing_newline (char *format)
{
    if (!format || strlen(format) == 0)
        return FALSE;
    int format_len = strlen(format);
    if ((format + format_len - 1) == (strrchr(format, '\n'))) {
        format[format_len - 1] = '\0';
        return TRUE;
    } else {
        return FALSE;
    }
}

void
vqes_vsyslog (pri, fmt, ap)
	int pri;
	register const char *fmt;
	va_list ap;
{
    char tbuf[BUF_SIZE];
    int rv;
    /* get the fd of the telnet session if exist, else just use stderr */
    int cli_fd = vqec_get_cli_fd(vqec_get_cli_def());

    memset(tbuf, 0, sizeof(tbuf));
    rv = vsnprintf(tbuf, sizeof(tbuf)-1, fmt, ap);
    if (rv > 0) {
        if (vqec_remove_trailing_newline(tbuf)) {
            strncat(tbuf, "\r\n", 2);
        }
        if (cli_fd != -1) {
            write(cli_fd, tbuf, strlen(tbuf));
        }
#if HAVE_SYSLOG
        if (pri > LOG_ERR) {
            syslog(LOG_ERR, tbuf, strlen(tbuf));
        } else {
            syslog(pri, tbuf, strlen(tbuf));
        }
#endif
    }
}

/*
 * The two functions below, vqes_syslog_cli() and vqes_vsyslog_cli() are to
 * be used ONLY when we're printing from the CLI thread, i.e. as part of a 
 * result from the execution of a CLI command.  They should be called
 * indirectly only, by using the CONSOLE_PRINTF macro.
 */
void
vqes_syslog_cli (int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vqes_vsyslog_cli(pri, fmt, ap);
	va_end(ap);
}

void
vqes_vsyslog_cli (pri, fmt, ap)
	int pri;
	register const char *fmt;
	va_list ap;
{
    char tbuf[BUF_SIZE];
    int rv;
    struct vqec_cli_def *cli = vqec_get_cli_def();

    rv = vsnprintf(tbuf, sizeof(tbuf)-1, fmt, ap);
    if (rv > 0) {
        if (cli && cli->client) {
            /*sa_ignore {no recourse if this fails} IGNORE_RETURN(1) */
            vqec_remove_trailing_newline(tbuf);
            vqec_cli_print(vqec_get_cli_def(), "%s", tbuf);
        } else {
            printf("%s", tbuf);
        }
    }
}

/* No-op for VQE-C */
void
vqes_openlog (const char *ident, int logstat, int logfac, 
              int fd[], int num_fds)
{
    return;
}

void
vqes_closelog ()
{
    return;
}

/* setlogmask -- set the log mask level */
int
vqes_setlogmask (pmask)
	int pmask;
{
	return (pmask);
}

void vqes_syslog_lock_lock (void)
{
    return;
}

void vqes_syslog_lock_unlock (void)
{
    return;
}

