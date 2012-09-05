#ifndef VMD5_H
#define VMD5_H

static const char* const vmd5HeaderVersion =
    "$Id: vmd5.h,v 1.1 2001/03/26 10:35:31 icahoon Exp $";

/*
 * This is the header file for the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 *
 * Changed so as no longer to depend on Colin Plumb's `usual.h'
 * header definitions; now uses stuff from dpkg's config.h
 *  - Ian Jackson <ijackson@nyx.cs.du.edu>.
 * Still in the public domain.
 */


#include "vam_types.h"
#ifdef __cplusplus
extern "C"
{
#endif

#define md5byte unsigned char
/*
 * Length of the 16-byte MD5 checksum mapped to a hexadecimal-encoded string.
 * Each 8 bit byte is mapped to two printable hex digits, with one extra
 * char allowed for a terminating '\0'.
 */
#define MD5_CHECKSUM_STRLEN (32 + 1)

    struct MD5Context
    {
        uint32_t buf[4];
        uint32_t bytes[2];
        uint32_t in[16];
    };

    void vqe_MD5Init(struct MD5Context *context);
    void vqe_MD5Update(struct MD5Context *context, md5byte const *buf, unsigned len);
    void vqe_MD5Final(unsigned char digest[16], struct MD5Context *context);
    void vqe_MD5Transform(uint32_t buf[4], uint32_t const in[16]);
    int  vqe_MD5ComputeChecksumStr(const char *pathname, boolean is_file,
                                   char chksum_str[MD5_CHECKSUM_STRLEN]);

#ifdef __cplusplus
}
#endif

/* !MD5_H */
#endif
