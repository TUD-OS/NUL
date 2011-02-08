/*
 * Tracebuffer for the clients.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver.nova is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * Vancouver.nova is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include "nul/motherboard.h"
#include "nul/service_log.h"
#include "nul/capalloc.h"

/**
 * Tracebuffer service.
 *
 * Missing: trace-buffer output on debug key
 */
class Tracebuffer: public CapAllocator<Tracebuffer> {
  unsigned long _size;
  unsigned long _pos;
  char        * _buf;
  bool          _verbose;
  long          _anon_sessions;

  struct ClientData : public GenericClientData {
    long guid;
  };

  __attribute__((aligned(8))) ClientDataStorage<ClientData, Tracebuffer> _storage;

  static void trace_putc(void *data, int value) {
    if (value < 0) return;
    Tracebuffer *t = reinterpret_cast<Tracebuffer *>(data);
    t->_buf[(t->_pos++) % t->_size] = value;
  }


  void trace_printf(const char *format, ...)
  {
    va_list ap;
    va_start(ap, format);
    Vprintf::vprintf(trace_putc, this, format, ap);
    va_end(ap);
  }

public:
  inline unsigned alloc_crd() { return alloc_cap() << Utcb::MINSHIFT | DESC_TYPE_CAP; }

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap) {
    unsigned res = ENONE;
    unsigned op;

    check1(EPROTO, input.get_word(op));
    //Logging::printf("%s words %x/%x id %x %x\n", __PRETTY_FUNCTION__, input.untyped(), input.typed(),  input.identity(), op);
    switch (op) {
    case ParentProtocol::TYPE_OPEN:
      {
        ClientData *data = 0;
        check1(res, res = _storage.alloc_client_data(utcb, data, input.received_cap()));
        free_cap = false;
        if (ParentProtocol::get_quota(utcb, data->pseudonym, "guid", 0, &data->guid))
          data->guid = --_anon_sessions;
        utcb << Utcb::TypedMapCap(data->identity);
        if (_verbose) Logging::printf("tb: client data %x guid %lx parent %x\n", data->identity, data->guid, data->pseudonym);
        return res;
      }
    case ParentProtocol::TYPE_CLOSE:
      {
        ClientData volatile *data = 0;
        ClientDataStorage<ClientData, Tracebuffer>::Guard guard(&_storage, utcb);
        check1(res, res = _storage.get_client_data(utcb, data, input.identity()));

        if (_verbose) Logging::printf("tb: close session for %lx\n", data->guid);
        return _storage.free_client_data(utcb, data);
      }
    case LogProtocol::TYPE_LOG:
      {
        unsigned len;
        ClientData volatile *data = 0;
        ClientDataStorage<ClientData, Tracebuffer>::Guard guard(&_storage, utcb);
        if ((res = _storage.get_client_data(utcb, data, input.identity())))  return res;

        char *text = input.get_string(len);
        check1(EPROTO, !text);
        if (_verbose) Logging::printf("tb: (%ld) %.*s\n", data->guid, len, text);
        trace_printf("(%ld) %.*s\n", data->guid, len, text);
      }
      return ENONE;
    default:
      return EPROTO;
    }
  }

  void * operator new (unsigned size, unsigned alignment) { return  new (alignment) char [sizeof(Tracebuffer)]; }

public:
  Tracebuffer(unsigned long size, char *buf, bool verbose, unsigned _cap, unsigned _cap_order)
    : CapAllocator<Tracebuffer> (_cap, _cap, _cap_order), _size(size), _pos(0), _buf(buf), _verbose(verbose) {
  }


#if 0
      case 4:
	Logging::printf("Trace buffer at %x bytes (%x).\n\n", _trace_pos, TRACE_BUF_SIZE);
	Logging::printf("%.*s", TRACE_BUF_SIZE - (_trace_pos % TRACE_BUF_SIZE), _trace_buf + (_trace_pos % TRACE_BUF_SIZE));
	if (_trace_pos % TRACE_BUF_SIZE) Logging::printf("%.*s", _trace_pos % TRACE_BUF_SIZE, _trace_buf);
	Logging::printf("\nEOF trace buffer\n\n");
	break;
      }
#endif
};

PARAM(tracebuffer,
      unsigned long size = ~argv[0] ? argv[0] : 32768;
      unsigned cap_region = alloc_cap_region(1 << 12, 12);

      Tracebuffer *t = new (8) Tracebuffer(size, new char[size], argv[1] == ~0UL ? false : argv[1] , cap_region, 12);
      MessageHostOp msg(t, "/log", reinterpret_cast<unsigned long>(StaticPortalFunc<Tracebuffer>::portal_func));
      msg.crd_t = Crd(cap_region, 12, DESC_TYPE_CAP).value();
      if (!cap_region || !mb.bus_hostop.send(msg))
        Logging::panic("registering the service failed");
      ,
      "tracebuffer:size=32768,verbose=1 - instanciate a tracebuffer for the clients")
