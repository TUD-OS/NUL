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
#include <nul/generic_service.h>
#include <sigma0/sigma0.h> // Sigma0Base object
#include <sigma0/console.h>

namespace ab {

class DummyFS : public NovaProgram, public ProgramConsole
{
  private:
    RegionList<512> * cap_range;
    #define RECV_WINDOW_ORDER 22
    #define RECV_WINDOW_SIZE (1 << RECV_WINDOW_ORDER)

  public:

  void init_service() {
    cap_range = new RegionList<512>;
    cap_range->add(Region(0x1000,0x3000));
  }

  inline unsigned alloc_cap(unsigned num = 1) { return cap_range->alloc(num, 0); }

  unsigned alloc_crd() {
    unsigned long addr = _free_virt.alloc(RECV_WINDOW_SIZE, RECV_WINDOW_ORDER);
    if (!addr) Logging::panic("out of free virt space\n");
    return Crd(addr >> Utcb::MINSHIFT, RECV_WINDOW_ORDER - 12, DESC_MEM_ALL).value();
  }

  static unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap, cap_sel pid)
    {
      unsigned op, len;
      check1(EPROTO, input.get_word(op));
      const char rawfile[]= "TestTest!!!";

      switch (op) {
      case FsProtocol::TYPE_INFO:
        {
          const char * name = input.get_zero_string(len);
          if (!name || !len) return EPROTO;
          Logging::printf("file info - file size=0x%x '%s' \n", sizeof(rawfile), name);
          unsigned long long size = sizeof(rawfile);
          utcb << size;
        }
        return ENONE;
      case FsProtocol::TYPE_COPY:
        {
          const char * name = input.get_zero_string(len);
          if (!name || !len) return EPROTO;

          unsigned long foffset;
          check1(EPROTO, input.get_word(foffset));
          if (foffset > sizeof(rawfile)) return ERESOURCE;

          Logging::printf("file map - file '%s'\n", name);

          //XXX you may get multiple items !
          unsigned long addr = input.received_item(0);
          if (!(addr >> 12)) return EPROTO;

          input.dump_typed_items();

          unsigned long long csize = sizeof(rawfile) - foffset;
          if (RECV_WINDOW_SIZE < csize) csize = RECV_WINDOW_SIZE;
          if ((1ULL << (12 + ((addr >> 7) & 0x1f))) < csize) csize = 1ULL << (12 + ((addr >> 7) & 0x1f));

          //Logging::printf("addr=%lx csize=%llx recv_order=%lx foffset=%lx", addr, csize, (addr >> 7) & 0x1f, foffset);

          memcpy(reinterpret_cast<void *>(addr & ~0xffful), rawfile + foffset, csize); 
          Logging::printf("send %p '%s'\n", reinterpret_cast<char*>(addr & ~0xffful), reinterpret_cast<char*>(addr & ~0xffful));
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

      Utcb *worker_utcb;
      unsigned cap_ec = create_ec4pt(this, utcb->head.nul_cpunr, 0, &worker_utcb, alloc_cap());
      if (!cap_ec) return false;

      worker_utcb->head.crd = alloc_crd();
      unsigned long portal_func = reinterpret_cast<unsigned long>(StaticPortalFunc<DummyFS>::portal_func);

      res = nova_create_pt(pt, cap_ec, portal_func, 0);
      if (res) return false;

      res = ParentProtocol::register_service(*utcb, service_name, utcb->head.nul_cpunr, pt, service_cap);
      Logging::printf("%s - registing service\n", (res == 0 ? "success" : "failure"));
      if (res) return !res;
      //service part - end

      //client part
      FsProtocol * fs = new FsProtocol(alloc_cap(FsProtocol::CAP_SERVER_PT + hip->cpu_desc_count()), "fs/dummy");

      FsProtocol::dirent fileinfo;
      FsProtocol::File file_obj(*fs, alloc_cap());
      res = fs->get(*utcb, file_obj, "myfile");
      if (res) return false;
      
      res = file_obj.get_info(*utcb, fileinfo);
      Logging::printf("fs size %llx err=%x\n", fileinfo.size, res);
      if (res) return false;

      char * text = new (0x1000) char [0x1000];
      res = file_obj.copy(*utcb, text, fileinfo.size);
      revoke_all_mem(text, 0x1000, DESC_MEM_ALL, false);
      if (res) return false;
      Logging::printf("received '%s'\n", text);
      fs->destroy(*utcb, FsProtocol::CAP_SERVER_PT + hip->cpu_desc_count(), this);
      return true;
    }

  void run(Utcb *utcb, Hip *hip)
    {

      init(hip);
      init_mem(hip);
      init_service();

      console_init("dummy fs", new Semaphore(alloc_cap(), true));

      Logging::printf("Hello\n");

      _virt_phys.debug_dump("");

      if (!use_filesystem(utcb, hip))
        Logging::printf("failed - starting fs failed \n");

    }
};

} /* namespace */

ASMFUNCS(ab::DummyFS, NovaProgram)
