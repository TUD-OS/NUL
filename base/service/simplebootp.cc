/**
 * Simple BOOTP daemon.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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
 * A simple BOOTP server.
 *
 * State: unstable
 * Features: reply with a BOOTP packet.
 * Missing:  UDP checksum, DHCP options, gateway, direct reply, servername
 * Documentation: RFCs 951(BOOTP), 1497(BOOTP-ext), 1542(BOOTP-clarification),
 * Documentation: 2131(DHCP), 768(UDP), 791(IP)
 */
class SimpleBootp : public StaticReceiver<SimpleBootp>
{
  DBus<MessageNetwork> &_bus_network;
  typedef struct{
    struct {
      unsigned short dst[3];
      unsigned short src[3];
      unsigned short type;
    } ethernet;
    struct {
      unsigned char version_ihl;
      unsigned char tos;
      unsigned short len;
      unsigned short id;
      unsigned short flags_fragment;
      unsigned char ttl;
      unsigned char protocol;
      unsigned short checksum;
      unsigned src;
      unsigned dst;
    } ip  __attribute((packed));
    struct {
      unsigned short srcport;
      unsigned short dstport;
      unsigned short len;
      unsigned short checksum;
    } udp;
    struct {
      unsigned char op;
      unsigned char htype;
      unsigned char hlen;
      unsigned char hops;
      unsigned xid;
      unsigned short secs;
      unsigned short flags;
      unsigned ciaddr;
      unsigned yiaddr;
      unsigned siaddr;
      unsigned giaddr;
      unsigned char chaddr[16];
      unsigned char sname[64];
      unsigned char file[128];
      unsigned char vendor[64];
    } bootp __attribute((packed));
  } EthernetBootpPacket;

  unsigned char _replypacket[1528];
  unsigned _ip;
  unsigned _mask;
public:
  bool  receive(MessageNetwork &msg) {
    const EthernetBootpPacket *request = reinterpret_cast<const EthernetBootpPacket *>(msg.buffer);

    if (// check for size
	msg.len < sizeof(EthernetBootpPacket)
	// ethernet: broadcast MAC?
	|| request->ethernet.dst[0] != 0xffff
	|| request->ethernet.dst[1] != 0xffff
	|| request->ethernet.dst[2] != 0xffff
	// IPV4?
	|| request->ethernet.type != 0x8 || request->ip.version_ihl != 0x45
	// broadcast UDP?
	|| request->ip.dst != 0xffffffff || request->ip.protocol != 0x11
	// right ports?
	|| request->udp.srcport != 0x4400 || request->udp.dstport != 0x4300
	// bootp request?
	|| request->bootp.op != 1
	// does it fit into the reply?
	|| sizeof(_replypacket) < msg.len
	)
	return false;
      // XXX check SNAME and Gateway
      assert(!request->bootp.giaddr);

      EthernetBootpPacket *reply = reinterpret_cast<EthernetBootpPacket *>(_replypacket);
      *reply = *request;

      reply->bootp.op     = 2;
      reply->bootp.yiaddr = Math::htonl(_ip & ~_mask)
	| *reinterpret_cast<const unsigned*>(request->ethernet.src+1) & Math::htonl(_mask);
      reply->bootp.siaddr = Math::htonl(_ip);
      memset(reply->bootp.vendor, 0, sizeof(reply->bootp.vendor)); 	// clear vendor field

      reply->udp.srcport  = request->udp.dstport;
      reply->udp.dstport  = request->udp.srcport;
      reply->udp.len      = Math::htons(sizeof(reply->bootp));
      reply->udp.checksum = 0;  // no UDP checksum


      reply->ip.len       = Math::htons(sizeof(reply->bootp) + sizeof(reply->udp));
      reply->ip.src       = Math::htonl(_ip);
      // XXX send to ciaddr if known
      reply->ip.dst       = 0xffffffff;
      reply->ip.checksum  = 0;
      unsigned checksum = 0;
      for (unsigned i=0; i < sizeof(reply->ip)/2; i++)
        checksum += reinterpret_cast<unsigned short *>(&reply->ip)[i];
      reply->ip.checksum = ~(checksum + (checksum >> 16));
      memcpy(reply->ethernet.dst, reply->ethernet.src, 6);
      reply->ethernet.src[0] = 0x1600;
      reply->ethernet.src[1] = _ip >> 0x10;
      reply->ethernet.src[2] = _ip >> 0x00;

      Logging::printf("BOOTP dst %x.%x.%x <- %x.%x.%x len %d IP %d %x:%x <- %x:%x bootp %x client %x\n",
		      request->ethernet.dst[0], request->ethernet.dst[1], request->ethernet.dst[2],
		      request->ethernet.src[0], request->ethernet.src[1], request->ethernet.src[2],
		      msg.len, request->ip.protocol,
		      request->ip.dst, request->udp.dstport, request->ip.src, request->udp.srcport,
		      request->bootp.op, reply->bootp.yiaddr);

      // send to destination
      MessageNetwork msg2(_replypacket, sizeof(EthernetBootpPacket), ~0u);
      return _bus_network.send(msg2);
    };

    SimpleBootp(DBus<MessageNetwork> &bus_network, unsigned ip, unsigned mask)
      : _bus_network(bus_network), _ip(ip), _mask(mask) {}
};

PARAM(bootp,
      {
	Device *dev = new SimpleBootp(mb.bus_network, argv[0], (1 << (32 - argv[1])) - 1);
	mb.bus_network.add(dev, SimpleBootp::receive_static<MessageNetwork>);
      },
      "bootp:ip,netmask - provide a simple BOOTP server.",
      "Example: 'bootp:0x0a000000,8'.",
      "Please note that we deduce IP addresses from the MAC address, thus the same MAC will result in the same IP.")
