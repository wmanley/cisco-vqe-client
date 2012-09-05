#include "stun_private.h"
#include "transaction_id.h"
#include <stdarg.h>

static bool stun_debug_enable = false;

static 
void stun_debug(const char *fmt, ...)
{
  if(stun_debug_enable) {
    va_list ap;
    
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
  }
}

bool compare_stun_address (const stun_address_t *left, 
			   struct sockaddr *right,
			   socklen_t right_len)
{
  if (left->family == STUN_ADDRESS_FAMILY_IPV4) {
    struct sockaddr_in *sin;
    if (right->sa_family != AF_INET) return false;
    sin = (struct sockaddr_in *)right;
    if (left->port != ntohs(sin->sin_port)) return false;
    return sin->sin_addr.s_addr == left->addr.ipv4_addr.s_addr;
  } else {
    struct sockaddr_in6 *sin6;
    if (right->sa_family != AF_INET6) return false;
    sin6 = (struct sockaddr_in6 *)right;
    if (left->port != ntohs(sin6->sin6_port)) return false;
    return memcmp(&left->addr.ipv6_addr.s6_addr, 
		  sin6->sin6_addr.s6_addr,
		  INET6_ADDRSTRLEN) == 0;
	
  }
}
void stun_to_sock_addr (const stun_address_t* from, 
			struct sockaddr_storage* to)
{
  if(from == NULL || to == NULL)
    return;
  if (from->family == STUN_ADDRESS_FAMILY_IPV4) {
    struct sockaddr_in *saddr = (struct sockaddr_in *)to;
    
    saddr->sin_port = htons(from->port);
    saddr->sin_family = AF_INET;
    saddr->sin_addr = from->addr.ipv4_addr;
  } else {
    struct sockaddr_in6 *saddr6 = (struct sockaddr_in6 *)to;
    
    saddr6->sin6_port = htons(from->port);
    saddr6->sin6_family = AF_INET6;
    saddr6->sin6_addr = from->addr.ipv6_addr;
  }
}

void sock_to_stun_addr (const struct sockaddr_storage* from,
			stun_address_t* to)
{
  if(from == NULL || to == NULL)
    return;
  if (from->ss_family == AF_INET) {
    struct sockaddr_in *saddr = (struct sockaddr_in *)from;
    to->family = STUN_ADDRESS_FAMILY_IPV4;
    to->port = ntohs(saddr->sin_port);
    to->addr.ipv4_addr = saddr->sin_addr;
  } else {
    struct sockaddr_in6 *saddr6 = (struct sockaddr_in6 *)from;
    to->family = STUN_ADDRESS_FAMILY_IPV6;
    to->port = ntohs(saddr6->sin6_port);
    to->addr.ipv6_addr = saddr6->sin6_addr;
  }
}


static bool stun_check_and_copy_address (const stun_address_t *from, 
					 stun_address_t *to,
					 uint16_t length)
{
  switch (from->family) {
  case STUN_ADDRESS_FAMILY_IPV4:
    if (length != STUN_ADDRESS_IPV4_LEN) return false;
    to->addr.ipv4_addr.s_addr = from->addr.ipv4_addr.s_addr;
    stun_debug("to %s ",inet_ntoa(to->addr.ipv4_addr));
    stun_debug("from %s\n", 
	       inet_ntoa(from->addr.ipv4_addr));
    break;
  case STUN_ADDRESS_FAMILY_IPV6:
    if (length != STUN_ADDRESS_IPV6_LEN) return false;
    memcpy(&to->addr.ipv6_addr, &from->addr.ipv6_addr, 16);
    break;
  default:
    return false;
  }
  to->family = from->family;
  to->port = ntohs(from->port);
  return true;
}

static
int stun_parse_uint32_t (const uint8_t *tlv_body, uint32_t tlv_len, 
			 stun_msg_uint32_t* result)
{
  if (tlv_len != 4) {
    return 400;
  }

  result->content = ntohl(*(const uint32_t *)(tlv_body));
  result->is_valid = true;
  return 0;
}


