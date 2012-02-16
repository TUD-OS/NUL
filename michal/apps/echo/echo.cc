/**
 * @file 
 * A simple service for educational purposes that does nothing useful.
 *
 * This is probably the simplest possible service. It does not
 * maintain any per-client state and as such it does not require the
 * client to open sessions.
 * 
 * Copyright (C) 2011, 2012, Michal Sojka <sojka@os.inf.tu-dresden.de>
 * Copyright (C) 2011, Alexander Boettcher <boettcher@tudos.org>
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
#include <sigma0/sigma0.h> // Sigma0Base object
#include <sigma0/console.h>

#include <nul/compiler.h>
#include "service_echo.h"
#include <nul/timer.h>
#include <nul/service_timer.h>
#include <service/math.h>

#include <host/keyboard.h>
#include <wvtest.h>

#ifdef QUIET
#define verbose(...)
#else
#define verbose(...) Logging::printf(__VA_ARGS__)
#endif

class EchoService : public NovaProgram, public ProgramConsole
{
public:

  EchoService() : NovaProgram(), ProgramConsole() {}

  inline unsigned alloc_crd() { return Crd(alloc_cap(), 0, DESC_CAP_ALL).value(); }

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap, cap_sel pid)
    {
      unsigned op;
      check1(EPROTO, input.get_word(op));

      switch (op) {
      case EchoProtocol::TYPE_ECHO:
      {
	unsigned value;

	check1(EPROTO, input.get_word(value)); // Get the value sent by a client
	verbose("echo: Client 0x%x sent us a value %d\n", input.identity(), value);
	// "Echo" the received value back. Beware that this might
	// clash with error codes defined in error.h
	return value;
      }
      default:
	Logging::printf("echo: Unknown op!!!!\n");
        return EPROTO;
      }
    }

  bool start_service (Utcb *utcb, Hip * hip)
    {
      const char * service_name = "/echo";
      unsigned res;
      unsigned exc_base_worker, pt_worker;
      unsigned service_cap = alloc_cap();
      Utcb *utcb_worker;

      // Create worker threads and portals for each CPU
      for (unsigned cpunr = 0; cpunr < hip->cpu_desc_count(); cpunr++) {
        Hip_cpu *cpu = reinterpret_cast<Hip_cpu *>(reinterpret_cast<char *>(hip) + hip->cpu_offs + cpunr*hip->cpu_size);
        if (~cpu->flags & 1) continue; // Skip disabled CPUs

        exc_base_worker    = alloc_cap(16);

        if (!exc_base_worker)
	  return false;

        pt_worker    = alloc_cap();

        unsigned cap_worker_ec = create_ec4pt(this, cpunr, exc_base_worker, &utcb_worker, alloc_cap());
        if (!cap_worker_ec)
	  return false;

        utcb_worker->head.crd = Crd(alloc_cap(), 0, DESC_CAP_ALL).value();
        utcb_worker->head.crd_translate = 0;

        unsigned long portal_func = reinterpret_cast<unsigned long>(StaticPortalFunc<EchoService>::portal_func);
        res = nova_create_pt(pt_worker, cap_worker_ec, portal_func, 0);
        if (res)
	  return false;

        res = ParentProtocol::register_service(*utcb, service_name, cpunr, pt_worker, service_cap);

        if (res)
	  return !res;
      }
      return true;
    }

  NORETURN
  void run(Utcb *utcb, Hip *hip)
  {

    init(hip);
    init_mem(hip);

    console_init("Echo Service", new Semaphore(alloc_cap(), true));
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_count()));
    if (!start_service(utcb, hip))
      Logging::printf("failure - starting echo service\n");

    Logging::printf("Echo service successfully started\n");

    block_forever();
  }
};

ASMFUNCS(EchoService, WvTest)

