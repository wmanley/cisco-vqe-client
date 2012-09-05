/*------------------------------------------------------------------
 *
 * stun_hmac.c
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * This file is intended SOLELY for testing purposes within the stun
 * component.  It should not be used as a socket interface for any
 * other purpose.
 *
 *------------------------------------------------------------------
 */

#include "stun_private.h"

#define STUN_MAX_HMAC_LEN 20

/* 
 * The message integrity function is dependent on the presence of openssl;
 * openssl must be natively present for integrity checks to work. 
 */
#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/hmac.h>

bool encode_message_integrity(uint8_t* stun_msg, uint16_t len,
			      const uint8_t* key, uint16_t key_len,
			      uint8_t* hmac, unsigned int* hmac_len)
{
  HMAC_CTX context;
  const EVP_MD* md = EVP_sha1();

  HMAC_CTX_init(&context);

  HMAC_Init(&context, key, key_len, md);
  HMAC_Update(&context, stun_msg, len);
 
  /* 
   * we should not need to pad out to 64 byte boundary -- RFC 2104
   * says that HMACs should do this already.
   */

  if(hmac_len && *hmac_len < STUN_MAX_HMAC_LEN) {
    return false;
  }

  HMAC_Final(&context, hmac, hmac_len);
  HMAC_cleanup(&context);

  return true;
}


bool verify_message_integrity(const uint8_t* stun_msg, uint16_t len,
			      const uint8_t* key, uint16_t key_len,
			      const uint8_t* hmac, unsigned int hmac_len)
{
  HMAC_CTX context;
  const EVP_MD* md = EVP_sha1();
  uint8_t temp_hmac[STUN_MAX_HMAC_LEN];
  unsigned int tmp_hmac_len = STUN_MAX_HMAC_LEN;

  HMAC_CTX_init(&context);

  HMAC_Init(&context, key, key_len, md);
  HMAC_Update(&context, stun_msg, len);
 
  /*
   *  we should not need to pad out to 64 byte boundary -- RFC 2104
   * says that HMACs should do this already.
   */

  if(hmac_len < STUN_MAX_HMAC_LEN) {
    return false;
  }

  HMAC_Final(&context, temp_hmac, &tmp_hmac_len);
  HMAC_cleanup(&context);

  if(memcmp(temp_hmac, hmac, STUN_MAX_HMAC_LEN) == 0)
    return true;
  else
    return false;
}


#else /* HAVE_OPENSSL */

/* do not have OpenSSL */

bool encode_message_integrity(uint8_t* stun_msg, uint16_t len,
			      const uint8_t* key, uint16_t key_len,
			      uint8_t* hmac, unsigned int* hmac_len)
{
    if(!hmac || (hmac_len && *hmac_len < STUN_MAX_HMAC_LEN)) {
        return false;
    }
    if (strncpy((char *)hmac, "hmac-not-implemented",STUN_MAX_HMAC_LEN)) {
        *hmac_len = STUN_MAX_HMAC_LEN;
        hmac[STUN_MAX_HMAC_LEN-1] = '\0';  /* Assure null terminated */
        return true;
    } else {
        return false;
    }
}


bool verify_message_integrity(const uint8_t* stun_msg, uint16_t len,
			      const uint8_t* key, uint16_t key_len,
			      const uint8_t* hmac, unsigned int hmac_len)
{
    char temp_hmac[STUN_MAX_HMAC_LEN];
    strncpy(temp_hmac, "hmac-not-implemented",STUN_MAX_HMAC_LEN);
    temp_hmac[STUN_MAX_HMAC_LEN-1] = '\0';
    if (hmac && hmac_len >=STUN_MAX_HMAC_LEN && memcmp(temp_hmac, hmac, STUN_MAX_HMAC_LEN) == 0) {
        return true;
    } else {
        return false;
    }
}

#endif /* HAVE_OPENSSL */
