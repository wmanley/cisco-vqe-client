/*------------------------------------------------------------------
 *
 * stun_msg.c
 *
 * Copyright (c) 2007 by Cisco Systems, Inc.
 * All rights reserved.
 *
 *------------------------------------------------------------------
 */

#include <assert.h>
#include "stun_private.h"
#include "transaction_id.h"

char *server_string = "Cisco STUN serve";

static uint32_t stun_message_address_size (stun_msg_address_t *addr)
{
  if (addr->content.family == STUN_ADDRESS_FAMILY_IPV4) 
    return 8;
  return 20;
}

static uint32_t stun_message_string_size (const char *string)
{
  uint str_len;
  if (string == NULL) return 0;
  str_len = strlen(string);
  str_len += 3;
  str_len /= 4;
  str_len *= 4; // pad to 4;
  return str_len;
}

static uint32_t check_and_add_size_uint8ptr(stun_msg_uint8_ptr_t* value)
{
  if(value != NULL && value->content != NULL) {
    return sizeof(stun_tlv_t) + (((value->length + 3) / 4) * 4);
  }
  return 0;
}

static uint32_t check_and_add_size_address(stun_msg_address_t* address)
{
  if(address != NULL && address->is_valid) {
    return sizeof(stun_tlv_t) + stun_message_address_size(address);
  }
  return 0;
}

static uint32_t check_and_add_size_uint32_t(stun_msg_uint32_t* value)
{
  if(value != NULL && value->is_valid) {
    return sizeof(stun_tlv_t) + 4;
  }
  return 0;
}


static uint32_t stun_message_calculate_size (stun_message_t *msg)
{
  uint32_t len = sizeof(stun_header_t);

  switch (msg->stun_header.message_type) {
  case STUN_BINDING_REQ:
    // we'll only generate 3489bis
    len += check_and_add_size_uint8ptr(&msg->username);
    len += check_and_add_size_uint32_t(&msg->ice_priority);
    break;

  case STUN_BINDING_RESP:
    // we always have either a MAPPED-ADDRESS, or XOR_MAPPED_ADDRESS
    len += sizeof(stun_tlv_t) + stun_message_address_size(&msg->mapped_address);
    if (msg->stun_header.magic_cookie == STUN_MAGIC_COOKIE) {
      // newer version
      len += check_and_add_size_uint8ptr(&msg->server);
      if (msg->binding_lifetime.is_valid) {
	len += sizeof(uint32_t) + sizeof(stun_tlv_t);
      }
    } else {
      // older version
      len += check_and_add_size_address(&msg->changed_address);
      len += check_and_add_size_address(&msg->source_address);
    }
    break;

  case STUN_ALLOCATE_REQ:
    // we'll only generate 3489bis
    len += check_and_add_size_uint8ptr(&msg->username);
    break;

  case STUN_ALLOCATE_RESP:
    // dodgy

    if (msg->stun_header.magic_cookie == STUN_MAGIC_COOKIE) {
      // newer version
      len += check_and_add_size_uint8ptr(&msg->server);
    }
    len += check_and_add_size_address(&msg->relay_address);
    len += check_and_add_size_address(&msg->mapped_address);
    len += check_and_add_size_uint32_t(&msg->bandwidth);
    len += check_and_add_size_uint32_t(&msg->lifetime);
    break;

  case STUN_CONNECT_ERROR_RESP:
  case STUN_ALLOCATE_ERROR_RESP:
  case STUN_BINDING_ERROR_RESP:
  case STUN_SET_ACTIVE_DEST_ERROR_RESP:
    if (msg->stun_header.magic_cookie == STUN_MAGIC_COOKIE) {
      // newer version
      len += check_and_add_size_uint8ptr(&msg->server);
    } 
    len += sizeof(stun_tlv_t); // error code attribute
    len += 4; // error code
    len += stun_message_string_size(msg->error_reason);
    break;

  case STUN_SHARED_SECRET_REQ:
    // no additional fields
    break; 


  case STUN_SET_ACTIVE_DEST_REQ:
    // we'll only generate 3489bis
    len += check_and_add_size_uint8ptr(&msg->username);
    len += check_and_add_size_address(&msg->destination_address);
    break;

  case STUN_SET_ACTIVE_DEST_RESP:
    // dodgy

    if (msg->stun_header.magic_cookie == STUN_MAGIC_COOKIE) {
      // newer version
      len += check_and_add_size_uint8ptr(&msg->server);
    }

    len += check_and_add_size_address(&msg->relay_address);
    len += check_and_add_size_address(&msg->mapped_address);
    len += check_and_add_size_uint32_t(&msg->bandwidth);
    len += check_and_add_size_uint32_t(&msg->lifetime);
    break;



  case STUN_CONNECT_REQ:
    // we'll only generate 3489bis
    len += check_and_add_size_uint8ptr(&msg->username);
    len += check_and_add_size_address(&msg->destination_address);
    break;

  case STUN_CONNECT_RESP:
    // dodgy

    if (msg->stun_header.magic_cookie == STUN_MAGIC_COOKIE) {
      // newer version
      len += check_and_add_size_uint8ptr(&msg->server);
    }

    len += check_and_add_size_address(&msg->relay_address);
    len += check_and_add_size_address(&msg->mapped_address);
    len += check_and_add_size_uint32_t(&msg->bandwidth);
    len += check_and_add_size_uint32_t(&msg->lifetime);
    break;



  case STUN_DATA_INDICATION:
  case STUN_SEND_INDICATION:
    len += check_and_add_size_address(&msg->destination_address);
    len += check_and_add_size_uint8ptr(&msg->data_attribute);
    break;


  case STUN_SHARED_SECRET_RESP:
    // need username and password
    if (msg->stun_header.magic_cookie == STUN_MAGIC_COOKIE) {
      // newer version
      len += check_and_add_size_uint8ptr(&msg->server);
    }
    len += check_and_add_size_uint8ptr(&msg->username);
    len += check_and_add_size_uint8ptr(&msg->password);
    return len; // no message integrity
    break;

  default:
    assert(0);
    printf("WARNING: invalid message type! -- fix stun_message_calculate_size\n");
    exit(0);
    break;
  }

  // Do not add any globals fields other than message integrity here
  if (msg->password.content != NULL) {
    len += sizeof(stun_tlv_t); // message integrity
    len += 20; // size of hmac
  }
  return len;
}




