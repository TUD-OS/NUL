/** @file
 * Host ne2k driver.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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

#include <nul/motherboard.h>
#include <host/hostpci.h>
#include <service/net.h>
#include <service/endian.h>

/**
 * A simple ne2k pci driver, mainly used on qemu devices.
 *
 * Features: reset, send, irq, receive, overflow-recover
 * Missing:  read counters, configuration of full-duplex modes
 * State: testing
 * Documentation: DP8390D, rtl8029, mx98905b
 */
class HostNe2k : public StaticReceiver<HostNe2k>
{
  enum {
    PAGE_SIZE= 256,
    PG_TX    = 0x40,
    PG_START = PG_TX + 9216/PAGE_SIZE,  // we allow to send jumbo frames!
    PG_STOP  = 0xc0,
    REG_ISR  = 0x7,
    BUFFER_SIZE = 32768,                // our receive buffer
  };

  #include "host/simplehwioin.h"
  #include "host/simplehwioout.h"
  DBus<MessageNetwork> &_bus_network;
  Clock * _clock;
  unsigned short _port;
  unsigned _irq;
  unsigned char _next_packet;
  unsigned char _receive_buffer[BUFFER_SIZE];
  EthernetAddr _mac;

  /**
   * Read/Write to internal ram of the network card.
   */
  void access_internal_ram(unsigned short offset, unsigned short dwords, void *buffer, bool read)
  {
    outb(0x22,   _port);              // page0 no-remote DMA, STA
    outb((offset & 0xFFU),  _port + 0x8);
    outb((offset >> 8) & 0xFFU,  _port + 0x9);
    outb((dwords*4) & 0xFFU,  _port + 0xa);
    outb(((dwords*4) >> 8) & 0xFFU,  _port + 0xb);
    outb(read ? 0xa : 0x12,   _port); // read or write remote DMA, STA

    // we use 32bit IO accesses
    if (read)
      insl (buffer, dwords, _port + 0x10);
    else
      outsl(buffer, dwords, _port + 0x10);
  }

public:
  bool send_packet(const void *buffer, unsigned len)
  {
    // is a transmit in progress or the packet to large?
    if ((inb(_port) & 4) || (len > (PG_START - PG_TX) * PAGE_SIZE)) return false;

    // send the packet out
    access_internal_ram(PG_TX * PAGE_SIZE, (len+3)/4, const_cast<void *>(buffer), false);
    outb((len & 0xFFU),  _port + 0x5);  // transmit count
    outb((len >> 8) & 0xFFU,  _port + 0x6);  // transmit count
    outb(0x26, _port);        // page0, no-dma, transmit, STA
    return true;
  }


  void reset()
  {
    // reset the card
    outb(inb(_port + 0x1f) , _port + 0x1f);

    // wait up to 1ms for the reset-completed bit in the isr
    timevalue timeout = 1 + _clock->clock(1000);
    while ((~inb(_port + REG_ISR) & 0x80) && _clock->clock(1000) < timeout)
      Cpu::pause();

    // intialize the card
    unsigned char reset_prog [] =
      {
        0x0, 0x21,      // page0, abort remote DMA, STOP
        0xe, 0x48,      // DCR in byte mode, no loopback and 4byte FIFO
        0xa, 0x00,      // zero remote byte count
        0xb, 0x00,      // zero remote byte count
        0xd, 0x02,      // transmit: loopback mode
        0xc, 0x20,      // receive:  monitor mode
        0x4, PG_TX,     // transmit start
        0x1, PG_START,  // startpg
        0x2, PG_STOP,   // stoppg
        0x3, PG_START,  // boundary
        0x7, 0xff,      // clear isr
        0xf, 0x00,      // set imr
        0x0, 0x61,      // page1, abort remote DMA, STOP
        // we do not initialize phys reg, as we use promiscuous mode later on
        0x7, PG_START+1,// CURPAGE
        0x8, 0xff,      // multicast
        0x9, 0xff,      // multicast
        0xa, 0xff,      // multicast
        0xb, 0xff,      // multicast
        0xc, 0xff,      // multicast
        0xd, 0xff,      // multicast
        0xe, 0xff,      // multicast
        0xf, 0xff,      // multicast
        0x0, 0x22,      // page0, START
        0x7, 0xff,      // clear isr
        0xf, 0x11,      // set imr to get RX and overflow IRQ
        0xd, 0x00,      // transmit: normal mode
        0xc, 0x1e,      // receive: small packets, broadcast, multicast and promiscuous
      };
    for (unsigned i=0; i < sizeof(reset_prog)/2; i++) outb(reset_prog[i*2 + 1], _port + reset_prog[i*2]);
    _next_packet = PG_START + 1;
  }


public:
  bool  receive(MessageNetwork &msg)
  {
    switch (msg.type) {
    case MessageNetwork::PACKET:
      if (msg.buffer >= _receive_buffer && msg.buffer < _receive_buffer + BUFFER_SIZE) return false;
      return send_packet(msg.buffer, msg.len);
    case MessageNetwork::QUERY_MAC:
      msg.mac = Endian::hton64(_mac.raw) >> 16;
      return true;
    default:
      return false;
    }
  }


