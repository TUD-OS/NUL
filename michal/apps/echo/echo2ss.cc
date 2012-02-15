/**
 * @file
 * A simple service for educational purposes that does nothing useful;
 * this implementation is based on generic class SServiceProgram.
 *
 * Copyright (C) 2011, 2012, Michal Sojka <sojka@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL (NOVA user land).
 *
 * NUL is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <nul/program.h>
#include <sigma0/console.h>

#include "service_echo.h"
#include <nul/sserviceprogram.h>
#include <wvtest.h>

#ifdef QUIET
#define verbose(...)
#else
#define verbose(...) Logging::printf(__VA_ARGS__)
#endif

#ifdef NOXLATE
#define BASE_SSERVICE NoXlateSService
#define BASE_CLIENT   PerCpuIdClientData
#else
#define BASE_SSERVICE XlateSService
#define BASE_CLIENT   GenericClientData
#endif

// This construct can be used to store per-client data
struct EchoClient : public BASE_CLIENT {
  unsigned last_val;		// Last value sent by client
};

class EchoService : public SServiceProgram<EchoClient, EchoService, BASE_SSERVICE>
{
  virtual unsigned new_session(EchoClient *client) {
    verbose("echo: Opened a new session for client %p\n", client);
    return ENONE;
  }

  virtual unsigned handle_request(EchoClient *client, unsigned op, Utcb::Frame &input, Utcb &utcb, bool &free_cap)
  {
    switch (op) {
    case EchoProtocol::TYPE_ECHO: {
      unsigned value;
      check1(EPROTO, input.get_word(value));
      // Get the value sent by a client
      verbose("echo: Client %p sent us value %d\n", client, value);
      client->last_val = value; // Remember the received value
      return value; // "Echo" the received value back
    }
    case EchoProtocol::TYPE_GET_LAST:
      utcb << client->last_val; // Reply with the remembered value (it will appear in utcb.msg[1])
      return ENONE; 		// The returned value will appear in utcb.msg[0]
    default:
      Logging::panic("Unknown op!!!!\n");
      return EPROTO;
    }
  }

public:
  EchoService() : SServiceProgram<EchoClient, EchoService, BASE_SSERVICE>("Echo service") {
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + Global::hip.cpu_count()));
    register_service("/echo");
  }
};

ASMFUNCS(EchoService, WvTest)