/* this version copies and converts host to network order for the port */
static uint stun_copy_address_to_msg (stun_address_t *to,
				      stun_address_t *from)
{
  to->pad = 0;
  to->family = from->family;
  to->port = htons(from->port);
  if (from->family == STUN_ADDRESS_FAMILY_IPV4) {
    to->addr.ipv4_addr = from->addr.ipv4_addr;
    return STUN_ADDRESS_IPV4_LEN;
  } 
  to->addr.ipv6_addr = from->addr.ipv6_addr;
  return STUN_ADDRESS_IPV6_LEN;
}


/* this version does NOT do host to network order conversion of the port */
static uint stun_copy_address_to_msg_nohn (stun_msg_address_t *to,
					   stun_address_t *from)
{
  to->is_valid = true;
  to->content.pad = 0;
  to->content.family = from->family;
  to->content.port = from->port;
  if (from->family == STUN_ADDRESS_FAMILY_IPV4) {
    to->content.addr.ipv4_addr = from->addr.ipv4_addr;
    return STUN_ADDRESS_IPV4_LEN;
  } 
  to->content.addr.ipv6_addr = from->addr.ipv6_addr;
  return STUN_ADDRESS_IPV6_LEN;
}




static inline int encode_uint32_t(uint8_t* body, uint16_t type, stun_msg_uint32_t value)
{
  stun_tlv_t *tlv;
  if(value.is_valid) {
    tlv = (stun_tlv_t *)body;
    tlv->tlv_type = htons(type);
    tlv->tlv_length = htons(sizeof(uint32_t));
    body += sizeof(stun_tlv_t);
    *(uint32_t *)body = htonl(value.content);
    body += sizeof(uint32_t);
    return sizeof(stun_tlv_t) + sizeof(uint32_t);
  }
  return 0;
}

