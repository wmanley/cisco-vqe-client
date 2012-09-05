/*
 * Copyright (c) 2006-2007 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "vqec_url.h"
#include <stdio.h>
#include <string.h>
#include "vqec_assert_macros.h"
#include <utils/strl.h>

#ifdef _VQEC_UTEST_INTERPOSERS
#define UT_STATIC 
#else
#define UT_STATIC static
#endif

UT_STATIC vqec_protocol_t vqec_url_protocol_parse (char * protocol) {
    assert(protocol);

    if (strcasecmp(protocol, "udp") == 0) {
        return VQEC_PROTOCOL_UDP;
    } else if (strcasecmp(protocol, "rtp") == 0) {
        return VQEC_PROTOCOL_RTP;
    } else {
        return VQEC_PROTOCOL_UNKNOWN;
    }
}

char * vqec_url_protocol_to_string (vqec_protocol_t protocol) {
    switch (protocol) {
      case VQEC_PROTOCOL_UNKNOWN:
        return "UNKNOWN";
      case VQEC_PROTOCOL_UDP:
        return "UDP";        
      case VQEC_PROTOCOL_RTP:
        return "RTP";
      default:
        return "UNKNOWN";
    }
}

/* 
 * vqec_url_ip_parse
 * parse an ip-address string into a numeric s_addr
 */
boolean vqec_url_ip_parse (char *ip_string, in_addr_t *ip)
{
    in_addr_t parsed_ip;
    struct in_addr temp_addr;

    if (!inet_aton(ip_string, &temp_addr)) {
        return FALSE;
    }
    parsed_ip = temp_addr.s_addr;

    /* make sure there are three decimal points (".") in the ip string */
    int i = 0;
    char *dotindex = ip_string - 1;
    for (i = 0 ; (i < 5 && dotindex != NULL) ; i++) {
        dotindex = strchr(dotindex + 1, '.');
    }
    if (i != 4) {
        return FALSE;
    }

    /* fail if IP address parses to 255.255.255.255 */
    if (parsed_ip == 0xffffffff) {
        return FALSE;
    }

    *ip = parsed_ip;
    return TRUE;
}

/*
 * vqec_url_build
 * given a protocol, ip, and port, build a url string
 */
int vqec_url_build (vqec_protocol_t protocol, in_addr_t ip, 
                       uint16_t port, vqec_url_t url, int url_len) {

    char * build_buf;
    int built_len = 0;
    char temp[INET_ADDRSTRLEN];
    struct in_addr in;

    if (url_len < VQEC_MAX_URL_LEN) {
        return 0;
    }

    build_buf = malloc(VQEC_MAX_URL_LEN);
    if (!build_buf) {
        return 0;
    }
    memset(build_buf, 0, VQEC_MAX_URL_LEN);
    
    /* stringify protocol and start the url with it */
    switch(protocol) {
      case VQEC_PROTOCOL_UDP:
          snprintf(build_buf, VQEC_MAX_URL_LEN, "udp://");
        break;
      case VQEC_PROTOCOL_RTP:
          snprintf(build_buf, VQEC_MAX_URL_LEN, "rtp://");
        break;
      case VQEC_PROTOCOL_UNKNOWN:
        goto build_bail;
      default:
        VQEC_ASSERT(0);
        goto build_bail;
    }

    /* stringify IP address and append it to the url */
    in.s_addr = ip;
    if (!inet_ntop(AF_INET, &in.s_addr, temp, INET_ADDRSTRLEN)) {
        goto build_bail;    
    }
    (void)strlcat(build_buf, temp, VQEC_MAX_URL_LEN);

    /* stringify port and append it to the url */
    snprintf(temp, INET_ADDRSTRLEN, ":%d", ntohs(port));
    (void)strlcat(build_buf, temp, VQEC_MAX_URL_LEN);

    /* copy the url to the "vqec_url_t url" that was provided to us */
    (void)strlcpy(url, build_buf, url_len);

    built_len = strlen(build_buf);
    free(build_buf);
    return built_len;

build_bail:
    if (build_buf) {
        free(build_buf);
    }
    return 0;
}


/*
 * vqec_url_parse
 * parse a url string and return the protocol, ip, and port
 */
boolean vqec_url_parse (vqec_url_t url, vqec_protocol_t *protocol, 
                       in_addr_t *ip, uint16_t *port) {
    char * buf;
    char * temp_buf; /* stay pointed to beginning while we modify buf */

    char * str_protocol;
    char * str_ip;
    char * str_port;
    uint32_t len;

    char * last;

    if (!protocol || !ip || !port || !url) {
        return FALSE;
    }

    temp_buf = buf = malloc(VQEC_MAX_URL_LEN);
    if (!buf) {
        return FALSE;
    }
    memset(buf, 0, VQEC_MAX_URL_LEN);
    (void)strlcpy(buf, (char *)url, VQEC_MAX_URL_LEN);

    len = strlen(buf);
    if (len > VQEC_MAX_URL_LEN || len == 0) {
        goto parse_bail;
    }


    /* make sure that the string "://" exists in the url first */
    if(!strstr(buf,"://")){
        goto parse_bail;
    }

    /* parse the protocol portion */
    str_protocol = strtok_r(buf, "://", &last);
    if (!str_protocol) {
        goto parse_bail;
    }        
    buf += strlen(str_protocol) + strlen("://");

    *protocol = vqec_url_protocol_parse(str_protocol);
    if (*protocol == VQEC_PROTOCOL_UNKNOWN) {
        goto parse_bail;
    }

    /* parse the ip address portion */
    str_ip = strtok_r(buf, ":", &last);
    if (!str_ip) {
        goto parse_bail;
    }
    buf += strlen(str_ip) + strlen(":");

    if (!vqec_url_ip_parse(str_ip, ip)) {
        goto parse_bail;
    }

    str_port = buf;
    *port = htons(atoi(str_port));
    if(atoi(str_port) < 1 || atoi(str_port) > 65535){
        goto parse_bail;
    }

    if (temp_buf) {
        free(temp_buf);
    }

    return TRUE;

parse_bail:
    if (temp_buf) {
        free(temp_buf);
    }
    *protocol = 0;
    *ip = 0;
    *port = 0;
    return FALSE;
}

