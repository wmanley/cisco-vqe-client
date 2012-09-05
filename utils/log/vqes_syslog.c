/******************************************************************** 
 * vqes_syslog.c
 *
 * Adapted from GNU inetutils 1.4.2 syslog.c to support custom 
 * VQES syslog facilities
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)syslog.c	8.4 (Berkeley) 3/18/94";
#endif /* LIBC_SCCS and not lint */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE  /* for open_memstream() */

#define IOVEC_COUNT    2
#define DATESTRING_LEN 256

#include <sys/types.h>
#include <sys/socket.h>
//#include <sys/syslog.h>  // Use VQES syslog.h 
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
#include <pthread.h>

#if HAVE_SYSLOG                 /* nominal syslog code block */

/* VQES specific includes */
#include <log/syslog_facility_num.h>
#include <log/syslog_all_const.h>
#include <log/syslog.h>

/* declarations for multiple syslogging FDs */
#define SYSLOG_FDS_MAX 10
static int fds[SYSLOG_FDS_MAX];
static int num_fds;

static int	LogType = SOCK_DGRAM;	/* type of socket connection */
static int	LogFile = -1;		/* fd for log */
static int	connected;		/* have done connect */
static int	LogStat = 0;		/* status bits, set by openlog() */
static const char *LogTag = NULL;	/* string to tag the entry with */
static int	LogFacility = LOG_USER;	/* default facility code */
static int	LogMask = 0xff;		/* mask of priorities to be logged */
extern char	*__progname;		/* Program name, from crt0. */

/* Forward declarations */
static void openlog_internal(const char *, int, int, int[], int);
static void closelog_internal(void);
static void sigpipe_handler(int);

/* Define the lock for syslog routines */
static pthread_mutex_t syslog_lock = PTHREAD_MUTEX_INITIALIZER;

void vqes_syslog_lock_lock (void)
{
    pthread_mutex_lock(&syslog_lock);
}

void vqes_syslog_lock_unlock (void)
{
    pthread_mutex_unlock(&syslog_lock);
}


/*
 * vqes_syslog_cli --
 * this is just a stub for the method used by vqec to print
 * to the CLI.  this is needed here for components outside
 * of vqec that use vqes_syslog_cli() to link properly.
 * FIXME: this should be fixed after the new build structure
 * with a separate vqec build target is rolled out.
 */
void vqes_vsyslog_cli(int, const char*, va_list);

void
vqes_syslog_cli (int pri, const char *fmt, ...)
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

/* Locked version */
void
vqes_syslog (int pri, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vqes_vsyslog(pri, fmt, ap);
    va_end(ap);
}