static inline int encode_msg_address_t(uint8_t* body, uint16_t type, stun_msg_address_t* value)
{
  stun_tlv_t *tlv;
  uint ret_len;
  stun_address_t *msg_addr;

  if(value == NULL || value->is_valid == false)
    return 0;

  tlv = (stun_tlv_t *)body;
  msg_addr = (stun_address_t *)(body + sizeof(stun_tlv_t));
  tlv->tlv_type = htons(type);
  ret_len = stun_copy_address_to_msg(msg_addr, &value->content);
  tlv->tlv_length = htons(ret_len);
  return ret_len + sizeof(stun_tlv_t);
}


static inline int encode_uint8ptr(uint8_t* body, uint16_t type, 
				  const uint8_t *from, uint32_t len)
{
  stun_tlv_t *tlv;
  uint32_t ix;

  if (from != NULL) {
    tlv = (stun_tlv_t *)body;
    tlv->tlv_type = htons(type);
    tlv->tlv_length = htons(len);
    body += sizeof(stun_tlv_t);
    ix = ((len + 3) / 4) * 4;
    memcpy(body, from, len);

    /* purely to make valgrind happy */
    if(ix > len) {
      memset(body + len, 0, ix - len);
    }
    body += ix;
    return sizeof(stun_tlv_t) + ix;
  }
  return 0;
}


static inline int encode_uint8_ptr_t (uint8_t* body, uint16_t type, 
				      stun_msg_uint8_ptr_t* value)
{
  return encode_uint8ptr(body, type, value->content, value->length);
}

static uint32_t stun_encode_message_integrity(uint8_t* stun_msg, 
					      uint32_t stun_msg_len, 
					      const uint8_t* password,
					      uint32_t password_len,
					      uint8_t* body_ptr)
{
  uint32_t length_no_integrity = stun_msg_len - (sizeof(stun_tlv_t) + 20);

#define STUN_HMAC_LEN 32
  uint8_t hmac[STUN_HMAC_LEN];
  unsigned int hmac_len = STUN_HMAC_LEN;

  if (password != NULL && password_len > 0) {
    if(encode_message_integrity(stun_msg, length_no_integrity, 
				password, password_len,
				hmac, &hmac_len)) {
      return encode_uint8ptr(body_ptr, STUN_MA_MESSAGE_INTEGRITY,
			     hmac, hmac_len);
    } else {
      // there was some HMAC error here.
      return 0;
    }
  }
  return 0;
}


static
void stun_generate_request(stun_message_t* msg,
			   uint16_t message_type,
			   const uint8_t *username, uint32_t username_len,
			   const uint8_t *password, uint32_t password_len)
{
  memset(msg, 0, sizeof(stun_message_t));

  msg->stun_header.message_type = message_type;
  msg->stun_header.magic_cookie = STUN_MAGIC_COOKIE;

  msg->username.content = username;
  msg->username.length = username_len;

  msg->password.content = password;
  msg->password.length = password_len;
}


static
uint8_t*
prepare_stun_msg (stun_message_t* msg,
		  transaction_id_t **tid,
		  uint32_t *return_msg_len,
		  bool generate_tid)

{
  uint8_t *buffer;
  stun_header_t *header;
  *return_msg_len = stun_message_calculate_size(msg);

  if(tid) {
    if (*tid == NULL) {
      *tid = transaction_id_create();
      if (*tid == NULL) {
	return NULL;
      }
    }
  }

  buffer = (uint8_t *)malloc(*return_msg_len);
  if (buffer == NULL) {
    *return_msg_len = 0;
    return NULL;
  }
 
  header = (stun_header_t *)buffer;
  header->message_type = htons(msg->stun_header.message_type);
  header->message_length = htons(*return_msg_len - sizeof(stun_header_t));
  header->magic_cookie = htonl(msg->stun_header.magic_cookie);

  if(generate_tid) {
    transaction_id_generate(header->transaction_id);
  } else {
    if(tid) {
      memcpy(header->transaction_id, (*tid)->tid, 12);
    } else {
      memcpy(header->transaction_id, 
	     msg->stun_header.transaction_id,
	     12);
    }
  }
  return buffer;
}


