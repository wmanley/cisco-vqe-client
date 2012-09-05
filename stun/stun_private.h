#ifndef __STUN_PRIVATE_H__
#define __STUN_PRIVATE_H__

#include "stun.h"


// Message Attributes
#define STUN_MA_MAPPED_ADDRESS        0x0001
#define STUN_MA_RESPONSE_ADDRESS      0x0002
#define STUN_MA_CHANGE_REQUEST        0x0003
#define STUN_MA_SOURCE_ADDRESS        0x0004
#define STUN_MA_CHANGED_ADDRESS       0x0005
#define STUN_MA_USERNAME              0x0006
#define STUN_MA_PASSWORD              0x0007
#define STUN_MA_MESSAGE_INTEGRITY     0x0008
#define STUN_MA_ERROR_CODE            0x0009
#define STUN_MA_UNKNOWN_ATTRIBUTES    0x000a
#define STUN_MA_REFLECTED_FROM        0x000b
//#define STUN_MA_ALTERNATE_SERVER      0x000e
#define STUN_MA_REALM                 0x0014
#define STUN_MA_NONCE                 0x0015
#define STUN_MA_XOR_MAPPED_ADDRESS    0x0020
#define STUN_MA_ICE_PRIORITY          0x0024
#define STUN_MA_SERVER                0x8022
#define STUN_MA_ALTERNATE_SERVER      0x8023
#define STUN_MA_BINDING_LIFETIME      0x8024


// TURN Message Attributes
#define STUN_MA_LIFETIME              0x000d
#define STUN_MA_BANDWIDTH             0x0010
#define STUN_MA_DESTINATION_ADDRESS   0x0011
#define STUN_MA_REMOTE_ADDRESS        0x0012
#define STUN_MA_DATA                  0x0013
#define STUN_MA_RELAY_ADDRESS         0x0016
#define STUN_MA_REQUESTED_PORT        0x0018
#define STUN_MA_REQUESTED_TRANSPORT   0x0019
#define STUN_MA_REQUESTED_IP          0x0020
#define STUN_MA_TIMER_VAL             0x0021



typedef struct stun_tlv_t {
  uint16_t tlv_type;
  uint16_t tlv_length;
} stun_tlv_t;


bool encode_message_integrity(uint8_t* stun_msg, uint16_t len,
			      const uint8_t* key, uint16_t key_len,
			      uint8_t* hmac, unsigned int *hmac_len);

bool verify_message_integrity(const uint8_t* stun_msg, uint16_t len,
			      const uint8_t* key, uint16_t key_len,
			      const uint8_t* hmac, unsigned int hmac_len);


#endif