void
vqes_vsyslog (int pri, register const char *fmt, va_list ap)
{
    struct tm now_tm;
    FILE *f;
    size_t prioff;
    
    time_t now;
    int fd;
    char *buf = 0;
    size_t bufsize = 0;
    size_t  msgoff;
    struct sigaction action, oldaction;
    struct sigaction *oldaction_ptr = NULL;
    
    int sigpipe;
        
#define	INTERNALLOG	LOG_ERR|LOG_CONS|LOG_PERROR|LOG_PID
    /* Check for invalid bits. */
    if (pri & ~(LOG_PRIMASK|LOG_FACMASK)) {
        vqes_syslog(INTERNALLOG,
		    "syslog: unknown facility/priority: %x", pri);
        pri &= LOG_PRIMASK|LOG_FACMASK;
    }
    
    /* Check priority against setlogmask values. */
    if ((LOG_MASK(LOG_PRI(pri)) & LogMask) == 0) 
        return;
    
    /* Set default facility if none specified. */
    if ((pri & LOG_FACMASK) == 0)
        pri |= LogFacility;
    
    /* Build the message in a memory-buffer stream.  */
    f = open_memstream(&buf, &bufsize);
    
    prioff = fprintf(f, "<%d>", pri);
    (void) time(&now);
    
    char datestring[DATESTRING_LEN];
    (void)strftime(datestring, DATESTRING_LEN,
                   "%h %e %T", localtime_r(&now, &now_tm));
    fprintf(f, "%s ", datestring);
    
    msgoff = ftell(f);
    if (LogTag == NULL)
        LogTag = __progname;
    if (LogTag != NULL)
        fputs(LogTag, f);
    if (LogStat & LOG_PID)
        fprintf(f, "[%d]", getpid());
    if (LogTag != NULL)
        (void)putc(':', f), (void)putc(' ', f);
    
    /* We have the header.  Print the user's format into the buffer.  */
    vfprintf(f, fmt, ap);
    
    /* Close the memory stream; this will finalize the data
       into a malloc'd buffer in BUF.  */
    fclose(f);
    
    /* Output to stderr if requested. */
    if (LogStat & LOG_PERROR) {
        struct iovec iov[IOVEC_COUNT];
        register struct iovec *v = iov;
        
        v->iov_base = buf + msgoff;
        v->iov_len = bufsize - msgoff;
        ++v;
        v->iov_base = (char *) "\n";
        v->iov_len = 1;
        (void)writev(STDERR_FILENO, iov, IOVEC_COUNT);
    }
    
    /* Prepare for multiple users.  We have to take care: open and
       write are cancellation points.  */    
    vqes_syslog_lock_lock();

    /* Prepare for a broken connection.  */
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigpipe_handler;
    (void)sigemptyset(&action.sa_mask);
    sigpipe = sigaction(SIGPIPE, &action, &oldaction);
    if (sigpipe == 0)
        oldaction_ptr = &oldaction;
    
    /* Get connected, output the message to the local logger. */
    if (!connected)
        openlog_internal(LogTag, LogStat | LOG_NDELAY, 0, NULL, 0);
    
    /* If we have a SOCK_STREAM connection, also send ASCII NUL as
       a record terminator.  */
    if (LogType == SOCK_STREAM)
        ++bufsize;
    
    /* first, send buf to default fd, LogFile */
    if (send(LogFile, buf, bufsize, 0) < 0) {
        closelog_internal();	/* attempt re-open next time */
        /*
         * Output the message to the console; don't worry about blocking,
         * if console blocks everything will.  Make sure the error reported
         * is the one from the syslogd failure.
         */
        if (LogStat & LOG_CONS &&
            (fd = open(_PATH_CONSOLE, O_WRONLY, 0)) >= 0) {
            /* GNU extension */
            dprintf(fd, "%s\r\n", buf + msgoff);
            
            (void)close(fd);
        }
    }
    
    /* then, send it to the other specified FDs */
    int i;
    for (i = 0 ; i < num_fds ; i++) {
        if (write(fds[i], buf, bufsize) < 0) { 
            closelog_internal();       /* attempt re-open next time */
            /* 
             * Output the message to the console; don't worry about 
             * blocking, if console blocks everything will.  Make sure the 
             * error reported is the one from the syslogd failure.
             */ 
            if (LogStat & LOG_CONS && 
                (fd = open(_PATH_CONSOLE, O_WRONLY, 0)) >= 0) {
                dprintf(fd, "%s\r\n", buf + msgoff);
                (void)close(fd);
            }
        }
    }
    
    if (sigpipe == 0)
        (void)sigaction(SIGPIPE, &oldaction, (struct sigaction *) NULL);

    /* End of critical section.  */
    vqes_syslog_lock_unlock();
    free(buf);
}