uint8_t *stun_generate_binding_request (const uint8_t *username, 
					uint32_t username_len,
					const uint8_t *password, 
					uint32_t password_len,
					uint32_t priority,
					transaction_id_t **tid,
					uint32_t *return_msg_len)
{
  stun_message_t msg;
  uint8_t *buffer;
  uint8_t *body;

  if (password != NULL && username == NULL) return NULL;

  stun_generate_request(&msg, STUN_BINDING_REQ,
			username, username_len,
			password, password_len);


  if (priority != 0) {
    msg.ice_priority.is_valid = true;
    msg.ice_priority.content = priority;
  }

  buffer = prepare_stun_msg(&msg, tid, return_msg_len, false);
  if (buffer == NULL || *return_msg_len == sizeof(stun_header_t)) {
    return buffer;
  }

  // header is complete.
  body = buffer + sizeof(stun_header_t);
  memset(body, 0, *return_msg_len - sizeof(stun_header_t));
  
  body += encode_uint8ptr(body, STUN_MA_USERNAME, username, username_len);
  body += encode_uint32_t(body, STUN_MA_ICE_PRIORITY, msg.ice_priority);
  body += stun_encode_message_integrity(buffer, *return_msg_len,
					password, password_len,
					body);
  return buffer;
}


uint8_t *stun_generate_allocate_request (uint8_t *username, uint32_t username_len,
					 uint8_t *password, uint32_t password_len,
					 transaction_id_t **tid,
					 uint32_t *return_msg_len,
					 int transport)
{
  stun_message_t msg;
  uint8_t *buffer;
  uint8_t *body;

  if (password != NULL && username == NULL) return NULL;

  stun_generate_request(&msg, STUN_ALLOCATE_REQ,
			username, username_len,
			password, password_len);

  buffer = prepare_stun_msg(&msg, tid, return_msg_len, false);
  if (buffer == NULL || *return_msg_len == sizeof(stun_header_t)) {
    return buffer;
  }

  // header is complete.
  body = buffer + sizeof(stun_header_t);
  memset(body, 0, *return_msg_len - sizeof(stun_header_t));
  
  body += encode_uint8ptr(body, STUN_MA_USERNAME, username, username_len);

  body += stun_encode_message_integrity(buffer, *return_msg_len,
					password, password_len,
					body);

  return buffer;
}


uint8_t *stun_generate_set_active_destination_request (uint8_t *username, 
						       uint32_t username_len,
						       uint8_t *password, 
						       uint32_t password_len,
						       stun_address_t* destination_address,
						       transaction_id_t **tid,
						       uint32_t *return_msg_len)
{
  stun_message_t msg;
  uint8_t *buffer;
  uint8_t *body;

  if (password != NULL && username == NULL) return NULL;

  stun_generate_request(&msg, STUN_SET_ACTIVE_DEST_REQ,
			username, username_len,
			password, password_len);

  if(destination_address != NULL) {
    /*sa_ignore {no use for return value} IGNORE_RETURN (1) */
    stun_copy_address_to_msg_nohn(&msg.destination_address, destination_address);
  }

  buffer = prepare_stun_msg(&msg, tid, return_msg_len, false);
  if (buffer == NULL || *return_msg_len == sizeof(stun_header_t)) {
    return buffer;
  }
  
  // header is complete.
  body = buffer + sizeof(stun_header_t);
  memset(body, 0, *return_msg_len - sizeof(stun_header_t));

  body += encode_uint8ptr(body, STUN_MA_USERNAME,
			  username, username_len);

  body += encode_msg_address_t(body, STUN_MA_DESTINATION_ADDRESS, 
			       &msg.destination_address);

  body += stun_encode_message_integrity(buffer, *return_msg_len,
					password, password_len,
					body);

  return buffer;
}