static
int stun_parse_uint8_ptr_t (const uint8_t *tlv_body, uint32_t tlv_len, 
			    stun_msg_uint8_ptr_t* result)
{
  CHECK_AND_FREE(result->content);
  result->content = tlv_body;
  result->length = tlv_len;
  return 0;
}


static
int stun_parse_msg_address_t (const uint8_t *tlv_body, uint32_t tlv_len, 
			      stun_msg_address_t* result)
{
  if (stun_check_and_copy_address((const stun_address_t *)tlv_body, 
				  &result->content, 
				  tlv_len) == false) {
    return 400;
  }
  result->is_valid = true;
  return 0;
}


static
bool is_valid_msg_type (const uint32_t msg_type)
{
  switch (msg_type) {
  case STUN_BINDING_REQ:
  case STUN_BINDING_RESP:
  case STUN_BINDING_ERROR_RESP:
  case STUN_ALLOCATE_REQ:
  case STUN_ALLOCATE_RESP:
  case STUN_ALLOCATE_ERROR_RESP:
  case STUN_SHARED_SECRET_REQ:
  case STUN_SHARED_SECRET_RESP:
  case STUN_SHARED_SECRET_ERROR_RESP:
  case STUN_DATA_INDICATION:
  case STUN_SEND_INDICATION:
  case STUN_SET_ACTIVE_DEST_REQ:
  case STUN_SET_ACTIVE_DEST_RESP:
  case STUN_SET_ACTIVE_DEST_ERROR_RESP:
  case STUN_CONNECT_REQ:
  case STUN_CONNECT_RESP:
  case STUN_CONNECT_ERROR_RESP:
    return true;
    break;
  default:
    return false;
  }
}


