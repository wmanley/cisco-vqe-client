/*
 * Copyright (c) 2006-2008 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include "test_vqec_utest_main.h"
#include "../add-ons/include/CUnit/CUnit.h"
#include "../add-ons/include/CUnit/Basic.h"
#include "vqec_url.h"

extern vqec_protocol_t vqec_url_protocol_parse(char * protocol);

/*
 * Unit tests for vqec_url
 */

vqec_url_t chan;
char *chanstr;
vqec_protocol_t protocol;
in_addr_t ip;
uint16_t port;
struct in_addr in;

int test_vqec_url_init (void) {
    chan = malloc(50);
    if (chan == NULL)
    {
        printf("malloc failure in test_vqec_url_init()!!\n");
        return -1;
    }
    memset(chan, 0, 50);
    chanstr = malloc(50);
    if (chanstr == NULL)
    {
        printf("malloc failure in test_vqec_url_init()!!\n");
        return -1;
    }
    memset(chanstr, 0, 50);
    return 0;
}

int test_vqec_url_clean (void) {
  return 0;
}

static void test_vqec_url_build (void) {
  int built_len;

  printf("\n\n\ntesting vqec_url_build\n\n\n");

  char *ipaddr;
  int bport;
  int len = 15;  /* not enough room for IP to be built */
  ipaddr = "1.2.3.4";
  bport = 5000;
  printf("Building (IN): %s:%d (%s)\n", ipaddr, bport, "rtp");
  built_len = vqec_url_build(VQEC_PROTOCOL_RTP, 
		 inet_addr(ipaddr), 
		 htons(bport), chanstr, len);
  printf("%s: %s\nlength: %d\n\n", built_len ? "SUCCESS" : "FAILURE", chanstr, len);
  CU_ASSERT(built_len == 0);

  /* test zero-length */
  built_len = vqec_url_build(VQEC_PROTOCOL_RTP, 
		 inet_addr(ipaddr), 
		 htons(bport), chanstr, 0);
  CU_ASSERT(built_len == 0);

  len = VQEC_MAX_URL_LEN;
  ipaddr = "1.2.3.4";
  bport = 5000;
  printf("Building (IN): %s:%d (%s)\n", ipaddr, bport, "rtp");
  built_len = vqec_url_build(VQEC_PROTOCOL_RTP, 
		 inet_addr(ipaddr), 
		 htons(bport), chanstr, len);
  printf("%s: %s\nlength: %d\n\n", built_len ? "SUCCESS" : "FAILURE", chanstr, len);
  CU_ASSERT(built_len == 18);
  CU_ASSERT(!strncmp("rtp://1.2.3.4:5000", chanstr, VQEC_MAX_URL_LEN));

  /* test unknown protocol */
  built_len = vqec_url_build(VQEC_PROTOCOL_UNKNOWN, 
		 inet_addr(ipaddr), 
		 htons(bport), chanstr, len);
  CU_ASSERT(built_len == 0);

  ipaddr = "128.224.133.144";
  bport = 50001;
  printf("Building (IN): %s:%d (%s)\n", ipaddr, bport, "rtp");
  built_len = vqec_url_build(VQEC_PROTOCOL_RTP, 
		 inet_addr(ipaddr), 
		 htons(bport), chanstr, len);
  printf("%s: %s\nlength: %d\n\n", built_len ? "SUCCESS" : "FAILURE", chanstr, len);
  CU_ASSERT(built_len == 27);
  CU_ASSERT(!strncmp("rtp://128.224.133.144:50001", chanstr, VQEC_MAX_URL_LEN));

  ipaddr = "128.224.133.144";
  bport = 50001;
  printf("Building (IN): %s:%d (%s)\n", ipaddr, bport, "rtp");
  built_len = vqec_url_build(VQEC_PROTOCOL_RTP, 
		 inet_addr(ipaddr), 
		 htons(bport), chanstr, len);
  printf("%s: %s\nlength: %d\n\n", built_len ? "SUCCESS" : "FAILURE", chanstr, len);
  CU_ASSERT(built_len == 27);
  CU_ASSERT(!strncmp("rtp://128.224.133.144:50001", chanstr, VQEC_MAX_URL_LEN));

  ipaddr = "128.224.133.144";
  bport = 50001;
  printf("Building (IN): %s:%d (%s)\n", ipaddr, bport, "rtp");
  built_len = vqec_url_build(VQEC_PROTOCOL_RTP, 
		 inet_addr(ipaddr), 
		 htons(bport), chanstr, len);
  printf("%s: %s\nlength: %d\n\n", built_len ? "SUCCESS" : "FAILURE", chanstr, len);
  CU_ASSERT(built_len == 27);
  CU_ASSERT(!strncmp("rtp://128.224.133.144:50001", chanstr, VQEC_MAX_URL_LEN));

  ipaddr = "128.224.133.145";
  bport = 50012;
  printf("Building (IN): %s:%d (%s)\n", ipaddr, bport, "udp");
  built_len = vqec_url_build(VQEC_PROTOCOL_UDP, 
		 inet_addr(ipaddr), 
		 htons(bport), chanstr, len);
  printf("%s: %s\nlength: %d\n\n", built_len ? "SUCCESS" : "FAILURE", chanstr, len);
  CU_ASSERT(built_len == 27);
  CU_ASSERT(!strncmp("udp://128.224.133.145:50012", chanstr, VQEC_MAX_URL_LEN));
}

