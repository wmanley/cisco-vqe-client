/*------------------------------------------------------------------
 *
 * stun.h
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *------------------------------------------------------------------
 */

#ifndef __STUN_H__
#define __STUN_H__ 1

/** @defgroup stun */

#include "stun_includes.h"
#include "transaction_id.h"

// Message Types
#define STUN_BINDING_REQ                 0x0001
#define STUN_BINDING_RESP                0x0101
#define STUN_BINDING_ERROR_RESP          0x0111
#define STUN_SHARED_SECRET_REQ           0x0002
#define STUN_SHARED_SECRET_RESP          0x0102
#define STUN_SHARED_SECRET_ERROR_RESP    0x0112

// TURN message types

#define STUN_ALLOCATE_REQ                0x0003
#define STUN_ALLOCATE_RESP               0x0103
#define STUN_ALLOCATE_ERROR_RESP         0x0113
				        
#define STUN_SEND_INDICATION             0x0004
#define STUN_DATA_INDICATION             0x0115
  
#define STUN_SET_ACTIVE_DEST_REQ         0x0006
#define STUN_SET_ACTIVE_DEST_RESP        0x0106
#define STUN_SET_ACTIVE_DEST_ERROR_RESP  0x0116

#define STUN_CONNECT_REQ                 0x0007
#define STUN_CONNECT_RESP                0x0107
#define STUN_CONNECT_ERROR_RESP          0x0117

#define STUN_REQUESTED_TRANSPORT_UDP     0x0000
#define STUN_REQUESTED_TRANSPORT_TCP     0x0001

#define STUN_REQUESTED_PORT_EVEN           0x0000
#define STUN_REQUESTED_PORT_ODD            0x0001
#define STUN_REQUESTED_PORT_EVEN_HOLD_NEXT 0x0002
#define STUN_REQUESTED_PORT_SPECIFIC       0x0003




/** @ingroup stun 
    @brief DOCME
*/
typedef struct stun_header_t {
  uint16_t  message_type;
  uint16_t  message_length;
  uint32_t  magic_cookie;
  uint8_t   transaction_id[12];
} stun_header_t;
#define STUN_MAGIC_COOKIE  0x2112a442

#define STUN_ADDRESS_FAMILY_IPV4  0x01
#define STUN_ADDRESS_FAMILY_IPV6  0x02

/** @ingroup stun 
    @brief DOCME
*/
typedef struct stun_address_t {
  uint8_t pad;
  uint8_t family;
  uint16_t port;  // will be in host order, except in outgoing messages
  union {
    struct in_addr ipv4_addr;
    struct in6_addr ipv6_addr;
  } addr;
} stun_address_t;

#define STUN_ADDRESS_IPV4_LEN (1 + 1 + 2 + 4)
#define STUN_ADDRESS_IPV6_LEN (1 + 1 + 2 + 16)

/** @ingroup stun 
    @brief DOCME
*/
typedef struct stun_msg_uint8_ptr_t {
  const uint8_t *content;
  uint16_t length;
} stun_msg_uint8_ptr_t;

/** @ingroup stun 
    @brief DOCME
*/
typedef struct stun_msg_uint32_t {
  bool is_valid;
  uint32_t content;
} stun_msg_uint32_t;

/** @ingroup stun 
    @brief DOCME
*/
typedef struct stun_msg_address_t {
  bool is_valid;
  stun_address_t content;
} stun_msg_address_t;

