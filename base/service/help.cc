/** @file
 * Define help output.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
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
#include "nul/motherboard.h"

PARAM_HANDLER(help, "help:nr - prints a list of valid parameters and give detailed help for a given parameter.")
{
  unsigned maxi = (&__param_table_end - &__param_table_start) / 2;

  Logging::printf("Supported cmdline parameters:\n");
  for (unsigned i=0; i < maxi; i++)
    {
      char **strings = reinterpret_cast<char **>((&__param_table_start)[i*2+1]);
      Logging::printf("\t%2d) %s\n", i, strings[1] ? strings[1] : strings[0]);
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
#ifndef NO_TIMESTAMP
  Logging::printf("Binary build at '%s %s'\n", __DATE__, __TIME__);
#endif
}