static void test_vqec_url_parse (void) {
#define TEST_PARSE(arg) { \
  char temp[ INET_ADDRSTRLEN ]; \
  chan = arg; \
  parse_status = 0; \
  printf("Parsing (IN): %s\n", chan); \
  parse_status = vqec_url_parse(chan, &protocol, &ip, &port); \
  in.s_addr = ip; \
  inet_ntop(AF_INET, &in.s_addr, temp, INET_ADDRSTRLEN); \
  printf("%s: %s:%d (%s)\n\n", \
         parse_status ? "SUCCESS" : "FAILURE", \
         temp, \
	 ntohs(port), \
	 vqec_url_protocol_to_string(protocol)); \
  }

  int parse_status = 0;

  TEST_PARSE(NULL);
  CU_ASSERT(!parse_status);

  TEST_PARSE("udp://224.1.1.1:50000");
  CU_ASSERT(parse_status);

  TEST_PARSE("rtp://224.2.3.4:51000");
  CU_ASSERT(parse_status);

  TEST_PARSE("rtsp://224.5.6.7:52000");
  CU_ASSERT(!parse_status);

  TEST_PARSE("igmp://224.8.9.10:53000");
  CU_ASSERT(!parse_status);

  TEST_PARSE("udp://224.1.1.261:50000");
  CU_ASSERT(!parse_status);

  TEST_PARSE("udp://224.1.1.1:500000");
  CU_ASSERT(!parse_status);

  TEST_PARSE("udppp://224.1.1.1:50000");
  CU_ASSERT(!parse_status);

  TEST_PARSE("rtsp:/224.1.1.1:50000");
  CU_ASSERT(!parse_status);

  TEST_PARSE("rtp://224.1.1.150000");
  CU_ASSERT(!parse_status);

  TEST_PARSE("http://224.1.1:50000");
  CU_ASSERT(!parse_status);

  TEST_PARSE("rtp://224.1.1.1:-145");
  CU_ASSERT(!parse_status);

  TEST_PARSE("udp224.1.2.3:50000");
  CU_ASSERT(!parse_status);

  TEST_PARSE("udp//224.1.1.1:50000");
  CU_ASSERT(!parse_status);

  TEST_PARSE("udp//224.11.1:50000");
  CU_ASSERT(!parse_status);

  TEST_PARSE("udp://232.45.99:1234");
  CU_ASSERT(!parse_status);

  TEST_PARSE("udp://232.45.99.32.4:1234");
  CU_ASSERT(!parse_status);

  TEST_PARSE("udp://232.45.99.32");
  CU_ASSERT(!parse_status);

  TEST_PARSE("udp://232.-45.99.32:2345");
  CU_ASSERT(!parse_status);

  TEST_PARSE("udp://232.45.99.5:1234");
  CU_ASSERT(parse_status);

}

CU_TestInfo test_array_url[] = {
    {"test vqec_url_build",test_vqec_url_build},
    {"test vqec_url_parse",test_vqec_url_parse},
    CU_TEST_INFO_NULL,
};