uint8_t *stun_generate_connect_request (uint8_t *username, 
					uint32_t username_len,
					uint8_t *password, 
					uint32_t password_len,
					stun_address_t* destination_address,
					transaction_id_t **tid,
					uint32_t *return_msg_len)
{
  stun_message_t msg;
  uint8_t *buffer;
  uint8_t *body;

  if (password != NULL && username == NULL) return NULL;

  stun_generate_request(&msg, STUN_CONNECT_REQ,
			username, username_len,
			password, password_len);

  if(destination_address != NULL) {
    /*sa_ignore {no use for return value} IGNORE_RETURN (1) */
    stun_copy_address_to_msg_nohn(&msg.destination_address,destination_address);
  }

  buffer = prepare_stun_msg(&msg, tid, return_msg_len, false);
  if (buffer == NULL || *return_msg_len == sizeof(stun_header_t)) {
    return buffer;
  }
  
  // header is complete.
  body = buffer + sizeof(stun_header_t);
  memset(body, 0, *return_msg_len - sizeof(stun_header_t));

  body += encode_uint8ptr(body, STUN_MA_USERNAME,
			  username, username_len);

  body += encode_msg_address_t(body, STUN_MA_DESTINATION_ADDRESS, 
			       &msg.destination_address);

  body += stun_encode_message_integrity(buffer, *return_msg_len,
					password, password_len,
					body);

  return buffer;
}


static uint8_t *stun_generate_response (stun_message_t *msg,
					uint32_t *return_msg_len)
{
  uint8_t *buffer;
  uint8_t *body;
  stun_tlv_t *tlv;
  stun_address_t *msg_addr;
  uint32_t ix;
  uint ret_len;
  stun_header_t *header;

  //if (msg->mapped_address.is_valid == false) return NULL;

  if (msg->server.content == NULL) {
    msg->server.content = (uint8_t *)server_string;
    msg->server.length = strlen(server_string);
  }

  buffer = prepare_stun_msg(msg, 0, return_msg_len, false);

  if(buffer == NULL)
    return NULL;

  header = (stun_header_t*)buffer;
  body = buffer + sizeof(stun_header_t);
  memset(body, 0, *return_msg_len - sizeof(stun_header_t));

  
  if (msg->stun_header.magic_cookie != STUN_MAGIC_COOKIE) {
    // we need to include mapped address and source address
    body += encode_msg_address_t(body, STUN_MA_MAPPED_ADDRESS, 
				 &msg->mapped_address);

    body += encode_msg_address_t(body, STUN_MA_SOURCE_ADDRESS, 
				 &msg->source_address);
  } else {
    if(msg->mapped_address.is_valid) {
      if(msg->stun_header.message_type == STUN_BINDING_RESP) {
	// generate XOR_MAPPED_ADDRESS
	tlv = (stun_tlv_t *)body;
	msg_addr = (stun_address_t *)(body + sizeof(stun_tlv_t));
	tlv->tlv_type = htons(STUN_MA_XOR_MAPPED_ADDRESS);
	ret_len = stun_copy_address_to_msg(msg_addr, &msg->mapped_address.content);
	tlv->tlv_length = htons(ret_len);
	// do the xor
	msg_addr->port ^= htons(STUN_MAGIC_COOKIE >> 16);
	if (msg_addr->family == STUN_ADDRESS_FAMILY_IPV4) {
	  msg_addr->addr.ipv4_addr.s_addr ^= header->magic_cookie;
	} else {
	  uint8_t *xor_value = (uint8_t *)&header->magic_cookie;
	  for (ix = 0; ix < 16; ix++) {
	    msg_addr->addr.ipv6_addr.s6_addr[ix] ^= xor_value[ix];
	  }
	}
	body += ret_len + sizeof(stun_tlv_t);
      } else {
	// regular MAPPED_ADDRESS, please (e.g. ALLOCATE)
	body += encode_msg_address_t(body, STUN_MA_MAPPED_ADDRESS, 
				     &msg->mapped_address);
      }
    }

    body += encode_msg_address_t(body, STUN_MA_RELAY_ADDRESS, 
				 &msg->relay_address);
    body += encode_uint32_t(body, STUN_MA_LIFETIME, msg->lifetime);
    body += encode_uint32_t(body, STUN_MA_BANDWIDTH, msg->bandwidth);
    body += encode_uint8ptr(body, STUN_MA_SERVER,
			    msg->server.content, msg->server.length);
    body += encode_uint32_t(body, STUN_MA_BINDING_LIFETIME, 
			    msg->binding_lifetime);
    body += encode_uint32_t(body, STUN_MA_TIMER_VAL, msg->timer_val);
  }
  body += stun_encode_message_integrity(buffer, *return_msg_len,
					msg->password.content, msg->password.length,
					body);

  return buffer;
}


