/** @file
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
#include <service/acpi.h>

class BasicHpet {
public:

  enum {
    // General Configuration Register
    ENABLE_CNF = (1U << 0),
    LEG_RT_CNF = (1U << 1),

    // General Capabilities Register
    LEG_RT_CAP = (1U << 15),
    BIT64_CAP  = (1U << 13),

    // Timer Configuration
    FSB_INT_DEL_CAP = (1U << 15),
    FSB_INT_EN_CNF  = (1U << 14),

    MODE32_CNF      = (1U << 8),
    PER_INT_CAP     = (1U << 4),
    TYPE_CNF        = (1U << 3),
    INT_ENB_CNF     = (1U << 2),
    INT_TYPE_CNF    = (1U << 1),

  };

  struct HostHpetTimer {
    volatile uint32 config;
    volatile uint32 int_route;
    union {
      volatile uint32 comp[2];
      volatile uint64 comp64;
    };
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
   * Get the HPET address from the ACPI table. Returns 0 on failure.
   */
  static unsigned long get_hpet_address(DBus<MessageAcpi> &bus_acpi)
  {
    MessageAcpi msg0("HPET");
    if (bus_acpi.send(msg0, true) and msg0.table) {
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

    return 0;
  }

  /** Try to find out HPET routing ID. Returns 0 on failure.
   *
   * HPET routing IDs are stored in the DMAR table and we need them
   * for interrupt remapping to work. This method is complicated by
   * weird BIOSes that give each comparator its own RID. Our heuristic
   * is to check, whether we see multiple device scope entries per
   * ID. If this is the case, we assume that each comparator has a
   * different RID.
   */
  static uint16 get_hpet_rid(DBus<MessageAcpi> &bus_acpi, unsigned block, unsigned comparator)
  {
    MessageAcpi msg0("DMAR");
    if (not bus_acpi.send(msg0, true) or not msg0.table)
      return 0;

    DmarTableParser p(msg0.table);
    DmarTableParser::Element e = p.get_element();

    uint16 first_rid_found = 0;

    do {
      if (e.type() == DmarTableParser::DHRD) {
        DmarTableParser::Dhrd dhrd = e.get_dhrd();
        
        if (dhrd.has_scopes()) {
          DmarTableParser::DeviceScope s = dhrd.get_scope();
          do {
            if (s.type() == DmarTableParser::MSI_CAPABLE_HPET) {
              // We assume the enumaration IDs correspond to timer blocks.
              // XXX Is this true? We haven't seen an HPET with
              // multiple blocks yet.
              if (s.id() != block) continue;

              if (first_rid_found == 0)
                first_rid_found = s.rid();
              
              // Return the RID for the right comparator.
              if (comparator-- == 0)
                return s.rid();
            }
          } while (s.has_next() and ((s = s.next()), true));
        }
      }
    } while (e.has_next() and ((e = e.next()), true));
    
    // When we get here, either we haven't found a single RID or only
    // one. For the latter case, we assume it's the right one.
    return first_rid_found;
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
