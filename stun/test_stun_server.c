/*------------------------------------------------------------------
 *
 * test_stun_server.c
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
#include "stun_socket.h"
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include "stun_private.h"
#include <getopt.h>

static bool do_stop = false;

static void stop_handler (int dummy)
{
  do_stop = true;
}
static const char *usageString = "[-port <port-num>] [-message-integrity] [-help]";

#define DEFAULT_STUN_PORT 3478
#define RCV_BUFFER_SIZE 128000    /* Socket receive buffer size in bytes */
#define MSG_BUFFER_SIZE 1514    
#define STUN_NUM_FDS 1
#define STUN_POLL_TIMEOUT 1000

typedef struct stun_global_stats_ {
  uint32_t rcv_messages;                 /* number of valid stun messages received */
  uint32_t send_messages;                /* number of stun messages transmitted */
  uint32_t binding_requests_received;     
  uint32_t binding_responses_sent;          
  uint32_t invalid_stun_msg_drops;        /* number of invalid stun messages received */
  uint32_t unsupported_stun_msg_drops;    /* number of drops of unsupported message types */
  uint32_t send_drops;                    /* number of message drops trying to send */
  uint32_t integrity_checks_failed;       /* number of failed message integrity checks on rcv */
  uint32_t integrity_checks_passed;       /* number of passed message integrity checks on rcv */
} stun_global_stats_t;

stun_global_stats_t stats;  /* global statistics holder */

static const char *password = "testclientpassword";
static const char *pass2 = "testserverpassword";

#define STUN_MSG_BUFFER_SIZE 1514

int main (int argc, char **argv)
{
  stun_sock_t *stun_sock;
  struct pollfd pollit;
  uint8_t msg_buffer[STUN_MSG_BUFFER_SIZE];
  uint ret_code;
  stun_message_t stun_msg;
  uint8_t *msg;
  uint32_t len;
  struct sigaction act;
  struct sockaddr_storage sa_storage;
  socklen_t sock_size;
  char *ProgName = argv[0];
  uint16_t port = DEFAULT_STUN_PORT;
  bool doMI = false;
  
  memset(&act, 0, sizeof(struct sigaction));
  memset(&stats, 0, sizeof(stun_global_stats_t));

  while (true) {
    int c = -1;
    int option_index = 0;
    static struct option long_options[] = {
      { "message-integrity", 0, 0, 'm'},
      { "port", required_argument, 0, 'p' },
      { "help", 0, 0, 'h' },
      { NULL, 0, 0, 0 }
    };

    c = getopt_long_only(argc, argv, "hmp:",
			 long_options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 'm':
      doMI = true;
      break;
    case 'p':
      port = (uint16_t)atoi(optarg);
      break;
    case '?':
    case 'h':
      fprintf(stderr, "usage: %s %s\n", ProgName, usageString);
      exit(0);
    }
  }

  printf("starting stun server at port %d\n", port);

  act.sa_handler = stop_handler;
  if (sigemptyset(&act.sa_mask) == -1) {
      perror("sigemptyset");
  }
  act.sa_flags = 0;

  if (sigaction(SIGINT, &act, 0) == -1) {
      perror("sigaction(SIGINT)");
  }
  if (sigaction(SIGHUP, &act, 0) == -1) {
      perror("sigaction(SIGHUP)");
  }
  argv++;
  stun_sock = stun_sock_create("STUN server",
                               INADDR_ANY,
                               htons(port),
                               RCV_BUFFER_SIZE);

  while (do_stop == false) {
    pollit.fd = stun_sock->fd;
    pollit.events = POLLIN | POLLPRI;
    pollit.revents = 0;
    
    if (poll(&pollit, STUN_NUM_FDS, STUN_POLL_TIMEOUT) > 0) {
      sock_size = sizeof(sa_storage);
      len = recvfrom(stun_sock->fd, msg_buffer, STUN_MSG_BUFFER_SIZE, 0,
                     (struct sockaddr *)&sa_storage, &sock_size);
      ret_code = stun_message_parse(msg_buffer, len, &stun_msg, false);
      if (ret_code != 0) {
          stats.invalid_stun_msg_drops++;
          continue;
      } else {
          stats.rcv_messages++;
      }
      if (stun_msg.stun_header.message_type == STUN_BINDING_REQ) {
        stats.binding_requests_received++;
	sock_to_stun_addr(&sa_storage, &stun_msg.mapped_address.content);
	stun_msg.mapped_address.is_valid = true;

	if (doMI) {
	  if (stun_message_integrity_check(msg_buffer, (const uint8_t *)password,
					   strlen(password), &stun_msg) == false) {
            stats.integrity_checks_failed++;
            continue;
	  } else {
            stats.integrity_checks_passed++;
	  }
          stun_msg.password.content = (const uint8_t *)pass2;
          stun_msg.password.length = strlen(pass2);
	}
    
	msg = stun_generate_binding_response(&stun_msg, &len);
        if (msg && len) {
            if (sendto(stun_sock->fd, msg, len, 0,
                       (const struct sockaddr *)&sa_storage, sock_size) == -1) {
                stats.send_drops++;
            } else {
                stats.binding_responses_sent++;
                stats.send_messages++;            }
        } else {
            stats.invalid_stun_msg_drops++;
        }
        if (msg) {
            free(msg);
            msg = NULL;
        }
      } else {
          stats.unsupported_stun_msg_drops++;
      }
    }
  }
    
  close(stun_sock->fd);
  free(stun_sock);
  printf("messages rcvd %u, responses sent %u\n"
         "binding_requests_received %u, binding_responses_sent %u\n"
         "integrity_checks_passed %u, integrity_checks_failed %u\n"
         "invalid_stun_msg_drops %u, unsupported_stun_msg_drops %u\n"
         "send_drops %u\n",
         stats.rcv_messages, stats.send_messages,
         stats.binding_requests_received, stats.binding_responses_sent,
         stats.integrity_checks_passed, stats.integrity_checks_failed,
         stats.invalid_stun_msg_drops, stats.unsupported_stun_msg_drops,
         stats.send_drops);
  return 0;
}


