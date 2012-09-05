/*------------------------------------------------------------------
 *
 * test_stun_parser.c
 *
 * Copyright (c) 2007-2008 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * This file is intended SOLELY for testing purposes within the stun
 * component.  It should not be used as a socket interface for any
 * other purpose.
 *
 *------------------------------------------------------------------
 */
#include "stun.h"
#include "stun_private.h"
#include <getopt.h>

/* this program tests parsing of various STUN messages, as well as
   message generation. */

bool verbose = false;
unsigned int test_pass = 0;
unsigned int test_fail = 0;

#define TEST(x)							\
  do {								\
    if(x) {							\
      test_pass++;						\
      if(verbose)						\
	printf("%s:%d: pass: %s\n", __FILE__, __LINE__, #x);	\
    } else {							\
      test_fail++;						\
      printf("%s:%d: FAIL: %s\n", __FILE__, __LINE__, #x);	\
    }								\
  } while(0)



static void parse_cycle(void)
{
  transaction_id_t *tid;
  tid = NULL;
  {
    uint8_t *msg;
    stun_message_t stun_msg;
    uint32_t len;
    uint ret_code;

    msg = stun_generate_binding_request((uint8_t *)"hello", strlen("hello"), NULL, 0, 123456, &tid, &len);
    
    ret_code = stun_message_parse(msg, len, &stun_msg, false);
    TEST(ret_code == 0);
    TEST(stun_msg.stun_header.message_type == STUN_BINDING_REQ);
    TEST(stun_msg.username.content != NULL);
    TEST(stun_msg.password.content == NULL);
    TEST(stun_msg.ice_priority.content == 123456);
    free(msg);
    if (tid != NULL) {
      free(tid);
      tid = NULL;
    }
  }


  {
    uint8_t *msg;
    stun_message_t stun_msg;
    uint32_t len;
    uint ret_code;
    
    msg = stun_generate_binding_request((uint8_t *)"hello", strlen("hello"), 
					(uint8_t *)"password", strlen("password"), 123456, &tid, &len);
    
    ret_code = stun_message_parse_with_integrity_check(msg, len, 
						       (uint8_t *)"password", strlen("password"), 
				  &stun_msg, false);
    TEST(ret_code == 0);
    TEST(stun_msg.stun_header.message_type == STUN_BINDING_REQ);
    TEST(stun_msg.username.content != NULL);
    TEST(stun_msg.password.content == NULL);
    TEST(stun_msg.ice_priority.content == 123456);
    free(msg);
    if (tid != NULL) {
      free(tid);
      tid = NULL;
    }
  }


  {
    uint8_t *msg;
    stun_message_t stun_msg2;
    uint32_t len;
    uint ret_code;
    stun_address_t addr;
    struct in_addr ad;
    uint8_t *username;
    uint32_t username_len;

    uint8_t *realm;
    uint32_t realm_len;

    if (!inet_aton("192.168.10.53", &ad)) {
        printf("inet_aton failed");
        return;
    }
    addr.family = STUN_ADDRESS_FAMILY_IPV4;
    addr.port = 1024;
    addr.addr.ipv4_addr.s_addr = ad.s_addr;
    
    msg = stun_generate_set_active_destination_request((uint8_t *)"hello", 
						       strlen("hello"), 
						       NULL, 0,
						       &addr,
						       &tid, &len);

    ret_code = stun_message_parse(msg, len, &stun_msg2, false);
    TEST(ret_code == 0);
    TEST(stun_msg2.stun_header.message_type == STUN_SET_ACTIVE_DEST_REQ);
    TEST(stun_msg2.username.content != NULL);
    TEST(stun_msg2.password.content == NULL);

    stun_message_find_username_realm(msg, len, 
				     &username, &username_len,
				     &realm, &realm_len);

    TEST(username != NULL);
    TEST(strcmp((char *)username, "hello") == 0);
    TEST(username_len == strlen("hello"));

    TEST(realm == NULL);

    free(msg);
    if (tid != NULL) {
      free(tid);
      tid = NULL;
    }
  }


  {
    uint8_t *msg;
    stun_message_t stun_msg2;
    uint32_t len;
    uint ret_code;
    stun_address_t addr;
    struct in_addr ad;
    uint8_t *username;
    uint32_t username_len;

    uint8_t *realm;
    uint32_t realm_len;

    if (!inet_aton("192.168.10.53", &ad)) {
        printf("inet_aton failed");
        return;
    }
    addr.family = STUN_ADDRESS_FAMILY_IPV4;
    addr.port = 1024;
    addr.addr.ipv4_addr.s_addr = ad.s_addr;
    
    msg = stun_generate_connect_request((uint8_t *)"hello", 
					strlen("hello"), 
					NULL, 0,
					&addr,
					&tid, &len);

    ret_code = stun_message_parse(msg, len, &stun_msg2, false);
    TEST(ret_code == 0);
    TEST(stun_msg2.stun_header.message_type == STUN_CONNECT_REQ);
    TEST(stun_msg2.username.content != NULL);
    TEST(stun_msg2.password.content == NULL);
    TEST(stun_msg2.destination_address.is_valid == true);
    TEST(stun_msg2.destination_address.content.port == 1024);
    TEST(stun_msg2.destination_address.content.addr.ipv4_addr.s_addr == ad.s_addr);

    stun_message_find_username_realm(msg, len, 
				     &username, &username_len,
				     &realm, &realm_len);

    TEST(username != NULL);
    TEST(strcmp((char *)username, "hello") == 0);
    TEST(username_len == strlen("hello"));

    TEST(realm == NULL);

    free(msg);
    if (tid != NULL) {
      free(tid);
      tid = NULL;
    }
  }


  {
    uint8_t *msg;
    stun_message_t stun_msg;
    stun_message_t stun_msg2;
    uint32_t len;
    uint ret_code;
    
    msg = stun_generate_allocate_request((uint8_t *)"hello", strlen("hello"), 
					 (uint8_t *)"password", strlen("password"), &tid, &len, 0);
    
    ret_code = stun_message_parse_with_integrity_check(msg, len, 
						       (uint8_t *)"password", strlen("password"), 
						       &stun_msg, false);
    TEST(ret_code == 0);
    TEST(stun_msg.stun_header.message_type == STUN_ALLOCATE_REQ);
    TEST(stun_msg.username.content != NULL);
    TEST(stun_msg.password.content == NULL);

    ret_code = stun_message_parse(msg, len, &stun_msg2, true);

    TEST(ret_code == 0);
    TEST(stun_message_integrity_check(msg, 
				      (uint8_t *)"password", strlen("password"), &stun_msg2));

    free(msg);
    if (tid != NULL) {
      free(tid);
      tid = NULL;
    }
  }

  {
    uint8_t *msg;
    uint32_t len;

    uint8_t *msg2;
    uint32_t len2;

    stun_message_t stun_msg;
    uint ret_code;

    msg = stun_generate_allocate_request(0, 0, 
					 0, 0, &tid, &len, 0);

    ret_code = stun_message_parse(msg, len, &stun_msg, false);

    TEST(ret_code == 0);

    msg2 = 
      stun_generate_error_response(&stun_msg, &len2, 431, "Integrity Check Failure");

    free(msg);
    free(msg2);
  }

  {
    uint8_t *msg;
    uint32_t len;

    uint8_t *msg2;
    uint32_t len2;

    stun_message_t stun_msg;
    uint ret_code;

    struct in_addr ad;
    stun_address_t addr;

    if (!inet_aton("192.168.10.53", &ad)) {
        printf("inet_aton failed");
        return;
    }
    addr.family = STUN_ADDRESS_FAMILY_IPV4;
    addr.port = 1024;
    addr.addr.ipv4_addr.s_addr = ad.s_addr;

    msg = stun_generate_connect_request(0,0,0,0, &addr, &tid, &len);

    ret_code = stun_message_parse(msg, len, &stun_msg, false);

    TEST(ret_code == 0);

    msg2 = 
      stun_generate_error_response(&stun_msg, &len2, 431, "Integrity Check Failure");

    free(msg);
    free(msg2);
  }
 
}

#if 0
static void
print_data(uint8_t* buf, uint32_t len)
{
  unsigned int i;
  printf("data (%d bytes):\n", len);
  for(i = 0; i < len; ++i) {
    printf("%02x ", buf[i]);
  }
  putchar('\n');
}
#endif


static void test_hmac(void)
{

  // this test comes from FIPS Pub 198, from the USA NIST. See:
  // http://csrc.nist.gov/publications/fips/fips198/fips-198a.pdf

#define PASSWD_LEN 64
#define HMAC_LEN 20

  char* text = "Sample #1";
  uint16_t text_len = strlen("Sample #1");

  uint8_t passwd[PASSWD_LEN];

  uint8_t hmac[HMAC_LEN];
  unsigned int hmac_len = HMAC_LEN;

  uint8_t comparison_hmac[HMAC_LEN] = 
    { 0x4f, 0x4c, 0xa3, 0xd5,   0xd6, 0x8b, 0xa7, 0xcc,
      0x0a, 0x12, 0x08, 0xc9,   0xc6, 0x1e, 0x9c, 0x5d,
      0xa0, 0x40, 0x3c, 0x0a, };


  int i;
  for(i = 0 ; i < PASSWD_LEN; ++i) {
    passwd[i] = i;
  }

  TEST(encode_message_integrity((uint8_t *)text, text_len, passwd, PASSWD_LEN,
				hmac, &hmac_len));

  TEST(memcmp(hmac, comparison_hmac, HMAC_LEN) == 0);

  //print_data(hmac, 32);
}


int main (int argc, char **argv)
{
  int c;

  while((c = getopt(argc, argv, "v")) > 0) {
    switch(c) {
    case 'v':
      verbose = true;
      break;
    default: 
      fprintf(stderr, "Usage: %s [-v]\n\n", argv[0]);
      exit(-1);
      break;
    }
  }

  printf("Testing...\n");
  parse_cycle();
  test_hmac();

  printf("Summary: %d passed, %d failed.\n", test_pass, test_fail);
  if(test_fail > 0) {
    printf("\n*** WARNING: tests failed ***\n\n");
    return -1;
  } else {
    printf("\nOK\n");
    return 0;
  }
}
  
  
