/**
 * @file 
 * 
 *
 * Copyright (C) 2012, Michal Sojka <sojka@os.inf.tu-dresden.de>
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

#include <wvprogram.h>
#include <nul/disk_helper.h>


class DiskNameCheck : public WvProgram
{
public:
  
  void wvrun(Utcb *utcb, Hip *hip)
  {
      DiskHelper<DiskNameCheck> disk(this, 0);
  
      unsigned count = 0;
      disk.get_disk_count(*BaseProgram::myutcb(), count);
      bool match = false;
      WVSHOW(count);
      for (unsigned i=0; i<count; i++) {
        WVNUL(disk.check_name(*BaseProgram::myutcb(), i, "uuid:29485cf6-e774-44c4-bd61-bfb3208094c0", match));
        Logging::printf("%s for disk %u\n", match ? "Match" : "No match", i);
      }
  }
};
      
ASMFUNCS(DiskNameCheck, WvTest)
