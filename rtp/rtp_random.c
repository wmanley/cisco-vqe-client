/*------------------------------------------------------------------
 *
 * July 2006, Atif Faheem
 *
 * Copyright (c) 2006-2009 by cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#include "rtp_types.h"
#include "vmd5.h"
#include "vam_util.h"
#include <sys/utsname.h>

/* Random Generation. Taken from RFC 3550*/
#define MD_CTX struct MD5Context
#define MDInit vqe_MD5Init
#define MDUpdate vqe_MD5Update
#define MDFinal vqe_MD5Final

#define DIGEST_BYTES  16
#define DIGEST_LWORDS  4

static uint32_t md_32(char *string, int length)
{
    MD_CTX context;
    union {
        unsigned char c[DIGEST_BYTES];
        uint32_t x[DIGEST_LWORDS];
    } digest;
    uint32_t r;
    int32_t i;
    MDInit (&context);
    MDUpdate (&context, (unsigned char *)string, length);
    MDFinal ((unsigned char *)&digest, &context);
    r = 0;
    for (i = 0; i < 3; i++) {
        r ^= digest.x[i];
    }

    return r;
} /* md_32 */

/*
 * Return random unsigned 32-bit quantity. Use 'type' argument if
 * you need to generate several different values in close succession.
 */
uint32_t rtp_random32(int type)
{
    struct {
        int32_t type;
        struct timeval tv;
        clock_t cpu;
        pid_t pid;
        uint32_t hid;
#define MAC_LEN 18
        char mac_addr[MAC_LEN];
        uid_t uid;
        gid_t gid;
        struct utsname name;
    } s;
    (void)gettimeofday(&s.tv, 0);
    (void)uname(&s.name);
    s.type = type;
    s.cpu = clock();
    s.pid = getpid();
    /* 
     * s.hid is intentionally not initialized, thus providing an  
     * variation in the input used to generate the random quantity
     */ 
    /*
     * instead  of host id, we initialize another item that
     * will uniquely identify the system: the MAC address
     * of the first non-loopback interface.
     */
    (void)get_first_mac_address(s.mac_addr, MAC_LEN);
    s.uid = getuid();
    s.gid = getgid();
    /* also: system uptime */

    return md_32((char *)&s, sizeof(s));
} /* rtp_random32 */


