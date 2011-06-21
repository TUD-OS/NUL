/*
 * (C) 2010 Alexander Boettcher
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <nul/program.h>
#include <nul/service_events.h>
#include <sigma0/console.h>

namespace ab {
class EventTest : public NovaProgram, public ProgramConsole
{
  public:

  bool use_events (Utcb *utcb, Hip * hip)
    {

      //client part
      EventsProtocol * events = new EventsProtocol(alloc_cap(EventsProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));
      events->send_event(*utcb, 0xa0a0);
      events->destroy(*utcb, EventsProtocol::CAP_SERVER_PT + hip->cpu_desc_count(), this);
      return true;

    }

  void run(Utcb *utcb, Hip *hip)
    {

      init(hip);
      init_mem(hip);

      console_init("test event service", new Semaphore(alloc_cap(), true));

      Logging::printf("Hello\n");

//      _virt_phys.debug_dump("");

      if (!use_events(utcb, hip))
        Logging::printf("failed - starting event test\n");
    }
};

} /* namespace */

ASMFUNCS(ab::EventTest, NovaProgram)