/* Un-locked version: the locking should be done in the calling functions */
static void
vqes_vsyslog_ul (int pri, register const char *fmt, va_list ap)
{
    struct tm now_tm;
    FILE *f;
    size_t prioff;
    
    time_t now;
    int fd;
    char *buf = 0;
    size_t bufsize = 0;
    size_t  msgoff;
    struct sigaction action, oldaction;
    struct sigaction *oldaction_ptr = NULL;
    
    int sigpipe;
        
#define	INTERNALLOG	LOG_ERR|LOG_CONS|LOG_PERROR|LOG_PID
    /* Check for invalid bits. */
    if (pri & ~(LOG_PRIMASK|LOG_FACMASK)) {
        vqes_syslog(INTERNALLOG,
		    "syslog: unknown facility/priority: %x", pri);
        pri &= LOG_PRIMASK|LOG_FACMASK;
    }
    
    /* Check priority against setlogmask values. */
    if ((LOG_MASK(LOG_PRI(pri)) & LogMask) == 0) 
        return;
    
    /* Set default facility if none specified. */
    if ((pri & LOG_FACMASK) == 0)
        pri |= LogFacility;
    
    /* Build the message in a memory-buffer stream.  */
    f = open_memstream(&buf, &bufsize);
    
    prioff = fprintf(f, "<%d>", pri);
    (void) time(&now);
    
    char datestring[DATESTRING_LEN];
    (void)strftime(datestring, DATESTRING_LEN,
                   "%h %e %T", localtime_r(&now, &now_tm));
    fprintf(f, "%s ", datestring);
    
    msgoff = ftell(f);
    if (LogTag == NULL)
        LogTag = __progname;
    if (LogTag != NULL)
        fputs(LogTag, f);
    if (LogStat & LOG_PID)
        fprintf(f, "[%d]", getpid());
    if (LogTag != NULL)
        (void)putc(':', f), (void)putc(' ', f);
    
    /* We have the header.  Print the user's format into the buffer.  */
    vfprintf(f, fmt, ap);
    
    /* Close the memory stream; this will finalize the data
       into a malloc'd buffer in BUF.  */
    fclose(f);
    
    /* Output to stderr if requested. */
    if (LogStat & LOG_PERROR) {
        struct iovec iov[IOVEC_COUNT];
        register struct iovec *v = iov;
        
        v->iov_base = buf + msgoff;
        v->iov_len = bufsize - msgoff;
        ++v;
        v->iov_base = (char *) "\n";
        v->iov_len = 1;
        (void)writev(STDERR_FILENO, iov, IOVEC_COUNT);
    }
    
    /* Prepare for a broken connection.  */
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigpipe_handler;
    (void)sigemptyset(&action.sa_mask);
    sigpipe = sigaction(SIGPIPE, &action, &oldaction);
    if (sigpipe == 0)
        oldaction_ptr = &oldaction;
    
    /* Get connected, output the message to the local logger. */
    if (!connected)
        openlog_internal(LogTag, LogStat | LOG_NDELAY, 0, NULL, 0);
    
    /* If we have a SOCK_STREAM connection, also send ASCII NUL as
       a record terminator.  */
    if (LogType == SOCK_STREAM)
        ++bufsize;
    
    /* first, send buf to default fd, LogFile */
    if (send(LogFile, buf, bufsize, 0) < 0) {
        closelog_internal();	/* attempt re-open next time */
        /*
         * Output the message to the console; don't worry about blocking,
         * if console blocks everything will.  Make sure the error reported
         * is the one from the syslogd failure.
         */
        if (LogStat & LOG_CONS &&
            (fd = open(_PATH_CONSOLE, O_WRONLY, 0)) >= 0) {
            /* GNU extension */
            dprintf(fd, "%s\r\n", buf + msgoff);
            
            (void)close(fd);
        }
    }
    
    /* then, send it to the other specified FDs */
    int i;
    for (i = 0 ; i < num_fds ; i++) {
        if (write(fds[i], buf, bufsize) < 0) { 
            closelog_internal();       /* attempt re-open next time */
            /* 
             * Output the message to the console; don't worry about 
             * blocking, if console blocks everything will.  Make sure the 
             * error reported is the one from the syslogd failure.
             */ 
            if (LogStat & LOG_CONS && 
                (fd = open(_PATH_CONSOLE, O_WRONLY, 0)) >= 0) {
                dprintf(fd, "%s\r\n", buf + msgoff);
                (void)close(fd);
            }
        }
    }
    
    if (sigpipe == 0)
        (void)sigaction(SIGPIPE, &oldaction, (struct sigaction *) NULL);

    free(buf);
}

/* Un-locked version: the locking should be done in the calling functions */
void
vqes_syslog_ul (int pri, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vqes_vsyslog_ul(pri, fmt, ap);
    va_end(ap);
}


