/**
 * HPET register layout.
 *
 * Copyright (C) 2009-2010, Bernhard Kauer <bk@vmmon.org>
 * Copyright (C) 2011, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

#pragma once

#include <nul/types.h>
#include <nul/motherboard.h>

class BasicHpet {
public:

  struct HostHpetTimer {
    volatile uint32 config;
    volatile uint32 int_route;
    volatile uint32 comp[2];
    volatile uint32 msi[2];
    uint32 res[2];
  };

  struct HostHpetRegister {
    volatile uint32 cap;
    volatile uint32 period;
    uint32 res0[2];
    volatile uint32 config;
    uint32 res1[3];
    volatile uint32 isr;
    uint32 res2[51];
    union {
      volatile uint32 counter[2];
      volatile uint64 main;
    };
    uint32 res3[2];
    HostHpetTimer timer[24];
  };


  /**
   * Get the HPET address from the ACPI table.
   */
  static unsigned long get_hpet_address(DBus<MessageAcpi> &bus_acpi, unsigned long address_overrride)
  {
    if (address_overrride != ~0ul) return address_overrride;

    MessageAcpi msg0("HPET");
    if (bus_acpi.send(msg0, true) && msg0.table)
      {
	struct HpetAcpiTable
	{
	  char res[40];
	  unsigned char gas[4];
          uint32 address[2];
	} *table = reinterpret_cast<HpetAcpiTable *>(msg0.table);

	if (table->gas[0])
	  Logging::printf("HPET access must be MMIO but is %d", table->gas[0]);
	else if (table->address[1])
	  Logging::printf("HPET must be below 4G");
	else
	  return table->address[0];
      }

    Logging::printf("Warning: no HPET ACPI table, trying default value 0xfed00000\n");
    return 0xfed00000;
  }


  /**
   * Check whether some address points to an hpet.
   */
  static bool check_hpet_present(void *address, unsigned timer, unsigned irq)
  {
    HostHpetRegister *regs = reinterpret_cast<HostHpetRegister *>(address);

    // check whether this looks like an HPET
    if (regs->cap == ~0u || !(regs->cap & 0xff))
      Logging::printf("%s: Invalid HPET cap\n", __func__);
    else {
      unsigned num_timer = ((regs->cap & 0x1f00) >> 8) + 1;
      if (timer >= num_timer)
	Logging::printf("%s: Timer %x not supported\n", __func__, timer);

      // output some debugging
      Logging::printf("HostHpet: cap %x config %x period %d\n", regs->cap, regs->config, regs->period);
      for (unsigned i=0; i < num_timer; i++)
	Logging::printf("\tHpetTimer[%d]: config %x int %x\n", i, regs->timer[i].config, regs->timer[i].int_route);

      if (timer >= num_timer)
	Logging::printf("%s: Timer %x not supported\n", __func__, timer);
      else if (regs->period > 0x05f5e100 || !regs->period)
	Logging::printf("%s: Invalid HPET period\n", __func__);
      else if ((irq != ~0u) && (irq >= 32 || ~regs->timer[timer].int_route & (1 << irq)))
	Logging::printf("%s: IRQ routing to GSI %x impossible\n", __func__, irq);
      else if (~regs->timer[timer].config & (1<<15) && !regs->timer[timer].int_route && !(regs->cap & 0x8000 && timer < 2))
	Logging::printf("%s: No IRQ routing possible\n", __func__);
      else
	return true;
    }
    return false;
  }

};

/* EOF */
