/*
 * Copyright (C) 2011, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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
#include <sigma0/sigma0.h>
#include <sigma0/console.h>

#include <nul/service_bridge.h>

class Bridge : public NovaProgram, public ProgramConsole
{
public:

  void run(Utcb *utcb, Hip *hip)
  {

    init(hip);
    init_mem(hip);

    console_init("Bridge", new Semaphore(alloc_cap(), true));
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));

    Logging::printf("booting - bridge ...\n");


  }
};

ASMFUNCS(Bridge, NovaProgram)

// EOF
