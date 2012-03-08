/**
 * @file 
 * A simple service for educational purposes that does nothing useful.
 *
 * This file demonstrates how to start service on all CPUs, and how to
 * maintain data for every client.
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
private:

  // This construct can be use to store per-client data
  struct ClientData : public GenericClientData {
    unsigned last_val;		// Last value sent by client
  };

  typedef ClientDataStorage<ClientData, EchoService> EchoClientDataStorage;

  EchoClientDataStorage _storage;
  char * flag_revoke; // Share memory with parent to signalize us dead clients
public:

  EchoService() : NovaProgram(), ProgramConsole() {}

  inline unsigned alloc_crd() { return Crd(alloc_cap(), 0, DESC_CAP_ALL).value(); }

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap, cap_sel pid)
    {
      unsigned op, res;
      check1(EPROTO, input.get_word(op));

      switch (op) {
      case ParentProtocol::TYPE_OPEN:
      {
        unsigned pseudonym = input.received_cap();
	verbose("echo2: ParentProtocol::TYPE_OPEN from pseudonym %#x\n", pseudonym);
        unsigned cap_session = 0;

        if (!pseudonym) return EPROTO;

        //Ask parent whether we have already a session with this client
        res = ParentProtocol::check_singleton(utcb, pseudonym, cap_session);
        if (!res && cap_session)
        {
          ClientData *data = 0;
          EchoClientDataStorage::Guard guard_c(&_storage, utcb, this);
          while (data = _storage.next(data)) {
            if (data->get_identity() == cap_session) {
              dealloc_cap(data->pseudonym); //replace old pseudonym, first pseudnym we got via parent and gets obsolete as soon as client becomes running
              data->pseudonym = pseudonym;
              utcb << Utcb::TypedMapCap(data->get_identity());
              free_cap = false;
	      _storage.cleanup_clients(utcb, this);
              return ENONE;
            }
          }
        }

        ClientData *data = 0;
        res = _storage.alloc_client_data(utcb, data, pseudonym, this);
        if (res == ERESOURCE) { _storage.cleanup_clients(utcb, this); return ERETRY; } //force garbage collection run
        else if (res) return res;

        if (*flag_revoke) { *flag_revoke = 0; _storage.cleanup_clients(utcb, this); }

        res = ParentProtocol::set_singleton(utcb, data->pseudonym, data->get_identity());
        assert(!res);

        free_cap = false;
        verbose("echo2: created echo client %p, pseudonym=0x%x, identity=0x%x\n", data, data->pseudonym, data->get_identity());
        utcb << Utcb::TypedMapCap(data->get_identity());
        return res;
      }
      case ParentProtocol::TYPE_CLOSE:
      {
	verbose("echo2: ParentProtocol::TYPE_CLOSE, from id %#x\n", input.identity());
        ClientData *data = 0;
        EchoClientDataStorage::Guard guard_c(&_storage, utcb, this);
        check1(res, res = _storage.get_client_data(utcb, data, input.identity()));
        return _storage.free_client_data(utcb, data, this);
      }
      case EchoProtocol::TYPE_ECHO:
      {
	verbose("echo2: EchoProtocol::TYPE_ECHO from id %#x\n", input.identity());

	EchoClientDataStorage::Guard guard_c(&_storage);
	ClientData *data = 0;
	if (res = _storage.get_client_data(utcb, data, input.identity())) {
	  // Return EEXISTS to ask the client for opening the session
	  verbose("echo2: Cannot get client data: err=%#x\n", res);
	  return res;
	}

	unsigned value;
	check1(EPROTO, input.get_word(value));
	// Get the value sent by a client
	verbose("echo2: Client 0x%x sent us value %d\n", input.identity(), value);
	data->last_val = value; // Remember the received value
	return value; // "Echo" the received value back
      }
      case EchoProtocol::TYPE_GET_LAST:
      {
	verbose("echo2: EchoProtocol::TYPE_GET_LAST from id %#x\n", input.identity());
	ClientData *data = 0;
	EchoClientDataStorage::Guard guard_c(&_storage);
	if (res = _storage.get_client_data(utcb, data, input.identity())) {
	  Logging::printf("echo: Client %d: Cannot get client data to retrieve the value\n",
			  input.identity());
	  return res;
	}
	utcb << data->last_val; // Reply with the remembered value (it will appear in utcb.msg[1])
	return ENONE;		// The returned value will appear in utcb.msg[0]
      }
      default:
	Logging::printf("echo2: Unknown op!!!!\n");
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

      flag_revoke = new (0x1000) char[0x1000];
      if (!flag_revoke) return false;

      // Create worker threads and portals for each CPU
      for (unsigned cpunr = 0; cpunr < hip->cpu_desc_count(); cpunr++) {
        Hip_cpu *cpu = reinterpret_cast<Hip_cpu *>(reinterpret_cast<char *>(hip) + hip->cpu_offs + cpunr*hip->cpu_size);
        if (~cpu->flags & 1) continue; // Skip disabled CPUs

        exc_base_worker    = alloc_cap(16);

        if (!exc_base_worker) return false;
        pt_worker    = alloc_cap();

        unsigned cap_worker_ec = create_ec4pt(this, cpunr, exc_base_worker, &utcb_worker, alloc_cap());
        if (!cap_worker_ec) return false;

        utcb_worker->head.crd = alloc_crd();
        utcb_worker->head.crd_translate = Crd(0, 31, DESC_CAP_ALL).value();

        unsigned long portal_func = reinterpret_cast<unsigned long>(StaticPortalFunc<EchoService>::portal_func);
        res = nova_create_pt(pt_worker, cap_worker_ec, portal_func, 0);
        if (res) return false;
        res = ParentProtocol::register_service(*utcb, service_name, cpunr, pt_worker, service_cap, flag_revoke);
        if (res) return !res;
      }

      return true;
    }

  NORETURN
  void run(Utcb *utcb, Hip *hip)
  {

    init(hip);
    init_mem(hip);

    console_init("Echo2 Service", new Semaphore(alloc_cap(), true));
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_count()));
    if (!start_service(utcb, hip))
      Logging::printf("failure - starting echo2 service\n");

    Logging::printf("Echo2 service successfully started\n");

    block_forever();
  }
};

ASMFUNCS(EchoService, WvTest)