uint8_t *stun_generate_binding_response (stun_message_t *msg,
					 uint32_t *return_msg_len)
{
  msg->stun_header.message_type = STUN_BINDING_RESP;
  return stun_generate_response(msg, return_msg_len);
}

uint8_t *stun_generate_allocate_response (stun_message_t *msg,
					  uint32_t *return_msg_len)
{
  msg->stun_header.message_type = STUN_ALLOCATE_RESP;
  return stun_generate_response(msg, return_msg_len);
}

uint8_t *stun_generate_connect_response (stun_message_t *msg,
					 uint32_t *return_msg_len)
{
  msg->stun_header.message_type = STUN_CONNECT_RESP;
  return stun_generate_response(msg, return_msg_len);
}


uint8_t *stun_generate_set_active_destination_response (stun_message_t *msg,
							uint32_t *return_msg_len)
{
  msg->stun_header.message_type = STUN_SET_ACTIVE_DEST_RESP;
  return stun_generate_response(msg, return_msg_len);
}


uint8_t *stun_generate_error_response (stun_message_t *msg,
				       uint32_t *return_msg_len,
				       int error_code,
				       const char* reason)
{
  uint8_t *buffer;
  uint8_t *body;
  stun_tlv_t *tlv;
  uint ix;
  uint str_len;
  uint16_t msg_type;

  switch(msg->stun_header.message_type) {
  case STUN_BINDING_REQ:
    msg_type = STUN_BINDING_ERROR_RESP;
    break;
  case STUN_ALLOCATE_REQ:
    msg_type = STUN_ALLOCATE_ERROR_RESP;
    break;
  case STUN_SET_ACTIVE_DEST_REQ:
    msg_type = STUN_SET_ACTIVE_DEST_ERROR_RESP;
    break;
  case STUN_CONNECT_REQ:
    msg_type = STUN_CONNECT_ERROR_RESP;
    break;
  default:
    return 0;
  }

  msg->stun_header.message_type = msg_type;
  msg->error_reason = reason;

  buffer = prepare_stun_msg(msg, 0, return_msg_len, false);
  if(buffer == NULL)
    return NULL;

  body = buffer + sizeof(stun_header_t);
  memset(body, 0, *return_msg_len - sizeof(stun_header_t));

  // add error code
  tlv = (stun_tlv_t *)body;
  tlv->tlv_type = STUN_MA_ERROR_CODE;
  
  body += sizeof(stun_tlv_t);
  ix = (error_code / 100) << 8 | (error_code % 100);
  *(uint32_t *)body = htonl(ix);
  body += sizeof(uint32_t);
  // string must be NUL-terminated
  str_len = strlen(reason);
  if (str_len > 0) {
    strncpy((char *)body, reason, str_len);
  }
  body += ((str_len + 3) / 4) * 4;
  tlv->tlv_length = htons(sizeof(uint32_t) + (((str_len + 3) / 4) * 4));

  if (msg->stun_header.magic_cookie == STUN_MAGIC_COOKIE) {
    if (msg->server.content != NULL) {
      tlv = (stun_tlv_t *)body;
      ix = msg->server.length;
      tlv->tlv_type = htons(STUN_MA_SERVER);
      tlv->tlv_length = htons(ix);
      body += sizeof(stun_tlv_t);
      // string must be NUL-terminated
      str_len = strlen((char *)msg->server.content);
      strncpy((char *)body, (char *)msg->server.content, str_len);
      body += ix;
    }
  }
  
  return buffer;
}


