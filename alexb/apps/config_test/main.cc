/*
 * (C) 2011 Alexander Boettcher
 *     economic rights: Technische Universitaet Dresden (Germany)
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
#include <nul/service_config.h> //ConfigService
#include <sigma0/sigma0.h> // Sigma0Base object
#include <sigma0/console.h>
#include "nul/service_log.h"

namespace ab {

class TestConfig : public NovaProgram, public ProgramConsole
{
  public:

  bool use_config (Utcb *utcb, Hip * hip)
    {
      char *args = reinterpret_cast<char *>(hip->get_mod(0)->aux);
      char * name = strstr(args, "rom://");
      if (!name) return false;
      name +=6;
      char * end = strstr(name, " ");
      if (!end) return false;
      char * dir = strstr(name, "/nova/bin/test_lwip.nul");
      if (!dir || dir > end) return false;

      char const * const_config [] = {
        "/nova/bin/vancouver.nul.gz sigma0::log sigma0::mem:256 82576vf_vnet PC_PS2 sigma0::dma sigma0::log name::/s0/log name::/s0/timer name::/s0/fs/rom || ",
        "/nova/tools/munich || ",
        "/nova/linux/bzImage-js clocksource=tsc || ",
        "/nova/linux/initrd-js.lzma",
        0
      };
      char * _config = new (0x1000) char[0x1000];
      char const * config = _config;
      unsigned i = 0;

      while (const_config[i]) {
        memcpy(_config, "rom://", 6); _config += 6;
        memcpy(_config, name, dir - name); _config += dir - name;
        memcpy(_config, const_config[i], strlen(const_config[i])); _config += strlen(const_config[i]);
        i ++;
      }
      *_config = 0;

      Logging::printf("cmdline %x %s\n", strlen(config), config);

      ConfigProtocol *service_config = new ConfigProtocol(alloc_cap(ConfigProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));

      unsigned short id;
      unsigned long mem;
      cap_sel scs_usage = alloc_cap();
      return (!service_config->start_config(*utcb, id, mem, scs_usage, config));
    }

  void run(Utcb *utcb, Hip *hip)
  {

    init(hip);
    init_mem(hip);

    console_init("Config test", new Semaphore(alloc_cap(), true));
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));

    Logging::printf("Hello\n");

    _virt_phys.debug_dump("");

    if (!use_config(utcb, hip)) 
      Logging::printf("failed  - starting config service\n");
  }
};

} /* namespace */

ASMFUNCS(ab::TestConfig, NovaProgram)