uint stun_message_parse (const uint8_t *message, 
			 uint32_t length, 
			 stun_message_t *response, 
			 bool require_magic_cookie)
{
  const uint8_t *start;
  uint32_t start_length;
  const stun_header_t *hdr = (stun_header_t *)message;
  const stun_tlv_t *tlv;
  const uint8_t *tlv_body;
  uint32_t tlv_len;
  uint32_t temp;
  stun_address_t *addr;
  uint error_code = 0;

  memset(response, 0, sizeof(stun_message_t));

  if (sizeof(stun_header_t) > length) return 400;

  if(require_magic_cookie && htonl(STUN_MAGIC_COOKIE) != hdr->magic_cookie)
    return 401;

  start = message;
  start_length = length;

  response->stun_header.message_type = ntohs(hdr->message_type);
  if( ! is_valid_msg_type(response->stun_header.message_type) ) 
    return 402;

  response->stun_header.message_length = ntohs(hdr->message_length);
  if (response->stun_header.message_length + sizeof(stun_header_t) != length) 
    return 403;

  response->stun_header.magic_cookie = ntohl(hdr->magic_cookie);
  memcpy(response->stun_header.transaction_id, hdr->transaction_id, 12);

  message += sizeof(stun_header_t);
  length -= sizeof(stun_header_t);
  while (length > sizeof(stun_tlv_t)) {
    tlv = (stun_tlv_t *)message;
    tlv_len = ntohs(tlv->tlv_length);
    tlv_body = message + sizeof(stun_tlv_t);

    if (tlv_len + sizeof(stun_tlv_t) > length)
      return 404;
    switch (ntohs(tlv->tlv_type)) {
    case STUN_MA_MAPPED_ADDRESS:
      if (response->mapped_address.is_valid || response->address_is_xor_mapped) 
	break;
      error_code = stun_parse_msg_address_t(tlv_body, tlv_len, 
					    &response->mapped_address);
      break;
    case STUN_MA_RESPONSE_ADDRESS:
      error_code = stun_parse_msg_address_t(tlv_body, tlv_len, 
					    &response->response_address);
      break;
    case STUN_MA_CHANGE_REQUEST:
      error_code = stun_parse_uint32_t(tlv_body, tlv_len,
				       &response->change_request);
      break;
    case STUN_MA_SOURCE_ADDRESS:
      error_code = stun_parse_msg_address_t(tlv_body, tlv_len, 
					    &response->source_address);
      break;
    case STUN_MA_CHANGED_ADDRESS:
      break;
    case STUN_MA_USERNAME:
      error_code = stun_parse_uint8_ptr_t(tlv_body, tlv_len,
					  &response->username);
      break;
    case STUN_MA_PASSWORD:
      error_code = stun_parse_uint8_ptr_t(tlv_body, tlv_len,
					  &response->password);
      break;
    case STUN_MA_MESSAGE_INTEGRITY:
      error_code = stun_parse_uint8_ptr_t(tlv_body, tlv_len,
					  &response->hmac);
      response->hmac_offset = message - start;
      // FIXME: after this point, you should ignore all values
      break;
    case STUN_MA_ERROR_CODE:
      if (tlv_len < 4) {
	return 405;
      }
      temp = ntohl(*(uint32_t *)(tlv_body));
      response->have_error_code = true;
      response->error_code = tlv_body[2] * 100;
      response->error_code += tlv_body[3];
      if (tlv_len > 4) {
	char* tmp;
	CHECK_AND_FREE(response->error_reason);
	tmp = (char *)malloc(tlv_len - 4 + 1);
    if (!tmp) {
      return 0;
    }
	memcpy(tmp, tlv_body + 4, tlv_len - 4);
	tmp[tlv_len - 4] = '\0';
	response->error_reason = tmp;
      }
      break;
    case STUN_MA_UNKNOWN_ATTRIBUTES:
      break;
    case STUN_MA_REFLECTED_FROM:
      break;
    case STUN_MA_REALM:
      error_code = stun_parse_uint8_ptr_t(tlv_body, tlv_len, &response->realm);
      break;
    case STUN_MA_NONCE:
      error_code = stun_parse_uint8_ptr_t(tlv_body, tlv_len, &response->nonce);
      break;
    case STUN_MA_XOR_MAPPED_ADDRESS:
      addr = (stun_address_t *)(tlv_body);
      if (stun_check_and_copy_address(addr, &response->mapped_address.content, tlv_len) == false) {
	return 406;
      }
      response->mapped_address.is_valid = true;
      response->address_is_xor_mapped = true;
      response->mapped_address.content.port ^= (STUN_MAGIC_COOKIE >> 16);
      if (response->mapped_address.content.family == STUN_ADDRESS_FAMILY_IPV4) {
	response->mapped_address.content.addr.ipv4_addr.s_addr ^= htonl(STUN_MAGIC_COOKIE);
	//printf("xor mapped %s\n", inet_ntoa(response->mapped_address.content.addr.ipv4_addr));
      } else {
	tlv_body = (uint8_t *)hdr->magic_cookie;
	for (temp = 0; temp < 16; temp++) {
	  response->mapped_address.content.addr.ipv6_addr.s6_addr[temp] ^= tlv_body[temp];
	}
      }
      break;
    case STUN_MA_ICE_PRIORITY:
      error_code = 
	stun_parse_uint32_t(tlv_body, tlv_len, &response->ice_priority);
      break;

    case STUN_MA_SERVER:
      error_code = stun_parse_uint8_ptr_t(tlv_body, tlv_len, 
					  &response->server);
      break;

    case STUN_MA_ALTERNATE_SERVER:
      error_code = stun_parse_msg_address_t(tlv_body, tlv_len, 
					    &response->alternate_address);
      break;
    case STUN_MA_RELAY_ADDRESS:
      error_code = stun_parse_msg_address_t(tlv_body, tlv_len, 
					    &response->relay_address);
      break;
    case STUN_MA_BINDING_LIFETIME:
      error_code = stun_parse_uint32_t(tlv_body, tlv_len,
				       &response->binding_lifetime);
      break;
    case STUN_MA_LIFETIME:
      error_code = stun_parse_uint32_t(tlv_body, tlv_len,
				       &response->lifetime);
      break;
    case STUN_MA_TIMER_VAL:
      error_code = stun_parse_uint32_t(tlv_body, tlv_len,
				       &response->timer_val);
      break;
    case STUN_MA_BANDWIDTH:
      error_code = stun_parse_uint32_t(tlv_body, tlv_len, 
				       &response->bandwidth);
      break;

    case STUN_MA_DATA:
      error_code = stun_parse_uint8_ptr_t(tlv_body, tlv_len,
					  &response->data_attribute);
      break;
    case STUN_MA_DESTINATION_ADDRESS:
      error_code = stun_parse_msg_address_t(tlv_body, tlv_len, 
					    &response->destination_address);
      break;
    case STUN_MA_REMOTE_ADDRESS:
    default:
      break;
    }

    if (error_code > 0)
      return error_code;

    tlv_len = ((tlv_len + 3) / 4) * 4; // round up to the next 4 byte octet
    message += tlv_len + sizeof(stun_tlv_t);
    length -= tlv_len + sizeof(stun_tlv_t);
  }
  return 0;
}

