/**
 * @file
 * A simple service for educational purposes that does nothing useful;
 * this implementation is based on generic class SServiceProgram.
 *
 * This is probably the simplest possible service. It does not
 * maintain any per-client state and as such it does not require the
 * client to open sessions.
 *
 * Copyright (C) 2011, Michal Sojka <sojka@os.inf.tu-dresden.de>
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

class EchoService : public SServiceProgram
{
private:
  // This construct can be used to store per-client data
  struct SessionData : public GenericClientData {
    unsigned last_val;		// Last value sent by client
  };

  typedef ClientDataStorage<SessionData, EchoService> Sessions;
  Sessions _sessions;

public:
  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap)
    {
      unsigned op, res;
      check1(EPROTO, input.get_word(op));

      switch (op) {
      case ParentProtocol::TYPE_OPEN:
      case ParentProtocol::TYPE_CLOSE:
	return handle_sessions(op, input, _sessions, free_cap);

      case EchoProtocol::TYPE_ECHO:
      {
	Sessions::Guard guard_c(&_sessions, utcb, this);
	SessionData *data = 0;
	if (res = _sessions.get_client_data(utcb, data, input.identity())) {
	  // Return EEXISTS to ask the client for opening the session
	  Logging::printf("Cannot get client (id=0x%x) data: 0x%x\n", input.identity(), res);
	  return res;
	}

	unsigned value;
	check1(EPROTO, input.get_word(value));
	// Get the value sent by a client
	Logging::printf("echo: Client 0x%x sent us a value %d\n", input.identity(), value);
	data->last_val = value; // Remember the received value
	return value; // "Echo" the received value back
      }
      case EchoProtocol::TYPE_GET_LAST:
      {
	SessionData *data = 0;
	Sessions::Guard guard_c(&_sessions, utcb, this);
	if (res = _sessions.get_client_data(utcb, data, input.identity())) {
	  Logging::printf("echo: Client %d: Cannot get client data to retrieve the value\n",
			  input.identity());
	  return res;
	}
	else
	  return data->last_val; // Return the remembered value
      }
      default:
	Logging::panic("Unknown op!!!!\n");
        return EPROTO;
      }
    }

  EchoService() : SServiceProgram("Echo service") {
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + Global::hip.cpu_count()));
    register_service(this, "/echo", Global::hip);
  }
};

ASMFUNCS(EchoService, WvTest)
