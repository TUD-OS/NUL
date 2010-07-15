/**
 * Define aliases for common parameters.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
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
#include "nul/motherboard.h"


#define DEFAULT_PARAM(NAME, DESC, VALUE) PARAM(NAME, { char param [] = VALUE; mb.parse_args(param); }, #NAME " - " DESC, "value: "  VALUE)

DEFAULT_PARAM(PC_PS2, "an alias to create an PS2 compatible PC",
	      " mem:0,0xa0000 mem:0x100000 ioio nullio:0x80 pic:0x20,,0x4d0 pic:0xa0,2,0x4d1" \
	      " pit:0x40,0 scp:0x92,0x61 kbc:0x60,1,12 keyb:0,0x10000 mouse:1,0x10001 rtc:0x70,8" \
	      " serial:0x3f8,0x4,0x4711 hostsink:0x4712,80 vga:0x03c0"                        \
	      " vbios_disk vbios_keyboard vbios_mem vbios_time vbios_reset vbios_multiboot"   \
	      " msi ioapic pcihostbridge:0,0x10,0xcf8,0xe0000000 pmtimer:0x8000 vcpus")
DEFAULT_PARAM(vcpu_default, "an alias to create a default VCPU",          "vcpu halifax vbios lapic")
DEFAULT_PARAM(S0_DEFAULT,   "an alias for the default sigma0 parameters", "ioio hostacpi hostrtc pcicfg mmconfig atare hostreboot:0 hostreboot:1  hostreboot:2 hostreboot:3 hostreboot:4")
unsigned _ncpu=1;
PARAM(ncpu, _ncpu = argv[0];, "ncpu - change the number of vcpus that are created" );
PARAM(vcpus,
      for (unsigned count = 0; count < _ncpu; count++)
	{ char param[] = "vcpu_default"; mb.parse_args(param); }
      ,
      " instantiate the vcpus");

PARAM(help,
      {
	unsigned maxi = (&__param_table_end - &__param_table_start) / 2;

	Logging::printf("Supported cmdline parameters:\n");
	for (unsigned i=0; i < maxi; i++)
	  {
	    char **strings = reinterpret_cast<char **>((&__param_table_start)[i*2+1]);
	    Logging::printf("\t%2d) %s\n", i, strings[1]);
	  }

	if (argv[0] <= maxi)
	  {
	    char **strings = reinterpret_cast<char **>((&__param_table_start)[argv[0]*2+1]);
	    Logging::printf("\nHelp for '%s':\n", strings[0]);
	    for (unsigned j=1; strings[j]; j++)
	      Logging::printf("\t%s\n", strings[j]);
	  }
	else
	  Logging::printf("No valid parameter number. Use 'help:0' to give detailed help for the first parameter in the list.\n");
	Logging::printf("Binary build at '%s %s'\n", __DATE__, __TIME__);
      },
      "help:nr - prints a list of valid parameters and give detailed help for a given parameter.")
