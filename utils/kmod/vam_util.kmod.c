/******************************************************************************
 *
 * Cisco Systems, Inc.
 *
 * Copyright (c) 2008-2009 by Cisco Systems, Inc.
 * All rights reserved.
 *
 ******************************************************************************
 *
 * File: 
 *
 * Description: Kernel utilities.
 *
 * Documents: 
 *
 *****************************************************************************/

#include <utils/vam_util.h>
#define SYSLOG_DEFINITION
#include <log/vqe_utils_syslog_def.h>
#undef SYSLOG_DEFINITION

#define VQE_IPV4_OCTETS 4
#define VQE_IPV4_OCTET_BITS 8
#define VQE_IPV4_OCTET_MASK  ((1 << VQE_IPV4_OCTET_BITS) - 1)

/**---------------------------------------------------------------------------
 * Convert a IPv4 address in network-order to dotted-decimal notation. 
 *
 * @param[in] addr 32-bit IPv4 address in network order.
 * @param[out] buf Buffer to which the dotted-decimal notation is written.
 * @param[out] buf_len Length of the buffer
 * @param[out] char* Returns the pointer to "buf" if the method was
 * successfully able to generate a dotted-decomal notation. Returns NULL
 * if the buffer length was too small or if any other error was encountered.
 *---------------------------------------------------------------------------*/
char *uint32_ntoa_r (uint32_t addr, char *buf, int32_t buf_len)
{
    return (uint32_htoa_r(ntohl(addr), buf, buf_len));
}


/**---------------------------------------------------------------------------
 * Convert a IPv4 address in host-order to dotted-decimal notation. 
 *
 * @param[in] addr 32-bit IPv4 address in host order.
 * @param[out] buf Buffer to which the dotted-decimal notation is written.
 * @param[out] buf_len Length of the buffer
 * @param[out] char* Returns the pointer to "buf" if the method was
 * successfully able to generate a dotted-decomal notation. Returns NULL
 * if the buffer length was too small or if any other error was encountered.
 *---------------------------------------------------------------------------*/
char *uint32_htoa_r (uint32_t addr, char *buf, int32_t buf_len)
{
    int32_t i;
    uint32_t mask;
    uint8_t octet[VQE_IPV4_OCTETS];

    if (unlikely(!buf ||
                 buf_len < (VQE_IPV4_ADDR_STRLEN + 1))) {
        return (NULL);
    }
    
    for (i = 0; i < VQE_IPV4_OCTETS; i++) { 
        mask = VQE_IPV4_OCTET_MASK << (VQE_IPV4_OCTET_BITS * i);
        octet[i] = (addr & mask) >> (VQE_IPV4_OCTET_BITS * i);
    }

    if (snprintf(buf, buf_len,
                 "%u.%u.%u.%u", 
                 octet[3], octet[2], octet[1], octet[0]) >= buf_len) {
        return (NULL);
    }

    return (buf);
}


/**---------------------------------------------------------------------------
 *  bsearch - same arguments as linux bsearch.
 *---------------------------------------------------------------------------*/
void *bsearch (void *__key, void *__base,
               size_t nmemb, size_t size, compar_fn_t compar)
{
    int32_t i, r;

    if (!__key || !__base || !compar || !nmemb || !size) {
        return (NULL);
    }

    
    if (nmemb == 1) {
        r = (*compar)(__key, __base);
        if (!r) {
            return __base;
        } else {
            return NULL;
        }
    }
    
    i = nmemb / 2 - 1;
    r = (*compar)(__key, (char *)__base + size * i);
    if (!r) {
        return ((char *)__base + size * i);
    } else if (r > 0) {
        return bsearch(__key, (char *)__base + ((i + 1) * size), 
                       nmemb - (i + 1), size, compar);
        
    } else {
        return bsearch(__key, (char *)__base, 
                       i, size, compar);
    }
} 