  bool  receive(MessageIrq &msg)
  {
    if (msg.line != _irq) return false;

    // ack them
    unsigned char isr = inb(_port + REG_ISR);
    outb(isr, _port + REG_ISR);

    // packet received
    if (isr & 1)
      {

        // get current page pointer
        outb(0x62, _port);
        unsigned char current_page = inb(_port + 7);
        outb(0x22, _port);

        if (current_page != _next_packet)
          {
            // read all packets from the ring buffer
            unsigned pages;
            if (current_page >= _next_packet)
               pages = current_page - _next_packet;
            else
              pages = PG_STOP - _next_packet;
            access_internal_ram(_next_packet * PAGE_SIZE, pages * PAGE_SIZE / 4, _receive_buffer, true);

            // ring buffer wrap around?
            if (current_page < _next_packet)
              {
                access_internal_ram(PG_START * PAGE_SIZE, (current_page - PG_START) * PAGE_SIZE / 4, _receive_buffer + pages * PAGE_SIZE, true);
                pages += (current_page - PG_START);
              }

            // prog new boundary
            _next_packet = current_page;
            outb((_next_packet > PG_START) ? (_next_packet - 1) : (PG_STOP - 1), _port + 0x3);

            // now parse the packets and send them upstream
            unsigned packet_len = 0;
            for (unsigned index = 0; index < pages; index += (4 + packet_len + PAGE_SIZE - 1) / PAGE_SIZE)
              {
                unsigned offset = index*PAGE_SIZE;

                // Please note that we receive only good packages, thus the status bits are not valid!
                packet_len = _receive_buffer[offset + 2] + (_receive_buffer[offset + 3] << 8);
                assert(packet_len + offset < BUFFER_SIZE);

                MessageNetwork msg2(_receive_buffer + offset + 4, packet_len - 4, 0);
                _bus_network.send(msg2);
              }
          }
      }

    // overflow -> we simply reset the card
    if (isr & 0x10)  reset();
    return false;
  }


  HostNe2k(DBus<MessageHwIOIn> &bus_hwioin, DBus<MessageHwIOOut> &bus_hwioout, DBus<MessageNetwork> &bus_network, Clock * clock, unsigned short port, unsigned irq)
    : _bus_hwioin(bus_hwioin), _bus_hwioout(bus_hwioout), _bus_network(bus_network), _clock(clock), _port(port), _irq(irq)
  {
    reset();

    unsigned short buffer[6];
    access_internal_ram(0, 3, buffer, true);
    for (unsigned i = 0; i < 6; i++) _mac.byte[i] = buffer[i];
    Logging::printf("ne2k MAC:" MAC_FMT "\n", MAC_SPLIT(&_mac));
  }
};



PARAM_HANDLER(hostne2k,
              "hostne2k - provide ne2k-pci drivers.",
              "Example: hostne2k.")
{
  HostPci pci(mb.bus_hwpcicfg, mb.bus_hostop);
  for (unsigned bdf, num = 0; bdf = pci.search_device(0x2, 0x0, num++);)
    if (pci.conf_read(bdf, 0) == 0x802910ec)
      {
        unsigned port = pci.conf_read(bdf, HostPci::BAR0);
        // must be an ioport
        if ((port & 3) != 1 || (port >> 16)) continue;
        port &= ~3;
        unsigned irq = pci.get_gsi(mb.bus_hostop, mb.bus_acpi, bdf, 0);

        Logging::printf("bdf %x id %x port %x irq %x\n", bdf, pci.conf_read(bdf, 0), port, irq);
        MessageHostOp msg_io(MessageHostOp::OP_ALLOC_IOIO_REGION, port << 8 | 5);
        if (!mb.bus_hostop.send(msg_io))
          {
            Logging::printf("%s could not allocate ioports %x-%x\n", __PRETTY_FUNCTION__, port, port + (1 << 5) - 1);
            continue;
          }

        HostNe2k *dev = new HostNe2k(mb.bus_hwioin, mb.bus_hwioout, mb.bus_network, mb.clock(), port, irq);
        mb.bus_network.add(dev, HostNe2k::receive_static<MessageNetwork>);
        mb.bus_hostirq.add(dev, HostNe2k::receive_static<MessageIrq>);
      }
}