uint8_t *stun_generate_binding_error_response (stun_message_t *msg,
					       uint32_t *return_msg_len)
{
  uint8_t *buffer;
  uint8_t *body;
  stun_tlv_t *tlv;
  uint ix;
  uint str_len;

  msg->stun_header.message_type = STUN_BINDING_ERROR_RESP;

  buffer = prepare_stun_msg(msg, 0, return_msg_len, false);
  if(buffer == NULL)
    return NULL;

  body = buffer + sizeof(stun_header_t);
  memset(body, 0, *return_msg_len - sizeof(stun_header_t));

  // add error code
  tlv = (stun_tlv_t *)body;
  tlv->tlv_type = STUN_MA_ERROR_CODE;
  
  body += sizeof(stun_tlv_t);
  ix = (msg->error_code / 100) << 8 | (msg->error_code % 100);
  *(uint32_t *)body = htonl(ix);
  body += sizeof(uint32_t);
  // string must be NUL-terminated
  str_len = strlen(msg->error_reason);
  if (str_len > 0) {
    strncpy((char *)body, msg->error_reason, str_len);
  }
  body += ((str_len + 3) / 4) * 4;
  tlv->tlv_length = htons(sizeof(uint32_t) + (((str_len + 3) / 4) * 4));

  if (msg->stun_header.magic_cookie == STUN_MAGIC_COOKIE) {
    if (msg->server.content != NULL) {
      tlv = (stun_tlv_t *)body;
      ix = msg->server.length;
      tlv->tlv_type = htons(STUN_MA_SERVER);
      tlv->tlv_length = htons(ix);
      body += sizeof(stun_tlv_t);
      // string must be NUL-terminated
      str_len = strlen((char *)msg->server.content);
      strncpy((char *)body, (char *)msg->server.content, str_len);
      body += ix;
    }
  }
  
  return buffer;
}


uint8_t *stun_generate_shared_secret_request (uint32_t *return_msg_len)
{
  stun_header_t *header;

  *return_msg_len = sizeof(stun_header_t);

  header = MALLOC_STRUCTURE(stun_header_t);
  if (!header) {
    return 0;
  }

  header->message_type = htons(STUN_SHARED_SECRET_REQ);
  header->message_length = htons(*return_msg_len - sizeof(stun_header_t));
  header->magic_cookie = htonl(STUN_MAGIC_COOKIE);

  transaction_id_generate(header->transaction_id);
  
  return (uint8_t *)header;
}

uint8_t *stun_generate_shared_secret_response (stun_message_t *msg, 
					       uint32_t *return_msg_len)
{
  uint8_t *buffer;
  uint8_t *body;
  stun_tlv_t *tlv;
  uint32_t ix;
  uint str_len;

  if (msg->password.content == NULL || msg->username.content == NULL ||
      msg->password.length == 0 || msg->username.content == 0)
    return NULL;


  msg->stun_header.message_type = STUN_SHARED_SECRET_RESP;

  buffer = prepare_stun_msg(msg, 0, return_msg_len, false);
  if(buffer == NULL)
    return NULL;

  body = buffer + sizeof(stun_header_t);
  memset(body, 0, *return_msg_len - sizeof(stun_header_t));

  body += encode_uint8_ptr_t(body, STUN_MA_USERNAME, &msg->username);
  body += encode_uint8_ptr_t(body, STUN_MA_PASSWORD, &msg->password);

  // write username and password
  if (msg->stun_header.magic_cookie == STUN_MAGIC_COOKIE) {
    // add server
    if (msg->server.content != NULL) {
      tlv = (stun_tlv_t *)body;
      ix = msg->server.length;
      tlv->tlv_type = htons(STUN_MA_SERVER);
      tlv->tlv_length = htons(ix);
      body += sizeof(stun_tlv_t);
      // string must be NUL-terminated
      str_len = strlen((char *)msg->server.content);
      strncpy((char *)body, (char *)msg->server.content, str_len);
      body += ix;
    }
  }

  return buffer;
}


