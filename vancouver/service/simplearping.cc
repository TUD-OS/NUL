/**
 * Arp ping test.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */
#include "nul/motherboard.h"

/**
 * A simple arp ping to test the network infrastructure.
 * State: testing
 */
class SimpleArpPing : public StaticReceiver<SimpleArpPing>
{
  const void * _mac;

  /**
   * Build an arp packet.
   */
  unsigned make_arp_packet(unsigned char *buffer, const void *srcmac, unsigned srcip, unsigned dstip)
  {
    srcip = Math::htonl(srcip);
    dstip = Math::htonl(dstip);
    unsigned char *orig = buffer;
    memset(buffer, 0xff, 6),    buffer += 6;  // broadcast
    memcpy(buffer, srcmac, 6),  buffer += 6;  // srcmac
    unsigned char middle[] =
      {
	0x08, 0x06, // ethernet-type
	0x00, 0x01, // ethernet protocol
	0x08, 0x00, // ARP
	0x06, 0x04, // hardware-address: 6byte, protocol-address: 4byte
	0x00, 0x01, // opcode: request
      };
    memcpy(buffer, middle, sizeof(middle)), buffer += sizeof(middle);
    memcpy(buffer, srcmac, 6), buffer += 6;
    memcpy(buffer, &dstip, 4), buffer += 4;
    memset(buffer, 0x00, 6),   buffer += 6;
    memcpy(buffer, &dstip, 4), buffer += 4;
    return buffer - orig;
  }

public:
  bool  receive(MessageNetwork &msg)
  {
    if (msg.len >= 8 && !memcmp(msg.buffer, _mac, 6))
      {
	for (unsigned i=0; i < 64 && i < msg.len; i++)
	  Logging::printf(" %02x%c", msg.buffer[i], !((i + 1) % 16) ? '\n' : ',');
	Logging::printf("\n");
      }
    return false;
  }

  void arp(DBus<MessageNetwork> &bus_network, unsigned dstip)
  {
    unsigned char packet[60];
    memset(packet, 0, sizeof(packet));
    make_arp_packet(packet, _mac, 0, dstip);
    MessageNetwork msg = MessageNetwork(packet, sizeof(packet), 0);
    unsigned count = 0;
    count += bus_network.send(msg);
    Logging::printf("%s sent %d packets\n",__PRETTY_FUNCTION__, count);
  }

  SimpleArpPing(const void *mac) : _mac(mac) {}
};



PARAM(arping,
      {
	SimpleArpPing *dev = new SimpleArpPing("\x12\x23\x45\x67\x89\xab");
	mb.bus_network.add(dev, SimpleArpPing::receive_static<MessageNetwork>);
	dev->arp(mb.bus_network, argv[0]);
      },
      "arping:dstip - test the network driver.",
      "Example: arping:0x0a000202.");