static struct sockaddr SyslogAddr;	/* AF_UNIX address of local logger */

static void
openlog_internal (const char *ident, int logstat, int logfac, 
                  int lfd[], int lnum_fds)
{
    /* populate fds[] and num_fds with passed-in values */
    if (lnum_fds > SYSLOG_FDS_MAX) {
        lnum_fds = SYSLOG_FDS_MAX;
        printf("max number of FDs for syslog exceeded!!\n");
    }
    num_fds = lnum_fds;
    memcpy(fds, lfd, lnum_fds * sizeof(int));

    if (ident != NULL)
        LogTag = ident;
    LogStat = logstat;
    if (logfac != 0 && (logfac &~ LOG_FACMASK) == 0)
        LogFacility = logfac;

    while (1) {
        if (LogFile == -1) {
            SyslogAddr.sa_family = AF_UNIX;
            (void)strncpy(SyslogAddr.sa_data, _PATH_LOG,
                          sizeof(SyslogAddr.sa_data));
            if (LogStat & LOG_NDELAY) {
                if ((LogFile = socket(AF_UNIX, LogType, 0))
                    == -1)
                    return;
                (void)fcntl(LogFile, F_SETFD, 1);
            }
        }
        if (LogFile != -1 && !connected) {
            if (connect(LogFile, &SyslogAddr, sizeof(SyslogAddr)) == -1) {
                int saved_errno = errno;
                (void)close(LogFile);
                LogFile = -1;
                if (LogType == SOCK_DGRAM
                    && saved_errno == EPROTOTYPE) {
                    /* retry with next SOCK_STREAM: */
                    LogType = SOCK_STREAM;
                    continue;
                }
            } else
                connected = 1;
        }
        break;
    }
}

void
vqes_openlog (const char *ident, int logstat, int logfac, 
              int fd[], int num_fds)
{
    /* Protect against multiple users.  */
    vqes_syslog_lock_lock();
    
    openlog_internal(ident, logstat, logfac, fd, num_fds);

    /* Free the lock.  */
    vqes_syslog_lock_unlock();
}

static void
sigpipe_handler (int signo)
{
    closelog_internal();
}

static void
closelog_internal (void)
{
    (void)close(LogFile);
    LogFile = -1;
    connected = 0;
}

void
vqes_closelog (void)
{
    /* Protect against multiple users.  */
    vqes_syslog_lock_lock();

    closelog_internal();
    memset(fds, 0, SYSLOG_FDS_MAX * sizeof(int));
    num_fds = 0;

    /* Free the lock. */
    vqes_syslog_lock_unlock();
}


/* setlogmask -- set the log mask level */
int
vqes_setlogmask (int pmask)
{
    int omask;

    omask = LogMask;
    if (pmask != 0)
        LogMask = pmask;
    return (omask);
}

#else /* else block for HAVE_SYSLOG, i.e., syslog is stubbed in this case */

/*
 * syslog, vsyslog --
 *	print message on log file; output is intended for syslogd(8).
 */
/* Forward declaration */
void vqes_vsyslog (int, const char*, va_list);

void
vqes_syslog (int pri, const char *fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    vqes_vsyslog(pri, fmt, ap);
    va_end(ap);
}

void
vqes_vsyslog (int pri, register const char *fmt, va_list ap)
{
    vprintf(fmt, ap);
}

void
vqes_openlog (const char *ident, int logstat, int logfac, int fd[], 
              int num_fds)
{
    return;
}

void
vqes_closelog (void)
{
    return;
}

int
vqes_setlogmask (int pmask)
{
    return (pmask);
}

/*
 * vqes_syslog_cli --
 * this is just a stub for the method used by vqec to print
 * to the CLI.  this is needed here for components outside
 * of vqec that use vqes_syslog_cli() to link properly.
 * FIXME: this should be fixed after the new build structure
 * with a separate vqec build target is rolled out.
 */
void vqes_vsyslog_cli(int, const char*, va_list);

void
vqes_syslog_cli(int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vqes_vsyslog(pri, fmt, ap);
	va_end(ap);
}

#endif /* end-of-if for HAVE_SYSLOG */