/** @ingroup stun 
    @brief DOCME
*/
typedef struct stun_message_t {
  stun_header_t stun_header;

  bool address_is_xor_mapped;
  stun_msg_address_t mapped_address;

  stun_msg_address_t response_address;

  stun_msg_uint32_t change_request;

  stun_msg_address_t changed_address;

  stun_msg_address_t source_address;

  stun_msg_uint8_ptr_t realm;

  stun_msg_uint8_ptr_t nonce;

  stun_msg_uint8_ptr_t username;

  stun_msg_uint8_ptr_t password;

  bool have_error_code;
  uint16_t error_code;
  const char *error_reason;

  stun_msg_uint8_ptr_t server;

  stun_msg_address_t alternate_address;
  
  stun_msg_uint32_t binding_lifetime;

  stun_msg_uint32_t ice_priority;

  /* these are TURN items */

  stun_msg_uint32_t lifetime;

  stun_msg_uint32_t bandwidth;

  stun_msg_uint32_t timer_val;

  stun_msg_address_t destination_address;

  stun_msg_address_t requested_address;

  stun_msg_uint32_t requested_transport; /* enum */

  bool have_requested_port;
  uint16_t requested_port_property;
  uint16_t requested_port_filter;

  stun_msg_address_t relay_address;

  /** @brief encapsulated data for SEND and DATA indications. */
  stun_msg_uint8_ptr_t data_attribute;

  stun_msg_uint8_ptr_t hmac;
  uint hmac_offset;
} stun_message_t;

