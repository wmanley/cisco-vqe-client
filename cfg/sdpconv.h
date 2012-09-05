/*
 *------------------------------------------------------------------
 * Channel format conversion functions for VCPT
 *
 * April 2007, Dong Hsu
 *
 * Copyright (c) 2006-2007 by Cisco Systems, Inc.
 * All rights reserved.
 *------------------------------------------------------------------
 */

#ifndef _SDPCONV_H_
#define _SDPCONV_H_

/*! \file sdpconv.h
    \brief Channel format conversion

    Internal channel format conversion functions.
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "cfgapi.h"

/**
 * gen_xml_hdr - Generate the header part of xml file
 */
extern void gen_xml_hdr(const char *xmlFilename);

/**
 * gen_xml_channel - Generate the body of each channel
 */
extern void gen_xml_channel(channel_cfg_t *channel);

/**
 * gen_xml_end - Generate the end part of xml file
 */
extern void gen_xml_end(const char *filename);

#endif /* _SDPCONV_H_ */
