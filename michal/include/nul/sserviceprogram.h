/**
 * @file 
 * Simple service in its own protection domain.
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

#include <nul/sservice.h>
#include <nul/program.h>
#include <sigma0/console.h>

template<class Session, class A, template <class Session, class A> class Base>
class SServiceProgram : public Base<Session, A>, public NovaProgram, public ProgramConsole
{
public:

  virtual cap_sel alloc_cap(unsigned count = 1)
  { return NovaProgram::alloc_cap(count); }

  virtual void    dealloc_cap(cap_sel c)
  { return NovaProgram::dealloc_cap(c); }

  virtual cap_sel create_ec4pt(phy_cpu_no cpu, Utcb **utcb_out, cap_sel ec = ~0u)
  {
    return NovaProgram::create_ec4pt(this, cpu, 0, utcb_out, ec);
  }

  SServiceProgram(const char *console_name = "service")
  {
    Hip *hip = &Global::hip;
    init(hip);
    init_mem(hip);

    console_init(console_name, new Semaphore(alloc_cap(), true));
  }

  NORETURN
  void run(Utcb *utcb, Hip *hip)
  {
    block_forever();
  }
};