bool stun_message_integrity_check (const uint8_t *orig_message,
				   const uint8_t *password, 
				   uint32_t password_len,
				   stun_message_t *response)
{
  if (password == NULL) return false;
  if (response->hmac.content == NULL || 
      response->hmac.length == 0) return false;

  return verify_message_integrity(orig_message, response->hmac_offset,
				  password, password_len, 
				  response->hmac.content,
				  response->hmac.length);
}


uint stun_message_parse_with_integrity_check (const uint8_t *message, 
					      uint32_t length, 
					      uint8_t *password,
					      uint32_t password_len,
					      stun_message_t *response, 
					      bool require_magic_cookie)
{
  uint ret = 
    stun_message_parse(message, length, response, require_magic_cookie);

  if (ret == 0) {
    if (stun_message_integrity_check(message, password, 
				     password_len, response)) {
      return 0; 
    } else {
      return 420; // FIXME: auth error code?
    }
  }
  return ret;
}


void stun_message_find_username_realm (uint8_t *message, 
				       uint32_t length, 
				       uint8_t **username,
				       uint32_t *username_len,
				       uint8_t **realm,
				       uint32_t *realm_len)
{
  stun_header_t *hdr = (stun_header_t *)message;
  stun_tlv_t *tlv;
  uint8_t *tlv_body;
  uint32_t tlv_len;

  *username = NULL;
  *realm = NULL;
  if (sizeof(stun_header_t) > length) return;

  if( ! is_valid_msg_type(ntohs(hdr->message_type)) )
    return;

  if (ntohs(hdr->message_length) + sizeof(stun_header_t) != length) {
    return;
  }

  message += sizeof(stun_header_t);
  length -= sizeof(stun_header_t);
  while (length > sizeof(stun_tlv_t)) {
    tlv = (stun_tlv_t *)message;
    tlv_len = ntohs(tlv->tlv_length);
    tlv_body = message + sizeof(stun_tlv_t);

    if (tlv_len + sizeof(stun_tlv_t) > length)
      return;
    switch (ntohs(tlv->tlv_type)) {
    case STUN_MA_USERNAME:
      *username_len = tlv_len;
      *username = tlv_body;
      break;
    case STUN_MA_REALM:
      *realm_len = tlv_len;
      *realm = tlv_body;
      break;
    }
    tlv_len = ((tlv_len + 3) / 4) * 4;
    message = tlv_body + tlv_len;
    length -= sizeof(stun_tlv_t) + tlv_len;
  }
  return;
}

uint stun_message_length (uint8_t *message, uint32_t length)
{
  stun_header_t *hdr = (stun_header_t *)message;


  if (sizeof(stun_header_t) > length) return 0;

  if( ! is_valid_msg_type(ntohs(hdr->message_type)) )
    return 0;

  if (ntohs(hdr->message_length) + sizeof(stun_header_t) < length) {
    return 0;
  }

  return ntohs(hdr->message_length) + sizeof(stun_header_t);
}

bool stun_message_match_transaction_id (transaction_id_t *tid, 
					uint8_t *message)
{
  stun_header_t *hdr = (stun_header_t *)message;

  if (ntohl(hdr->magic_cookie) != STUN_MAGIC_COOKIE) return false;

  return transaction_id_match(tid->tid, hdr->transaction_id);
}
