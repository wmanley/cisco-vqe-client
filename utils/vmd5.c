/*
 * This code implements the MD5 message-digest algorithm.
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
 * Changed so as no longer to depend on Colin Plumb's `usual.h' header
 * definitions; now uses stuff from dpkg's config.h.
 *  - Ian Jackson <ijackson@nyx.cs.du.edu>.
 * Still in the public domain.
 */

#include <string.h>		/* for memcpy() */
#ifndef WIN32
#include <sys/types.h>		/* for stupid systems */
#include <netinet/in.h>		/* for ntohl() */
#endif

#include "vmd5.h"

/* Add _BIG_ENDIAN for Solaris    -ctam */
#if defined(WORDS_BIGENDIAN) || defined(_BIG_ENDIAN) || __BYTE_ORDER == __BIG_ENDIAN
void
byteSwap(u_int32_t *buf, unsigned words)
{
    md5byte *p = (md5byte *)buf;

    do
    {
        *buf++ = (u_int32_t)((unsigned)p[3] << 8 | p[2]) << 16 |
                 ((unsigned)p[1] << 8 | p[0]);
        p += 4;
    }
    while (--words);
}
#else
#define byteSwap(buf,words)
#endif

/*
#ifdef WORDS_BIGENDIAN
void
byteSwap(u_int32_t *buf, unsigned words)
{
    md5byte *p = (md5byte *)buf;

    do
    {
        *buf++ = (u_int32_t)((unsigned)p[3] << 8 | p[2]) << 16 |
                 ((unsigned)p[1] << 8 | p[0]);
        p += 4;
    }
    while (--words);
}
#else
#define byteSwap(buf,words)
#endif
*/


/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
void
vqe_MD5Init(struct MD5Context *ctx)
{
    ctx->buf[0] = 0x67452301;
    ctx->buf[1] = 0xefcdab89;
    ctx->buf[2] = 0x98badcfe;
    ctx->buf[3] = 0x10325476;

    ctx->bytes[0] = 0;
    ctx->bytes[1] = 0;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
void
vqe_MD5Update(struct MD5Context *ctx, md5byte const *buf, unsigned len)
{
    uint32_t t;

    /* Update byte count */

    t = ctx->bytes[0];
    if ((ctx->bytes[0] = t + len) < t)
        ctx->bytes[1]++; 	/* Carry from low to high */

    t = 64 - (t & 0x3f); 	/* Space available in ctx->in (at least 1) */
    if (t > len)
    {
        memcpy((md5byte *)ctx->in + 64 - t, buf, len);
        return ;
    }
    /* First chunk is an odd size */
    memcpy((md5byte *)ctx->in + 64 - t, buf, t);
    byteSwap(ctx->in, 16);
    vqe_MD5Transform(ctx->buf, ctx->in);
    buf += t;
    len -= t;

    /* Process data in 64-byte chunks */
    while (len >= 64)
    {
        memcpy(ctx->in, buf, 64);
        byteSwap(ctx->in, 16);
        vqe_MD5Transform(ctx->buf, ctx->in);
        buf += 64;
        len -= 64;
    }

    /* Handle any remaining bytes of data. */
    memcpy(ctx->in, buf, len);
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
void
vqe_MD5Final(md5byte digest[16], struct MD5Context *ctx)
{
    int count = ctx->bytes[0] & 0x3f; 	/* Number of bytes in ctx->in */
    md5byte *p = (md5byte *)ctx->in + count;

    /* Set the first char of padding to 0x80.  There is always room. */
    *p++ = 0x80;

    /* Bytes of padding needed to make 56 bytes (-8..55) */
    count = 56 - 1 - count;

    if (count < 0)
    {	/* Padding forces an extra block */
        memset(p, 0, count + 8);
        byteSwap(ctx->in, 16);
        vqe_MD5Transform(ctx->buf, ctx->in);
        p = (md5byte *)ctx->in;
        count = 56;
    }
    memset(p, 0, count);
    byteSwap(ctx->in, 14);

    /* Append length in bits and transform */
    ctx->in[14] = ctx->bytes[0] << 3;
    ctx->in[15] = ctx->bytes[1] << 3 | ctx->bytes[0] >> 29;
    vqe_MD5Transform(ctx->buf, ctx->in);

    byteSwap(ctx->buf, 4);
    memcpy(digest, ctx->buf, 16);
    memset(ctx, 0, sizeof(ctx)); 	/* In case it's sensitive */
}

#ifndef ASM_MD5

/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f,w,x,y,z,in,s) \
(w += f(x,y,z) + in, w = (w<<s | w>>(32-s)) + x)

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
void
vqe_MD5Transform(u_int32_t buf[4], u_int32_t const in[16])
{
    register uint32_t a, b, c, d;

    a = buf[0];
    b = buf[1];
    c = buf[2];
    d = buf[3];

    MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
    MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
    MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
    MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
    MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
    MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

    MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
    MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
    MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

    MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
    MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
    MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

    MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
    MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
    MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
    MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
    MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
    MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
    MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}

/*
 * Computes the MD5 checksum for a file, and then
 * encodes it as a 32-char printable hex string (where each 8 bit byte is
 * mapped to 2 printable hex digits, one for each nibble in the byte).
 * The generated string will be NULL terminated.
 *
 * Upon success, a value of 0 will be returned, and "chksum_str" will
 * contain the file's checksum string.
 * Upon failure, a value of -1 will be returned and "chksum_str" will
 * be unmodified.
 *
 * @param[in]  pathname    - path of file whose checksum string is desired
 * @param[out] chksum_str  - generated MD5 checksum string
 * @param[out] int         -  0 upon success
 *                           -1 upon failure
 */
int
vqe_MD5ComputeChecksumStr (const char *pathname, boolean is_file,
                       char chksum_str[MD5_CHECKSUM_STRLEN])
{
    FILE *fp = NULL;
    unsigned char buf[1024];
    struct MD5Context ctx;
    int n, i, err = -1;
    md5byte digest[16];
    char hex_digits[16] = 
        {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 
         'a', 'b', 'c', 'd', 'e', 'f'};

    if (!chksum_str || !pathname) {
        goto done;
    }

    if (is_file) {
        fp = fopen(pathname, "r");
        if (!fp) {
            goto done;
        }
    }

    vqe_MD5Init(&ctx);
    if (is_file) {
        while (1) {
            n = fread(buf, 1, sizeof(buf), fp);
            if (n < 0) {
                goto done;
            } else if (n == 0) {
                break;
            }
            vqe_MD5Update(&ctx, buf, n);
        }
        if (fclose(fp)) {
            goto done;
        }
    } else {
        n = strlen(pathname);
        vqe_MD5Update(&ctx, (unsigned char*)pathname, n);
    }
    vqe_MD5Final(digest, &ctx);

    for (i = 0; i < 16; i++) {
        /*
         * Derive two printable hex bytes in the chksum_str string from a 
         * single digest byte by mapping the value of the lower and upper 
         * nibbles to their corresponding hex digit.  Avoid sign extension
         * upon shifting the lower nibble.
         */
        chksum_str[i*2] = hex_digits[(digest[i] >> 4) & 0x0f];
        chksum_str[i*2 + 1] = hex_digits[digest[i] & 0x0f];
    }
    chksum_str[MD5_CHECKSUM_STRLEN-1] = '\0';
    err = 0;

done:
    return (err);
}
                      
#endif
