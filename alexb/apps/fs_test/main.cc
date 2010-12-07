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
#include <nul/service_fs.h>
#include <sigma0/sigma0.h> // Sigma0Base object
#include <sigma0/console.h>

namespace ab {

class DummyFS : public NovaProgram, public ProgramConsole
{
  public:

  static unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap)
    {
      unsigned op, len;
      check1(EPROTO, input.get_word(op));

      switch (op) {
      case FsProtocol::TYPE_GET_FILE_INFO:
        {
          const char * name = input.get_zero_string(len);
          unsigned long long size = 0;
          if (!name || !len) return EPROTO;
          Logging::printf("file info - file '%s'\n", name);

          utcb << size;
        }
        return ENONE;
      case FsProtocol::TYPE_GET_FILE_MAPPED:
        {
          const char * name = input.get_zero_string(len);
          if (!name || !len) return EPROTO;
          Logging::printf("file map - file '%s'\n", name);
          //mapping code to place here
        }
        return ENONE;
      default:
        return EPROTO;
      }
    }

  bool use_filesystem (Utcb *utcb, Hip * hip)
    {
      //server part - start
      const char * service_name = "/dummy";
      unsigned service_cap = alloc_cap();
      unsigned pt = alloc_cap();
      unsigned res;
      unsigned long object = 0;

	    Utcb *worker_utcb;
	    unsigned cap_ec = create_ec_helper(object, utcb->head.nul_cpunr, 0, &worker_utcb);
      if (!cap_ec) return false;
	    worker_utcb->head.crd = alloc_cap() << Utcb::MINSHIFT | DESC_TYPE_CAP;

      unsigned long portal_func = reinterpret_cast<unsigned long>(StaticPortalFunc<DummyFS>::portal_func);

      res = nova_create_pt(pt, cap_ec, portal_func, 0);
      if (res) return false;

	    res = ParentProtocol::register_service(*utcb, service_name, utcb->head.nul_cpunr, pt, service_cap);
      Logging::printf("%s - registing service\n", (res == 0 ? "success" : "failure"));
      if (res) return !res;
      //service part - end

      //client part
      FsProtocol * rom_fs = new FsProtocol(alloc_cap(FsProtocol::CAP_NUM), "fs/dummy");

      FsProtocol::dirent fileinfo;
      res = rom_fs->get_file_info(*utcb, "myfile", fileinfo);
      Logging::printf("fs %x dirent.size %llx\n", res, fileinfo.size);
      if (res) return false;
      
      return true;
    }

  void run(Utcb *utcb, Hip *hip)
    {

      init(hip);
      init_mem(hip);

      console_init("dummy fs");

      Logging::printf("Hello\n");

      _virt_phys.debug_dump("");

      if (!use_filesystem(utcb, hip))
        Logging::printf("failed - starting fs failed \n");
    }
};

} /* namespace */

ASMFUNCS(ab::DummyFS, NovaProgram)
