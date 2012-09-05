STUN Library for VQE

Andrew Sawchuk (ansawchu@cisco.com)
December 2006

Copyright (c) 2006 by Cisco Systems, Inc.
All rights reserved.

This STUN library has been derived from William May's code which is 
included in his project.  Portions of it have been modified and updated
to be more useful for VQE's needs.

The base STUN library contains STUN message processing functions only,
so it does not include any sockets calls.  The code for this base
library is contained in these files:

  stun.h
  stun_hmac.c
  stun_includes.h
  stun_msg.c
  stun_parse.c
  stun_private.h
  transaction_id.c
  transaction_id.h

The only changes that were made to the above files were to modularize their
functionality and cut unnecessary dependencies on other components.

In addition to the base STUN library, there are a couple test applications
that were derived from a test application that Bill May provided.  However,
the stun_socket files have replaced the net_udp package, which is what
the original test applications were using for sockets and network calls.
The code for these test applications is contained in these files:

  stun_socket.c
  stun_socket.h
  test_stun_client.c
  test_stun_parser.c
  test_stun_server.c


