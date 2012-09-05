/*------------------------------------------------------------------
 *
 * test_stun_client.c
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
#include <sys/time.h>
#include <errno.h>

#include "stun.h"
#include <getopt.h>
#include "stun_socket.h"
#include <sys/socket.h>
#include <poll.h>

static const char *usageString = "[-message-integrity] [-flood] <server> <src-port> <dest-port>";
static const char *password = "testclientpassword";
static const char *pass2 = "testserverpassword";

int main (int argc, char **argv)
{
  stun_sock_t *stun_sock;
  uint8_t *msg;
  size_t len;
  struct pollfd pollit;
  uint8_t msg_buffer[1514];
  uint ret_code;
  stun_message_t stun_msg;
  uint8_t username[16];
  uint32_t ix;
  uint32_t times;
  transaction_id_t *tid;
  struct timeval start, now;
  bool flood = false;
  char *ProgName = argv[0];
  bool doMI = false;

  while (true) {
    int c = -1;
    int option_index = 0;
    static struct option long_options[] = {
      { "flood", 0, 0, 'f' },
      { "message-integrity", 0, 0, 'm'},
      { "help", 0, 0, 'h' },
      { NULL, 0, 0, 0 }
    };

    c = getopt_long_only(argc, argv, "fhm",
			 long_options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 'f':
      flood = true;
      break;
    case 'm':
      doMI = true;
      break;
    case '?':
    case 'h':
      fprintf(stderr, "usage: %s %s\n", ProgName, usageString);
      exit(0);
    }
  }

  /* check that we have at least one non-option argument */
  if ((argc - optind) < 3) {
    fprintf(stderr, "usage: %s %s\n", ProgName, usageString);
    exit(1);
  }

  /* point to the specified file names */
  
  printf("Sending request from localhost:%d to %s:%d...\n",
         atoi(argv[optind+1]), argv[optind], atoi(argv[optind+2]));
  
  stun_sock = stun_sock_create("STUN client",
                                 INADDR_ANY,
                                 htons(atoi(argv[optind+1])),
                                 1514);
  
  for (ix = 0; ix < 16; ix++) {
    username[ix] = random();
  }

  printf("generating username len %u\n", ix);
  times = 0;
  if (gettimeofday(&start, NULL) == -1) {
      perror("gettimeofday(start)");
  }
  do {
    tid = NULL;
    msg = stun_generate_binding_request(username, ix, 
					doMI ? (uint8_t *)password : NULL, 
					doMI ? strlen(password) : 0, 
					0, &tid, &len);
    
    if (msg == NULL) {
      printf("no message generated\n");
      exit(-1);
    }
    
    struct sockaddr_in    s_in;

    s_in.sin_family      = AF_INET;
    if (!inet_pton(AF_INET, argv[optind], &s_in.sin_addr)) {
        perror("inet_pton");
        exit(-1);
    }
    s_in.sin_port        = htons(atoi(argv[optind+2]));
    printf("sending...");
    if (sendto(stun_sock->fd, msg, len, 0, (struct sockaddr *) &s_in, sizeof(s_in)) == -1) {
        perror("sendto");
        exit(-1);
    }
    printf("sent\n");

    if (flood == false) {
      pollit.fd = stun_sock->fd;
      pollit.events = POLLIN | POLLPRI;
      pollit.revents = 0;
      
      if (poll(&pollit, 1, 1000) > 0) {
	len = recvfrom(stun_sock->fd, msg_buffer, 1514, 0, NULL, 0);
        if (len < 0) {
            /* recvfrom() error */
            if (errno != EAGAIN) {
                perror("recvfrom(): %m");
                exit(-1);
            }
        } else {

            printf("len received %d\n", len);
            ret_code = stun_message_parse(msg_buffer, len, &stun_msg, false);
            printf("return code %u\n", ret_code);
            if (doMI) {
                if (stun_message_integrity_check(msg_buffer, 
                                                 (const uint8_t *)pass2, 
                                                 strlen(pass2), 
                                                 &stun_msg) == false) {
                    printf("mi failed\n");
                } else {
                    printf("mi passed\n");
                }
            }

            if (stun_msg.mapped_address.is_valid) {
                printf("mapped address is %s:%u\n",
                       inet_ntoa(stun_msg.mapped_address.content.addr.ipv4_addr),
                       stun_msg.mapped_address.content.port);
            } else {
                printf("stun address is invalid\n");
            }
        }
      }
      
      CHECK_AND_FREE(tid);
      free(msg);
    }
    if (gettimeofday(&now, NULL)) {
        perror("gettimeofday");
    }
    times++;
  } while (flood == true && start.tv_sec + 10 > now.tv_sec);

  if (flood) {
    uint64_t usec = now.tv_usec;
    if (now.tv_usec < start.tv_usec) {
      usec += 1000000;
      now.tv_sec--;
    }
    usec -= start.tv_usec;
    printf("%u times in %lu.%llu secs\n", times, now.tv_sec - start.tv_sec,
	   usec);
  }

  close(stun_sock->fd);
  free(stun_sock);

  return 0;
}

 