#ifdef __cplusplus
extern "C" {
#endif

  /** @brief check the stun header of the message whether the pointed message 
             is a stun binding request. note the message still needs to be 
             parsed to be sure that it is a valid stun message.
             
      @ingroup stun

      @param[in] message   pointer to message buffer.
      @param[in] length    length of message.
      @retval true if the message has passed the STUN 
      @retval false if there message integrity failed.
  */
bool is_stun_binding_req_message(const uint8_t *message,
                                 uint32_t length);


  /** @brief parse a stun message
      @ingroup stun

      @param[in] message   pointer to message buffer.
      @param[in] length    length of message.
      @param[in] response  the parsed message.
      @param[in] require_magic_cookie   allow non-rfc3489bis draft messages.
      @retval 0 if successfully parsed.
      @retval 400 if there was a parse error.
  */
uint stun_message_parse(const uint8_t *message, 
			uint32_t length, 
			stun_message_t *response, 
			bool require_magic_cookie);


  /** @brief verify integrity.
      @ingroup stun

      @param[in] orig_message   pointer to message buffer.
      @param[in] password       password to compare.
      @param[in] password_len   length of password.
      @param[in] response       parsed message.
      @retval true if message integrity verified, 
             @retval false if there message integrity failed.
  */
bool stun_message_integrity_check(const uint8_t *orig_message,
				  const uint8_t *password, 
				  uint32_t password_len,
				  stun_message_t *response);


  /** @brief parse and verify integrity.
      @ingroup stun

      @param[in] message   pointer to message buffer.
      @param[in] length   length of the message buffer.
      @param[in] password       password to compare.
      @param[in] password_len   length of password.
      @param[in] response       parsed message.
      @param[in] require_magic_cookie   allow non-rfc3489bis draft messages.
      @retval 0 if successfully parsed.
      @retval 400 if there was a parse error.
  */
       
uint stun_message_parse_with_integrity_check(const uint8_t *message, 
					     uint32_t length, 
					     uint8_t *password, 
					     uint32_t password_len, 
					     stun_message_t *response, 
					     bool require_magic_cookie);

  /** @brief find the username and realm in the given STUN message.
      @ingroup stun

      @param[in] message   pointer to message buffer.
      @param[in] length   length of message buffer.
      @param[out] username       username in the message.
      @param[out] username_len   length of username.
      @param[out] realm       realm in the message.
      @param[out] realm_len   length of realm.
  */

void stun_message_find_username_realm(uint8_t *message, uint32_t length, 
				      uint8_t **username,
				      uint32_t *username_len,
				      uint8_t **realm, 
				      uint32_t *realm_len);

  /** @brief verify that the transaction ID is the same.
      @ingroup stun

      @param[in] tid   pointer to message buffer.
      @param[in] message   pointer to message buffer.

      @retval true if transaction ID the same.
      @retval false if transaction ID differs.
  */

bool stun_message_match_transaction_id(transaction_id_t *tid, 
				       uint8_t *message);
  

  /** @brief generate STUN Binding Request.
      @ingroup stun

      @param[in] username        username for message integrity, or NULL for no username.
      @param[in] username_len       length of username.
      @param[in] password       password for message integrity generation, or NULL to not do message integrity.
      @param[in] password_len   length of password.
      @param[in] priority       ICE priorty (0 if not used).
      @param[out] tid  transaction ID.
      @param[out] return_msg_len  length of message returned.

      @return pointer to message, or NULL if failed.
  */

uint8_t *stun_generate_binding_request(const uint8_t *username, 
				       uint32_t username_len,
				       const uint8_t *password, 
				       uint32_t password_len,
				       uint32_t priority,
				       transaction_id_t **tid,
				       uint32_t *return_msg_len);

  /** @brief generate STUN Relay Allocate request.
      @ingroup stun_relay

      @param[in] username        username for message integrity, or NULL for no username.
      @param[in] username_len       length of username.
      @param[in] password       password for message integrity generation, or NULL to not do message integrity.
      @param[in] password_len   length of password.
      @param[out] tid  transaction ID.
      @param[out] return_msg_len  length of message returned.
      @param[in] transport    Request either a UDP or TCP address.
      @return pointer to message, or NULL if failed.
  */

uint8_t *stun_generate_allocate_request(uint8_t *username, 
					uint32_t username_len,
					uint8_t *password, 
					uint32_t password_len,
					transaction_id_t **tid,
					uint32_t *return_msg_len,
					int transport);

  /** @brief Generate a STUN relay DATA indication.  This version copies the data, rather than using an iov, for simplicity.

      @ingroup stun

      @param[in] data_buffer   pointer to message buffer.
      @param[in] data_len       password to compare.
      @param[in] destination_address   length of password.
      @param[out] return_msg_len       length of generated message.
      @retval a pointer to the generated message, or NULL if generation failed.
  */
uint8_t *stun_generate_data_indication(uint8_t *data_buffer,
				       uint32_t data_len,
				       stun_address_t* destination_address,
				       uint32_t *return_msg_len);

  /** @brief generate a STUN relay SEND indication.  This version copies the data, rather than using an iov, for simplicity.

      @ingroup stun

      @param[in] data_buffer   pointer to message buffer.
      @param[in] data_len       password to compare.
      @param[in] destination_address   length of password.
      @param[out] return_msg_len       length of generated message.

      @retval a pointer to the generated message, or NULL if generation failed.
  */
uint8_t *stun_generate_send_indication(uint8_t *data_buffer,
				       uint32_t data_len,
				       stun_address_t* destination_address,
				       uint32_t *return_msg_len);


  /** @brief generate STUN Binding Response.
      @ingroup stun

      @param[in] msg   pointer to STUN message to encode.
      @param[out] return_msg_len       length of message returned.

      @return pointer to message, or NULL if failed.
  */

uint8_t *stun_generate_binding_response(stun_message_t *msg,
					uint32_t *return_msg_len);

  /** @brief generate STUN Relay Allocate Response.
      @ingroup stun

      @param[in] msg   pointer to STUN message to encode.
      @param[out] return_msg_len       length of message returned.

      @return pointer to message, or NULL if failed.
  */

uint8_t *stun_generate_allocate_response(stun_message_t *msg,
					 uint32_t *return_msg_len);

  /** @brief generate STUN Relay Connect Response.
      @ingroup stun

      @param[in] msg   pointer to STUN message to encode.
      @param[out] return_msg_len       length of message returned.

      @return pointer to message, or NULL if failed.
  */

uint8_t *stun_generate_connect_response(stun_message_t *msg,
					uint32_t *return_msg_len);

  /** @brief generate STUN Relay Set Active Destination Response.
      @ingroup stun

      @param[in] msg   pointer to STUN message to encode.
      @param[out] return_msg_len       length of message returned.

      @return pointer to message, or NULL if failed.
  */


uint8_t *stun_generate_set_active_destination_response(stun_message_t *msg,
						       uint32_t *return_msg_len);

  /** @brief generate a generic STUN Error Response.
      @ingroup stun

      @param[in] msg   pointer to STUN message to encode.
      @param[out] return_msg_len       length of message returned.
      @param[in] error_code   error code for error response.
      @param[in] reason    C string of human-readable reason for error.

      @return pointer to message, or NULL if failed.
  */


uint8_t *stun_generate_error_response(stun_message_t *msg,
				      uint32_t *return_msg_len,
				      int error_code,
				      const char* reason);


  /** @brief generate STUN Binding Error  Response.
      @ingroup stun

      @param[in] msg   pointer to STUN message to encode.
      @param[out] return_msg_len       length of message returned.

      @return pointer to message, or NULL if failed.
  */

uint8_t *stun_generate_binding_error_response(stun_message_t *msg,
					      uint32_t *return_msg_len);

  /** @brief generate STUN Shared Secret Request.
      @ingroup stun

      @param[out] return_msg_len       length of message returned.

      @return pointer to message, or NULL if failed.
  */
uint8_t *stun_generate_shared_secret_request(uint32_t *return_msg_len);

  /** @brief generate STUN Shared Secret Response.
      @ingroup stun

      @param[in] msg   pointer to STUN message to encode.
      @param[out] return_msg_len       length of message returned.

      @return pointer to message, or NULL if failed.
  */

uint8_t *stun_generate_shared_secret_response(stun_message_t *msg, 
					      uint32_t *return_msg_len);

  /** @brief generate STUN Relay Set Active Destination request.
      @ingroup stun_relay

      @param[in] username        username for message integrity, or NULL for no username.
      @param[in] username_len       length of username.
      @param[in] password       password for message integrity generation, or NULL to not do message integrity.
      @param[in] password_len   length of password.
      @param[in] destination_address  address of destination to set as active destination to.
      @param[out] tid  transaction ID.
      @param[out] return_msg_len  length of message returned.

      @return pointer to message, or NULL if failed.
  */

uint8_t *stun_generate_set_active_destination_request (uint8_t *username, 
						       uint32_t username_len,
						       uint8_t *password, 
						       uint32_t password_len,
						       stun_address_t* destination_address,
						       transaction_id_t **tid,
						       uint32_t *return_msg_len);

  /** @brief generate STUN Relay Connect request.
      @ingroup stun_relay

      @param[in] username        username for message integrity, or NULL for no username.
      @param[in] username_len       length of username.
      @param[in] password       password for message integrity generation, or NULL to not do message integrity.
      @param[in] password_len   length of password.
      @param[in] destination_address  address of destination to connect to.
      @param[out] tid  transaction ID.
      @param[out] return_msg_len  length of message returned.

      @return pointer to message, or NULL if failed.
  */

uint8_t *stun_generate_connect_request (uint8_t *username, 
					uint32_t username_len,
					uint8_t *password, 
					uint32_t password_len,
					stun_address_t* destination_address,
					transaction_id_t **tid,
					uint32_t *return_msg_len);

  /** @brief return a human-readable string for the type of a STUN message.
      @ingroup stun

      @param[in] msg   pointer to message buffer.

      @return pointer to C string of name (suitable for output)
  */
const char *stun_msg_type(stun_message_t *msg);


  /** @brief  compare incoming stun address/port with a stored one
      @ingroup stun

      @param[in] left        Address within a Stun Message (host order)
      @param[in] right       Address in sockaddr form (network order)
      @param[in] right_len   Length of address

      @retval true if they are equal (address, address type, port),
      @retval false if not equal.
  */
  bool compare_stun_address(const stun_address_t *left, 
			    struct sockaddr *right,
			    socklen_t right_len);

  /** @brief  DOCME
      @ingroup stun

      @param[in] from DOCME
      @param[out] to DOCME

  */
void stun_to_sock_addr(const stun_address_t* from, 
		       struct sockaddr_storage* to);

  /** @brief  DOCME
      @ingroup stun

      @param[in] from   pointer to message buffer.
      @param[out] to       password to compare.
  */

void sock_to_stun_addr(const struct sockaddr_storage* from,
		       stun_address_t* to);

  /** @brief  DOCME
      @ingroup stun

      @param[in] message   pointer to message buffer.
      @param[in] length       length of message.

      @retval uint length of STUN message needed.
  */

uint stun_message_length (uint8_t *message, uint32_t length);


void stun_uint32t_set(struct stun_msg_uint32_t *msg, uint32_t value);


#ifdef __cplusplus
}
#endif
#endif