static uint8_t *
stun_generate_ds_indication(uint16_t type,
			    uint8_t *data_buffer,
			    uint32_t data_len,
			    stun_address_t* destination_address,
			    uint32_t *return_msg_len)
{
  uint8_t *buffer;
  stun_message_t msg;
  uint8_t *body;
  uint ret_len;

  memset(&msg, 0, sizeof(msg));
  msg.stun_header.message_type = type;
  msg.stun_header.magic_cookie = STUN_MAGIC_COOKIE;

  ret_len = stun_copy_address_to_msg_nohn(&msg.destination_address, destination_address);

  msg.data_attribute.content = data_buffer;
  msg.data_attribute.length = data_len;

  *return_msg_len = stun_message_calculate_size(&msg);

  buffer = prepare_stun_msg(&msg, 0, return_msg_len, true);
  if(buffer == NULL)
    return NULL;

  body = buffer + sizeof(stun_header_t);
  body += encode_msg_address_t(body, STUN_MA_DESTINATION_ADDRESS,
			       &msg.destination_address);
  body += encode_uint8ptr(body, STUN_MA_DATA, data_buffer, data_len);
 
  return buffer;
}


uint8_t *stun_generate_data_indication(uint8_t *data_buffer,
				       uint32_t data_len,
				       stun_address_t* destination_address,
				       uint32_t *return_msg_len)
{
  return stun_generate_ds_indication(STUN_DATA_INDICATION,
				     data_buffer, 
				     data_len, 
				     destination_address,
				     return_msg_len);
}


uint8_t *stun_generate_send_indication(uint8_t *data_buffer,
				       uint32_t data_len,
				       stun_address_t* destination_address,
				       uint32_t *return_msg_len)
{
  return stun_generate_ds_indication(STUN_SEND_INDICATION,
				     data_buffer, 
				     data_len, 
				     destination_address,
				     return_msg_len);
}

const char *stun_msg_type(stun_message_t *msg)
{
  switch (msg->stun_header.message_type) {

  case STUN_BINDING_REQ:
    return "STUN Binding Request";
  case STUN_BINDING_RESP:
    return "STUN Binding Response";
  case STUN_BINDING_ERROR_RESP:
    return "STUN Binding Error";


  case STUN_ALLOCATE_REQ:
    return "STUN Relay Allocate Request";
  case STUN_ALLOCATE_RESP:
    return "STUN Relay Allocate Response";
  case STUN_ALLOCATE_ERROR_RESP:
    return "STUN Relay Allocate Error";


  case STUN_SET_ACTIVE_DEST_REQ:
    return "STUN Relay Set Active Destination Request";
  case STUN_SET_ACTIVE_DEST_RESP:
    return "STUN Relay Set Active Destination Response";
  case STUN_SET_ACTIVE_DEST_ERROR_RESP:
    return "STUN Relay Set Active Destination Error";


  case STUN_SHARED_SECRET_REQ:
    return "STUN Shared Secret Request";
  case STUN_SHARED_SECRET_RESP:
    return "STUN Shared Secret Response";
  case STUN_SHARED_SECRET_ERROR_RESP:
    return "STUN Shared Secret Error";


  case STUN_SEND_INDICATION:
    return "STUN Relay Send Indication";

  case STUN_DATA_INDICATION:
    return "STUN Relay Data Indication";

  case STUN_CONNECT_REQ:
    return "STUN Connect Request";
  case STUN_CONNECT_RESP:
    return "STUN Connect Response";
  case STUN_CONNECT_ERROR_RESP:
    return "STUN Connect Error";

  default:
    return "Unknown STUN message";
  }
}


void stun_uint32t_set (struct stun_msg_uint32_t *msg, uint32_t value)
{
  msg->is_valid = true;
  msg->content = value;
}

/* Check the stun header of the message whether the pointed message is a stun 
 * binding request. Note the message still needs to be parsed to be sure that 
 * it is a valid stun message. Following checkings are performed:
 *   1. The STUN message type is STUN Binding Request;
 *   2. The length of the STUN message is multiples of four (4); and
 *   3. The magic cookie field is 0x2112A442.
 */
bool is_stun_binding_req_message (const uint8_t *message,
                                  uint32_t length)
{
    stun_header_t *stun_hdr;

    if (!message || (length < sizeof(stun_header_t))) {
        return false;
    }

    stun_hdr = (stun_header_t*)message;
    if ((ntohs(stun_hdr->message_type) == STUN_BINDING_REQ) && 
        (ntohs(stun_hdr->message_length) % 4 == 0) &&
        (ntohl(stun_hdr->magic_cookie) == STUN_MAGIC_COOKIE)) {
        return true;
    }
    
    return false;
}
