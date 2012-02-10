/**
 * @file 
 * Periodically start and stop an application
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

#include "nul/service_config.h"
#include "nul/service_admission.h"
#include <wvprogram.h>
#include "nul/service_fs.h"

volatile unsigned count;

NORETURN void watchdog() 
{
  while(1) {
    //Logging::printf("I'm here %u\n", count);
  }
}


class StartStop : public WvProgram
{
public:
  void wvrun(Utcb *utcb, Hip *hip)
  {
    ConfigProtocol service_config(alloc_cap(ConfigProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));


    unsigned portal_num = FsProtocol::CAP_SERVER_PT + Global::hip.cpu_count();
    unsigned cap_base = alloc_cap(portal_num);
    FsProtocol fs_obj(cap_base, "fs/rom");
    FsProtocol::File file_obj(fs_obj, alloc_cap());

    const char *cmdline = reinterpret_cast<const char *>(hip->get_mod(0)->aux);
    WVSHOW(cmdline);
    cmdline += strcspn(cmdline, " \t\r\n\f");
    cmdline +=  strspn(cmdline, " \t\r\n\f");
    WVPASS(strncmp("rom://", cmdline, 6) == 0);
    const char *filename = cmdline + 6;
    WVSHOW(filename);

    WVNUL(fs_obj.get(*utcb, file_obj, filename, strlen(filename)));
    FsProtocol::dirent fileinfo;
    WVNUL(file_obj.get_info(*utcb, fileinfo));

    char *nulconfig = new(4096) char[fileinfo.size];
    WVPASS(nulconfig != nullptr);
    WVNUL(file_obj.copy(*utcb, nulconfig, fileinfo.size));

    // Create a thread on CPU0 (we should be on CPU1) which monitors when we die on "Out of memory"
    cap_sel ec;
    phy_cpu_no cpu = 0;
    Utcb *u = 0;
    WVPASS(ec = create_ec_helper(this, cpu, 0, &u, reinterpret_cast<void *>(&watchdog)));
    AdmissionProtocol::sched sched(AdmissionProtocol::sched::TYPE_APERIODIC);
    AdmissionProtocol service_admission(alloc_cap(AdmissionProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));
    WVNUL(service_admission.set_name(*utcb, "startstop"));
    WVNUL(service_admission.alloc_sc(*utcb, ec, sched, cpu, "watchdog"));

    unsigned short id = 0;
    unsigned long mem = 0;
    cap_sel scs_usage = alloc_cap();
    for (count = 0; count < 10000; count++) {
      Logging::printf("#%u\n", count);
      unsigned start_res = service_config.start_config(*utcb, id, mem, scs_usage, nulconfig, fileinfo.size);
      if (start_res) { WVNUL(start_res); break; }
      unsigned kill_res = service_config.kill(*utcb, id);
      if (kill_res) { WVNUL(kill_res); break; }
    }
  }
};

ASMFUNCS(StartStop, WvTest)
