/**
 * \addtogroup rime
 * @{
 */

/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * $Id: rime.h,v 1.20 2008/07/03 22:36:02 adamdunkels Exp $
 */

/**
 * \file
 *         Header file for the Rime stack
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#ifndef __RIME_H__
#define __RIME_H__

#include "rime/collect.h"
#include "rime/ctimer.h"
#include "rime/ipolite.h"
#include "rime/mesh.h"
#include "rime/multihop.h"
#include "rime/neighbor-discovery.h"
#include "rime/neighbor.h"
#include "rime/netflood.h"
#include "rime/polite.h"
#include "rime/queuebuf.h"
#include "rime/rimeaddr.h"
#include "rime/rimebuf.h"
#include "rime/rimestats.h"
#include "rime/rmh.h"
#include "rime/route-discovery.h"
#include "rime/route.h"
#include "rime/runicast.h"
#include "rime/rucb.h"
#include "rime/timesynch.h"
#include "rime/trickle.h"

#include "mac/mac.h"
/**
 * \brief      Initialize Rime
 *
 *             This function should be called from the system boot up
 *             code to initialize Rime.
 */
void rime_init(const struct mac_driver *);

/**
 * \brief      Send an incoming packet to Rime
 *
 *             This function should be called by the network driver to
 *             hand over a packet to Rime for furhter processing. The
 *             packet should be placed in the rimebuf (with
 *             rimebuf_copyfrom()) before calling this function.
 *
 */
void rime_input(void);

/**
 * \brief      Rime calls this function to send out a packet
 *
 *             This function must be implemented by the driver running
 *             below Rime. It is called by abRime to send out a
 *             packet. The packet is consecutive in the rimebuf. A
 *             pointer to the first byte of the packet is obtained
 *             with the rimebuf_hdrptr() function. The length of the
 *             packet to send is obtained with the rimebuf_totlen()
 *             function.
 *
 *             The driver, which typically is a MAC protocol, may
 *             queue the packet by using the queuebuf functions.
 */
void rime_driver_send(void);

void rime_set_output(void (*output_function)(void));
void rime_output(void);

extern const struct mac_driver *rime_mac;

struct rime_sniffer {
  struct rime_sniffer *next;
  void (* input_callback)(void);
  void (* output_callback)(void);
};

#define RIME_SNIFFER(name, input_callback, output_callback) \
static struct rime_sniffer name = { NULL, input_callback, output_callback }

void rime_sniffer_add(struct rime_sniffer *s);
void rime_sniffer_remove(struct rime_sniffer *s);


/* Generic Rime return values. */
enum {
  RIME_OK,
  RIME_ERR,
  RIME_ERR_CONTENTION,
  RIME_ERR_NOACK,
};
#endif /* __RIME_H__ */

/** @} */
/** @} */
