/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Kernel syslog.
 *
 * Documents: 
 *
 *****************************************************************************/
#include <utils/vam_types.h>
#include <log/syslog_facility_num.h>
#include <log/syslog_all_const.h>
#include <log/syslog.h>
#include <linux/version.h>

    
/**---------------------------------------------------------------------------
 * CONTROLLING printk [log] BEHAVIOR
 *
 * proc/sys/kernel/printk [from Linux proc man page]
 * The four values in this file are:
 *   -- console_loglevel, 
 *   -- default_message_loglevel, 
 *   -- minimum_console_loglevel, and 
 *   -- default_console_loglevel. 
 * These values influence printk() behavior when printing or logging 
 * error messages. 
 *
 * (a) Messages with a higher priority than console_loglevel will be printed
 * to the console. 
 * (b) Messages without an explicit priority will be printed with priority
 * default_message_loglevel. 
 * (c) minimum_console_loglevel is the minimum (highest) value to which 
 * console_loglevel can be set. 
 * (d) default_console_loglevel is the default value for console_loglevel.
 * 
 * The kernel defaults for these are defined in the kernel source,
 * as DEFAULT_CONSOLE_LOGLEVEL, DEFAULT_MESSAGE_LOGLEVEL, etc.
 *
 * The console log-level can be controlled by invoking /bin/dmesg -n level
 * during init, or by using the sysrq key [if support for sysrq has been
 * enabled]. It can also be specified as a boot parameter, and can be 
 * controlled via the printk sysctl's above through sysctl.conf.
 *
 * proc/sys/kernel/printk_ratelimit_jiffies is the token-bucket fill-rate
 * in jiffies for syslog.
 * proc/sys/kernel/printk_ratelimit_burst is the maximum depth of
 * the token bucket.
 *---------------------------------------------------------------------------*/

#define VQEC_LOG_BUF_SIZE 1024
#define VQEC_PRINTK_RATELIMIT_JIFFIES (HZ * 10) / 1000
#define VQEC_PRINTK_RATELIMIT_BURST 10
int32_t vqec_printk_ratelimit_jiffies = VQEC_PRINTK_RATELIMIT_JIFFIES;
int32_t vqec_printk_ratelimit_burst = VQEC_PRINTK_RATELIMIT_BURST;

static char *s_vqec_loglevel_str[] = {
    KERN_EMERG,                 /* 0: LOG_EMERG */
    KERN_ALERT,                 /* 1: LOG_ALERT */
    KERN_CRIT,                  /* 2: LOG_CRIT */
    KERN_ERR,                   /* 3: LOG_ERR */
    KERN_WARNING,               /* 4: LOG_WARN */
    KERN_NOTICE,                /* 5: LOG_NOTICE */
    KERN_INFO,                  /* 6: LOG_INFO */
    KERN_DEBUG                  /* 7: LOG_DEBUG */
};

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,26)
DEFINE_RATELIMIT_STATE(vqec_printk_ratelimit_state,
                       VQEC_PRINTK_RATELIMIT_JIFFIES,
                       VQEC_PRINTK_RATELIMIT_BURST);
#endif

/**---------------------------------------------------------------------------
 * Implementation of syslog in the kernel. Kernel logs will be channeled to 
 * /var/log/messages. All priorities except *LOG_DEBUG* are rate-limited.
 * LOG_DEBUG is not rate-limited [users should be careful not to enable
 * per-packet debug].
 *
 * For syslog's the token bucket will fill at vqec_printk_ratelimit_jiffies
 * with a maximum burst depth of vqec_printk_ratelimit_burst.
 *
 * @param[in] pri Priority
 * @param[in] fmt Format string.
 * @param[in] ap Variable argument list.
 *---------------------------------------------------------------------------*/
void
vqes_vsyslog (int32_t pri, const char *fmt, va_list ap)
{
    static char vqec_fmtbuf[VQEC_LOG_BUF_SIZE];

    ASSERT(pri <= LOG_DEBUG);

    if (likely((pri == LOG_DEBUG) || 
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,26)
               __ratelimit(&vqec_printk_ratelimit_state))) {
#else
               __printk_ratelimit(vqec_printk_ratelimit_jiffies,
                                  vqec_printk_ratelimit_burst))) {
#endif
        (void)snprintf(vqec_fmtbuf, sizeof(vqec_fmtbuf), 
                       "%s %s", s_vqec_loglevel_str[pri], fmt);
        (void)vprintk(vqec_fmtbuf, ap);
    }
}


/**---------------------------------------------------------------------------
 * Variadic wrapper for the main syslog routine [unsafe version] 
 *
 * @param[in] pri Priority
 * @param[in] fmt Format string followed by variable arguments.
 *---------------------------------------------------------------------------*/
void
vqes_syslog_ul (int32_t pri, const char *fmt,...)
{
    va_list ap;
    
    va_start(ap, fmt);
    vqes_vsyslog(pri, fmt, ap);
    va_end(ap);
}


/**---------------------------------------------------------------------------
 * Variadic wrapper for the main syslog routine. [safe version, however,
 * no protection is needed for the client dataplane].
 *
 * @param[in] pri Priority
 * @param[in] fmt Format string followed by variable arguments.
 *---------------------------------------------------------------------------*/
void
vqes_syslog (int32_t pri, const char *fmt,...)
{
    va_list ap;
    
    va_start(ap, fmt);
    vqes_vsyslog(pri, fmt, ap);
    va_end(ap);
}


/**---------------------------------------------------------------------------
 * STUB for open syslog: no-op in kernel.
 *---------------------------------------------------------------------------*/
void
vqes_openlog (const char *ident, int logstat, int logfac, 
              int fd[], int num_fds)
{
}


/**---------------------------------------------------------------------------
 * STUB for closing syslog socket: no-op in kernel [controlled via facility
 * filter levels]
 *---------------------------------------------------------------------------*/
void
vqes_closelog (void)
{
}


/**---------------------------------------------------------------------------
 * STUB for syslog log mask level: no-op in kernel.
 *---------------------------------------------------------------------------*/
int32_t
vqes_setlogmask (int32_t pmask)
{
    return (pmask);
}


/**---------------------------------------------------------------------------
 * STUB for syslog mutex: not needed since all calls made under global lock.
 *---------------------------------------------------------------------------*/
void 
vqes_syslog_lock_lock (void)
{
}


/**---------------------------------------------------------------------------
 * STUB for syslog mutex: not needed since all calls made under global lock.
 *---------------------------------------------------------------------------*/
void 
vqes_syslog_lock_unlock (void)
{
}

