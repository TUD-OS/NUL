/*
 * Main sigma0 code.
 *
 * Copyright (C) 2008-2010, Bernhard Kauer <bk@vmmon.org>
 * Copyright (C) 2011, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Copyright (C) 2011, Alexander Boettcher <boettcher@tudos.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver.nova is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * Vancouver.nova is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include "nul/motherboard.h"
#include "host/keyboard.h"
#include "host/screen.h"
#include "host/dma.h"
#include "host/hostpci.h"
#include "nul/program.h"
#include "nul/generic_service.h"
#include "service/elf.h"
#include "service/logging.h"
#include "sigma0/sigma0.h"
#include "nul/service_fs.h"
#include "nul/service_admission.h"

// global VARs
unsigned     console_id;
Motherboard *global_mb;
class Sigma0;
Sigma0      *sigma0;
Semaphore   *consolesem;
unsigned     mac_prefix = 0x42000000;
unsigned     mac_host;

// extra params
PARAM(mac_prefix, mac_prefix = argv[0],  "mac_prefix:value=0x42000000 - override the MAC prefix.")
PARAM(mac_host,   mac_host = argv[0],    "mac_host:value - override the host part of the MAC.")
enum { VERBOSE_ERRORS = 0, VERBOSE_INFO = 1 } verbose = VERBOSE_ERRORS;
PARAM(verbose, verbose = VERBOSE_INFO, "verbose - print additional information during runtime")

PARAM_ALIAS(S0_DEFAULT,   "an alias for the default sigma0 parameters",
            " ioio hostacpi pcicfg mmconfig atare"
            " hostreboot:0 hostreboot:1 hostreboot:2 hostreboot:3 service_per_cpu_timer service_romfs script")

#define S0_DEFAULT_CMDLINE "namespace::/s0 name::/s0/timer name::/s0/fs/rom name::/s0/admission quota::guid"

  // module data
  struct ModuleInfo
  {
    unsigned        id;
    enum { TYPE_S0, TYPE_ADMISSION } type;
    unsigned        cpunr;
    unsigned long   rip;
    bool            dma;
    unsigned long   physsize;
    unsigned        mac;
    void *          hip;
    char const *    cmdline;
    unsigned long   sigma0_cmdlen;
    char *          mem; //have to be last element - see free_module
  };

  enum {
    MAXCPUS            = Config::MAX_CPUS,
    MAXPCIDIRECT       = 64,
    MAXMODULES         = 1 << Config::MAX_CLIENTS_ORDER,
    CPUGSI             = 0,
    MEM_OFFSET         = 1ul << 31,
    CLIENT_HIP         = 0xBFFFF000U,
    CLIENT_BOOT_UTCB   = CLIENT_HIP - 0x1000,
    CLIENT_PT_OFFSET   = 0x20000U,
    CLIENT_PT_SHIFT    = Config::CAP_RESERVED_ORDER,
    CLIENT_PT_ORDER    = CLIENT_PT_SHIFT + Config::MAX_CLIENTS_ORDER,
  };

#include "parent_protocol.h"

/**
 * Sigma0 application class.
 */
struct Sigma0 : public Sigma0Base, public NovaProgram, public StaticReceiver<Sigma0>
{
  // a mapping from virtual cpus to the physical numbers
  unsigned  _cpunr[MAXCPUS];
  unsigned  _numcpus;
  unsigned  _last_affinity;

  // data per physical CPU number
  struct {
    unsigned  cap_ec_worker;
    unsigned  cap_ec_echo;
    unsigned  cap_pt_echo;
    unsigned  exc_base;
  } _percpu[MAXCPUS];

  // synchronisation of GSIs+worker
  Semaphore _lock_gsi;
  // lock for memory allocator
  static Semaphore _lock_mem;

  // putc+vga
  char    * _vga;
  VgaRegs   _vga_regs;
  struct PutcData
  {
    VgaRegs        *regs;
    unsigned short *screen;
    Semaphore      sem;
  } putcd;

  // device data
  Motherboard *_mb;

  // module data
  ModuleInfo _modinfo[MAXMODULES];
  unsigned _mac;

  // Hip containing virtual address for aux, use _hip if physical addresses are needed
  Hip * __hip;

  // IRQ handling
  unsigned  _msivector;        // next gsi vector
  unsigned long long _gsi;     // bitfield per used GSI
  unsigned _pcidirect[MAXPCIDIRECT];

  // parent protocol
  friend class s0_ParentProtocol;

  // services
  bool admission; 
  AdmissionProtocol * service_admission;
  s0_ParentProtocol * service_parent;

  char *map_self(Utcb *utcb, unsigned long physmem, unsigned long size, unsigned rights = DESC_MEM_ALL)
  {
    assert(size);

    // we align to order but not more than 4M
    unsigned order = Cpu::bsr(size | 1);
    if (order < 12 || order > 22) order = 22;

    unsigned alignment = (1 << order) - 1;
    unsigned long ofs = physmem & alignment;
    physmem -= ofs;
    size    += ofs;
    if (size & alignment) size += alignment + 1 - (size & alignment);

    unsigned long virt = 0;
    if ((rights & 3) == DESC_TYPE_MEM)
    {
      SemaphoreGuard l(_lock_mem);

      unsigned long s = _virt_phys.find_phys(physmem, size);
      if (s)  return reinterpret_cast<char *>(s) + ofs;
      virt = _free_virt.alloc(size, Cpu::minshift(physmem, size, 22));
      if (!virt) return 0;
    }
    unsigned old = utcb->head.crd;
    utcb->head.crd = Crd(0, 20, rights).value();
    char *res = reinterpret_cast<char *>(virt);
    unsigned long offset = 0;
    while (size > offset)
    {
      utcb->set_header(0, 0);
      unsigned long s = add_mappings(utcb, physmem + offset, size - offset, (virt + offset) | MAP_DPT | MAP_HBIT, rights);
      if (verbose & VERBOSE_INFO)
        Logging::printf("s0: map self %lx -> %lx size %lx offset %lx s %lx typed %x\n",
                        physmem, virt, size, offset, s, utcb->head.typed);
      offset = size - s;
      unsigned err;
      memmove(utcb->msg, utcb->item_start(), sizeof(unsigned) * utcb->head.typed * 2);
      utcb->set_header(2*utcb->head.typed, 0);
      if ((err = nova_call(_percpu[utcb->head.nul_cpunr].cap_pt_echo)))
      {
        Logging::printf("s0: map_self failed with %x mtr %x/%x\n", err, utcb->head.untyped, utcb->head.typed);
        res = 0;
        break;
      }
    }
    //XXX if a mapping fails undo already done mappings and add free space back to free_virt !
    utcb->head.crd = old;
    if ((rights & 3) == DESC_TYPE_MEM) {
      SemaphoreGuard l(_lock_mem);
      _virt_phys.add(Region(virt, size, physmem));
    }
    return res ? res + ofs : 0;
  }

  /**
   * Command lines need to be mapped.
   */
  char *map_string(Utcb *utcb, unsigned long src)
  {
    unsigned size = 0;
    unsigned offset = src & 0xfffull;
    src &= ~0xfffull;
    char *res = 0;
    do {
      //if (res) unmap_self(res, size);
      size += 0x1000;
      res = map_self(utcb, src, size) + offset;
    } while (strnlen(res, size - offset) == (size - offset));
    return res;
  }

  bool attach_irq(unsigned gsi, unsigned cap_sm, bool unlocked, unsigned cpunr)
  {
    Utcb *u = 0;
    unsigned cap_ec = create_ec_helper(this, cpunr, _percpu[cpunr].exc_base,  &u,
                                       reinterpret_cast<void *>(&do_gsi_wrapper));
    u->msg[0] =  cap_sm;
    u->msg[1] =  gsi | (unlocked ? 0x200 : 0x0);
    AdmissionProtocol::sched sched(AdmissionProtocol::sched::TYPE_SYSTEM); //XXX special handling here to get it to prio 4, get rid of that
    return (service_admission->alloc_sc(*myutcb(), cap_ec, sched, myutcb()->head.nul_cpunr, this) == NOVA_ESUCCESS);
  }

  unsigned attach_msi(MessageHostOp *msg, unsigned cpunr) {
    //XXX if attach failed msivector should be reused!
    msg->msi_gsi = _hip->cfg_gsi - ++_msivector;
    unsigned irq_cap = _hip->cfg_exc + 3 + msg->msi_gsi;
    unsigned res = nova_assign_gsi(irq_cap, cpunr, msg->value, &msg->msi_address, &msg->msi_value);
    if (res != NOVA_ESUCCESS)
      Logging::printf("s0: failed to setup msi - err=%x, msi_gsi=%x irq_cap=%x cpu=%x\n", res, msg->msi_gsi, irq_cap, cpunr);
    return res == NOVA_ESUCCESS ? irq_cap : 0;
  }


  /**
   * Converts client ptr to a pointer in our address space.
   * Returns true on error.
   */
  template<typename T>
  static bool convert_client_ptr(ModuleInfo *modinfo, T *&ptr, unsigned size)
  {
    unsigned long offset = reinterpret_cast<unsigned long>(ptr);

    if (offset < MEM_OFFSET || modinfo->physsize + MEM_OFFSET <= offset || size > modinfo->physsize + MEM_OFFSET - offset)
      return true;
    ptr = reinterpret_cast<T *>(offset + modinfo->mem - MEM_OFFSET);
    return false;
  }

  /**
   * Returns true on error. (Why?!)
   */
  static bool adapt_ptr_map(ModuleInfo *modinfo, unsigned long &physoffset, unsigned long &physsize)
  {
    if (physoffset - MEM_OFFSET > modinfo->physsize) return true;
    if (physsize > (modinfo->physsize - physoffset + MEM_OFFSET))
      physsize = modinfo->physsize - physoffset + MEM_OFFSET;
    physoffset += reinterpret_cast<unsigned long>(modinfo->mem) - MEM_OFFSET;
    return false;
  }

  /**
   * Handle an attach request.
   */
  template <class CONSUMER, class PRODUCER>
  void handle_attach(ModuleInfo *modinfo, PRODUCER &res, Utcb *utcb)
  {
    CONSUMER *con = reinterpret_cast<CONSUMER *>(utcb->msg[1]);
    if (convert_client_ptr(modinfo, con, sizeof(CONSUMER)))
      Logging::printf("s0: [%2u] consumer %p out of memory %lx\n", modinfo->id, con, modinfo->physsize);
    else
      {
        res = PRODUCER(con, utcb->head.crd >> Utcb::MINSHIFT);
        utcb->msg[0] = 0;
      }

    utcb->head.crd = alloc_crd();
    utcb->set_header(1, 0);
  }

  void postinit(Hip * hip) {
    // parse cmdline of sigma0
    char * cmdline = reinterpret_cast<char *>(hip->get_mod(0)->aux);
    _modinfo[0].sigma0_cmdlen = strlen(cmdline);
    _modinfo[0].type = ModuleInfo::TYPE_S0;

    char * s0_pos;
    if (s0_pos = strstr(cmdline, "S0_DEFAULT")) {
      unsigned long s0_size = strlen(S0_DEFAULT_CMDLINE);
      unsigned long cmd_size = s0_size + _modinfo[0].sigma0_cmdlen - 10;
      char * expanded = new char[cmd_size + 1];
      memcpy(expanded, cmdline, s0_pos - cmdline);
      memcpy(expanded + (s0_pos - cmdline), S0_DEFAULT_CMDLINE, s0_size);
      memcpy(expanded + (s0_pos - cmdline) + s0_size, s0_pos + 10, _modinfo[0].sigma0_cmdlen - 10 - (s0_pos - cmdline));
      *(expanded + cmd_size) = 0;

      //XXX free old cmdline - change aux stuff in phys/virt
      _modinfo[0].cmdline = expanded;
      _modinfo[0].sigma0_cmdlen = cmd_size;
    } else
      _modinfo[0].cmdline = cmdline;

    // check services to support
    admission = strstr(_modinfo[0].cmdline, "sigma0::admission");

    // init services required or provided by sigma0
    service_parent    = new (sizeof(void *)*2) s0_ParentProtocol(CLIENT_PT_OFFSET + (1 << CLIENT_PT_ORDER), CLIENT_PT_ORDER, CLIENT_PT_OFFSET, CLIENT_PT_ORDER + 1);
    service_admission = new AdmissionProtocol(alloc_cap(AdmissionProtocol::CAP_SERVER_PT + hip->cpu_count()), true);
  }

  /**
   * Create the needed host devices aka instantiate the drivers.
   */
  unsigned create_host_devices(Utcb *utcb, Hip *hip)
  {
    _mb = new Motherboard(new Clock(hip->freq_tsc*1000), hip);
    global_mb = _mb;
    _mb->bus_hostop.add(this,  receive_static<MessageHostOp>);

    init_disks();
    init_network();
    init_vnet();
    char * cmdline = reinterpret_cast<char *>(hip->get_mod(0)->aux);
    _mb->parse_args(cmdline);
    init_console();
    MessageLegacy msg3(MessageLegacy::RESET, 0);
    _mb->bus_legacy.send_fifo(msg3);
    return 0;
  }

  static void serial_send(long value) {
    if (global_mb)
      {
        MessageSerial msg(1, value);
        global_mb->bus_serial.send(msg);
      }
  }


  static void putc(void *data, int value)
  {
    PutcData *p = reinterpret_cast<PutcData *>(data);
    if (p && value == -1) p->sem.down();
    if (p && value == -2) p->sem.up();
    if (value < 0) return;

    if (p) Screen::vga_putc(0xf00 | value, p->screen, p->regs->cursor_pos);
    if (value == '\n')  serial_send('\r');
    serial_send(value);
  }

  static void fancy_output(const char *st, unsigned maxchars)
  {
    unsigned lastchar = 0;
    for (unsigned x=0; x < maxchars && st[x]; x++)
      {
        unsigned value = st[x];
        if (value == '\n' || value == '\t' || value == '\r' || value == 0) value = ' ';
        if (value == ' ' && lastchar == value) continue;
        if (value == ' ' && lastchar == '|') Logging::printf("' \\\ns0: ... '");
        Logging::printf("%c", value);
        lastchar = value;
      }
    Logging::printf("'\n");
  }

  /**
   * Prepare UTCB for receiving a new cap.
   */
  //XXX opening a CRD for everybody is a security risk, as another client could have alread mapped something at this cap!
  inline unsigned alloc_crd() { return Crd(alloc_cap(), 0, DESC_CAP_ALL).value(); }

  unsigned create_worker_threads(Hip *hip, int cpunr) {
    for (int i = 0; i < (hip->mem_offs - hip->cpu_offs) / hip->cpu_size; i++) {

      Hip_cpu *cpu = reinterpret_cast<Hip_cpu *>(reinterpret_cast<char *>(hip) + hip->cpu_offs + i*hip->cpu_size);
      if (~cpu->flags & 1 || (cpunr != -1 && i != cpunr)) continue;
      Logging::printf("s0: cpu[%x]: %x:%x:%x\n", i, cpu->package, cpu->core, cpu->thread);
      // have we created it already?
      if (_percpu[i].cap_ec_echo)  continue;
      _cpunr[_numcpus++] = i;

      // Create the ec_echo with an invalid exception base.
      _percpu[i].cap_ec_echo = create_ec_helper(this, i, 0);
      _percpu[i].cap_pt_echo = alloc_cap();
      check1(1, nova_create_pt(_percpu[i].cap_pt_echo, _percpu[i].cap_ec_echo, reinterpret_cast<unsigned long>(do_map_wrapper), 0));

      _percpu[i].exc_base = alloc_cap(0x20);

      Utcb *utcb = 0;
      _percpu[i].cap_ec_worker = create_ec_helper(this, i, _percpu[i].exc_base, &utcb);
      utcb->head.crd = alloc_crd();

      // Create error handling portals.
      // XXX All threads that use ec_echo have broken error handling
      for (unsigned pt = 0; pt <= 0x1D; pt++)
        check1(4, nova_create_pt(_percpu[i].exc_base + pt, _percpu[i].cap_ec_echo,
                                 reinterpret_cast<unsigned long>(do_error_wrapper),
                                 MTD_ALL));

      // Create GSI boot wrapper.
      check1(5, nova_create_pt(_percpu[i].exc_base + 0x1E, _percpu[i].cap_ec_echo,
                               reinterpret_cast<unsigned long>(do_thread_startup_wrapper),
                               MTD_RSP | MTD_RIP_LEN));
    }
    return 0;
  }

  /**
   * Init the pager, console and map initial resources.
   */
  unsigned __attribute__((noinline)) preinit(Utcb *utcb, Hip *hip)
  {
    Logging::init(putc, 0);
    Logging::printf("s0: preinit %p\n\n", hip);
    check1(1, init(hip));

    //cap range from 0 - 0x10000 assumed to be reserved by program.h -> assertion
    //reserve cap range from 0x20000 (CLIENT_PT_OFFSET) to MAX_CLIENTS (CLIENT_ALL_SHIFT) - is used by sigma0 implicitly without a cap allocator !
    assert(_cap_start == 0 && _cap_order == 16 && CLIENT_PT_OFFSET == 0x20000U);
    Region * reserve = _cap_region.find(CLIENT_PT_OFFSET);
    assert(reserve && reserve->virt <= CLIENT_PT_OFFSET && reserve->size - (reserve->virt - CLIENT_PT_OFFSET) >= (2 << CLIENT_PT_ORDER));
    //clients range + parent_protocol range
    _cap_region.del(Region(CLIENT_PT_OFFSET, 2U << CLIENT_PT_ORDER));

    //sanity checks
    assert(!_cap_region.find(CLIENT_PT_OFFSET) && !_cap_region.find(CLIENT_PT_OFFSET + (1U << CLIENT_PT_ORDER) - 1U));
    assert(!_cap_region.find(CLIENT_PT_OFFSET + (2U << CLIENT_PT_ORDER) - 1U));
    assert(!_cap_region.find(0));   //should be reserved by program.h
    assert(!_cap_region.find(ParentProtocol::CAP_PT_PERCPU - 1U));
    assert(!_cap_region.find(ParentProtocol::CAP_PT_PERCPU + Config::MAX_CPUS - 1U));

    Logging::printf("s0: create locks\n");
    _lock_gsi    = Semaphore(alloc_cap());
    _lock_mem    = Semaphore(alloc_cap());
    check1(2, nova_create_sm(_lock_gsi.sm()) || nova_create_sm(_lock_mem.sm()));
    _lock_mem.up();

    Logging::printf("s0: create pf echo+worker threads\n");
    check1(3, create_worker_threads(hip, utcb->head.nul_cpunr));

    // map vga memory
    Logging::printf("s0: map vga memory\n");
    _vga = map_self(utcb, 0xa0000, 1<<17);

    // keep the boot screen
    memcpy(_vga + 0x1a000, _vga + 0x18000, 0x1000);

    putcd.screen = reinterpret_cast<unsigned short *>(_vga + 0x18000);
    putcd.regs = &_vga_regs;
    putcd.sem = Semaphore(alloc_cap());
    consolesem = &putcd.sem;
    putcd.sem.up();
    check1(6, nova_create_sm(putcd.sem.sm()));

    _vga_regs.cursor_pos = 24*80*2;
    _vga_regs.offset = 0;
    Logging::init(putc, &putcd);


    // create the parent semaphore
    check1(7, nova_create_sm(CLIENT_PT_OFFSET + ParentProtocol::CAP_PARENT_ID));


    // map it to the parent id of module 0
    Utcb::TypedMapCap(CLIENT_PT_OFFSET + ParentProtocol::CAP_PARENT_ID).fill_words(utcb->msg, Crd(ParentProtocol::CAP_PARENT_ID, 0, MAP_MAP).value());
    *utcb << Crd(0, 31, DESC_CAP_ALL);
    utcb->set_header(2, 0);

    // map all IRQs
    for (unsigned gsi=0; gsi < hip->cfg_gsi; gsi++) {
      Utcb::TypedMapCap(gsi + hip->cpu_desc_count()).fill_words(utcb->msg + utcb->head.untyped, Crd(hip->cfg_exc + 3 + gsi, 0, MAP_HBIT).value());
      utcb->head.untyped += 2;
    }

    check1(8, nova_call(_percpu[utcb->head.nul_cpunr].cap_pt_echo));
    return 0;
  };


  /**
   * Request memory from the memmap. Minimum alignment is 16-bytes
   * (for SSE stuff).
   */
  static void *sigma0_memalloc(unsigned long size, unsigned long align) {
    if (!size) return 0;
    if (align < 0xF) align = 0xF;

    size = (size + 0xF) & ~0xF;

    unsigned long pmem;
    {
      SemaphoreGuard l(_lock_mem);
      pmem = sigma0->_free_phys.alloc(size, Cpu::bsr(align | 1));
    }
    void *res;
    if (!pmem || !(res = sigma0->map_self(myutcb(), pmem, size))) Logging::panic("s0: %s(%lx, %lx) EOM!\n", __func__, size, align);
    memset(res, 0, size);
    return res;
  }

  /**
   * Init the memory map from the Hip.
   */
  unsigned init_memmap(Utcb *utcb)
  {
    Logging::printf("s0: init memory map\n");

    // our image
    extern char __image_start, __image_end;
    _virt_phys.add(Region(reinterpret_cast<unsigned long>(&__image_start), &__image_end - &__image_start, _hip->get_mod(0)->addr));

    // Do a first pass to add all available memory below 4G.
    for (int i = 0; i < (_hip->length - _hip->mem_offs) / _hip->mem_size; i++) {
      Hip_mem *hmem = reinterpret_cast<Hip_mem *>(reinterpret_cast<char *>(_hip) + _hip->mem_offs) + i;
      Logging::printf("s0:  mmap[%02d] addr %16llx len %16llx type %2d aux %8x\n", i, hmem->addr, hmem->size, hmem->type, hmem->aux);

      // Skip regions above 4GB.
      if (hmem->addr >= (1ULL<<32)) continue;

      if (hmem->addr + hmem->size > (1ULL<<32))
        Logging::panic("s0: Bogus memory map. Region crosses 4G boundary.");

      if (hmem->type == 1) _free_phys.add(Region(hmem->addr, hmem->size, hmem->addr));
    }

    // Remove all reserved regions.
    for (int i = 0; i < (_hip->length - _hip->mem_offs) / _hip->mem_size; i++) {
      Hip_mem *hmem = reinterpret_cast<Hip_mem *>(reinterpret_cast<char *>(_hip) + _hip->mem_offs) + i;

      if (hmem->type != 1) _free_phys.del(Region(hmem->addr, (hmem->size + 0xfff) & ~0xffful));

      if (hmem->type == -2) {
        // map all the modules early to be sure that we have enough virtual memory left
        // skip modules with length zero
        if (hmem->size != 0) {
          map_self(utcb, hmem->addr, (hmem->size + 0xfff) & ~0xffful);
        }

        // Make sure to remove the commandline as well.
        if (hmem->aux)
          _free_phys.del(Region(hmem->aux, (strlen(map_string(utcb, hmem->aux)) + 0xfff) & ~0xffful));
      }
    }

    // Reserve the very first 1MB and remove everything above 4G.
    _free_phys.del(Region(0,        1<<20, 0));
    _free_phys.del(Region(1ULL<<32, -(1ULL << 32) - 1, 0)); // avoid end() == 0

    // switch to another allocator
    memalloc = sigma0_memalloc;

    // create a hip containing virtual addresses for aux
    if (_hip->length > 0x1000) return 1;
    __hip = reinterpret_cast<Hip *>(new (0x1000) char[0x1000]);
    if (!__hip) return 1;
    memcpy(__hip, _hip, _hip->length);

    for (int i = 0; i < (__hip->length - __hip->mem_offs) / __hip->mem_size; i++) {
      Hip_mem *hmem = reinterpret_cast<Hip_mem *>(reinterpret_cast<char *>(__hip) + __hip->mem_offs) + i;
      if (hmem->type == -2) {
        if (hmem->size == 0) {
          // It is an error to access this module, because it is zero bytes long.
          hmem->addr = 0;
        } else 
          hmem->addr = reinterpret_cast<unsigned long>(map_self(utcb, hmem->addr, (hmem->size + 0xfff) & ~0xffful));
      } 
      if (hmem->aux)
        hmem->aux = reinterpret_cast<unsigned>(map_string(utcb, hmem->aux));
    }
    __hip->fix_checksum();

    return 0;
  }

  void free_module(ModuleInfo * modinfo) {
    modinfo->mem = 0;
  }

  ModuleInfo * get_module(unsigned id) {
    if (id < 1 || id >= MAXMODULES) return 0;
    ModuleInfo *modinfo = _modinfo + id;
    return modinfo;
  }

  ModuleInfo * alloc_module(char const * cmdline, unsigned sigma0_cmdlen) {
    // search for a free module
    again:
    unsigned module = 1;
    while (module < MAXMODULES && *reinterpret_cast<unsigned long volatile *>(&_modinfo[module].mem)) module++;
    if (module >= MAXMODULES) return 0;
    if (0 != Cpu::cmpxchg4b(&_modinfo[module].mem, 0, 1)) goto again;

    // init module parameters
    ModuleInfo *modinfo = _modinfo + module;
    assert(reinterpret_cast<unsigned long>(&modinfo->mem) + sizeof(modinfo->mem) == reinterpret_cast<unsigned long>(modinfo) + sizeof(*modinfo));
    memset(modinfo, 0, sizeof(*modinfo) - sizeof(modinfo->mem));

    modinfo->id             = module;
    char *p = strstr(cmdline, "sigma0::cpu");
    if (p > cmdline + sigma0_cmdlen) p = 0;
    modinfo->cpunr          = _cpunr[( p ? strtoul(p+12, 0, 0) : ++_last_affinity) % _numcpus];
    p = strstr(cmdline, "sigma0::dma");
    modinfo->dma            = p > cmdline + sigma0_cmdlen ? 0 : p;
    modinfo->cmdline        = cmdline;
    modinfo->sigma0_cmdlen = sigma0_cmdlen;

    if (verbose & VERBOSE_INFO) {
      Logging::printf("s0: [%2u] module '", modinfo->id);
      fancy_output(cmdline, 4096);
    }

    return modinfo;
  }

  /**
   * Starts a configuration loaded during boottime. Configuration numbers start at zero.
   */
  unsigned start_config(Utcb *utcb, unsigned which, unsigned &internal_id)
  {
    // Skip to interesting configuration
    char const *cmdline = 0;
    Hip_mem *mod;
    unsigned configs = 0;
    unsigned i, len;

    for (i=1; mod = __hip->get_mod(i); i++) {
      cmdline = reinterpret_cast<char const *>(mod->aux);
      len = strcspn(cmdline, " \t\r\n\f");
      if (!(len >= 10 && !strncmp(cmdline + len - 10, ".nulconfig", 10))) continue;
      if (configs++ == which) break;
    }
    if (!mod) {
      Logging::printf ("Cannot find configuration %u.\n", which);
      return __LINE__;
    }
    if (mod->size == 1) {
      if (!(len = strcspn(cmdline, " \t\r\n\f"))) return __LINE__;
      cmdline += len;
    } else {
      char endc;
      cmdline = reinterpret_cast<char const *>(mod->addr);
      if ((mod->size > 0) && ((endc = *(cmdline + mod->size - 1)) != 0)) {
        if (endc > 32) {
          Logging::printf("s0: config %u has invalid end character (got %x : expected 0 - 32)\n", which, endc);
          return __LINE__;
        }
        *(reinterpret_cast<char *>(mod->addr) + mod->size - 1) = 0;
      }
    }
    return start_config(utcb, cmdline, internal_id);
  }

  /**
   * Start a configuration from a stable memory region (mconfig). Region has to be zero terminated.
   */
  unsigned start_config(Utcb *utcb, char const * mconfig, unsigned &internal_id)
  {
    char const * file_name, * client_cmdline;

    if (!mconfig || (!(client_cmdline = strstr(mconfig, "||")))) return __LINE__;
    unsigned long sigma0_cmdlen = client_cmdline - mconfig;
    client_cmdline += 2 + strspn(client_cmdline + 2, " \t\r\n\f");
    if (!(file_name = strstr(client_cmdline, "://")) || client_cmdline + strcspn(client_cmdline, " \t\r\n\f") < file_name) return __LINE__;

    if (file_name - client_cmdline > 64 - 4) return __LINE__;
    char fs_name [64] = "fs/";
    memcpy(&fs_name[3], client_cmdline, file_name - client_cmdline);
    fs_name[file_name - client_cmdline + 3] = 0;

    file_name += 3;
    unsigned namelen = strcspn(file_name, " \t\r\n\f");
    unsigned res;
    unsigned long long msize, physaddr;
    char *addr;

    FsProtocol fs_obj = FsProtocol(alloc_cap(FsProtocol::CAP_SERVER_PT + _hip->cpu_count()), fs_name);
    FsProtocol::dirent fileinfo;
    if (fs_obj.get_file_info(*utcb, fileinfo, file_name, namelen)) { Logging::printf("s0: File not found '%s'\n", file_name); res = __LINE__; goto fs_out; }

    msize = (fileinfo.size + 0xfff) & ~0xffful;
    {
      SemaphoreGuard l(_lock_mem);
      physaddr = _free_phys.alloc(msize, 12);
    }
    if (!physaddr || !msize) { Logging::printf("s0: Not enough memory\n"); res = __LINE__; goto fs_out; }

    addr = map_self(utcb, physaddr, msize);
    if (!addr) { Logging::printf("s0: Could not map file\n"); res= __LINE__; goto phys_out; }
    if (fs_obj.get_file_copy(*utcb, addr, fileinfo.size, file_name, namelen)) { Logging::printf("s0: Getting file failed %s.\n", file_name); res = __LINE__; goto map_out; }

    res = _start_config(utcb, addr, fileinfo.size, mconfig, client_cmdline, sigma0_cmdlen, internal_id);

  map_out:
    //don't try to unmap from ourself "revoke(..., true)"
    //map_self may return a already mapped page (backed by 4M) which contains the requested phys. page
    //revoking a small junk of a larger one unmaps the whole area ...
    revoke_all_mem(reinterpret_cast<void *>(addr), msize, DESC_MEM_ALL, false);
  phys_out:
    {
      SemaphoreGuard l(_lock_mem);
      _free_phys.add(Region(physaddr, msize));
    }
  fs_out:
    fs_obj.destroy(*utcb, _hip->cpu_count(), this);

    return res;
  }

  unsigned _start_config(Utcb * utcb, char * elf, unsigned long mod_size, char const * mconfig,
                         char const * client_cmdline, unsigned sigma0_cmdlen, unsigned &internal_id)
  {
    AdmissionProtocol::sched sched; //Qpd(1, 100000)
    unsigned res = 0, slen, pt = 0;
    unsigned long pmem = 0, maxptr = 0;
    Hip * modhip = 0;
    char * tmem;

    ModuleInfo *modinfo = alloc_module(mconfig, sigma0_cmdlen);
    if (!modinfo) { Logging::printf("s0: to many modules to start -- increase MAXMODULES in %s\n", __FILE__); return __LINE__; }
    if (admission && modinfo->id == 1) modinfo->type = ModuleInfo::TYPE_ADMISSION;

    // Figure out how much memory we give the client. If the user
    // didn't give a limit, we give it enough to unpack the ELF.
    unsigned long psize_needed = elf ? Elf::loaded_memsize(elf, mod_size) : ~0u;
    char *p = strstr(modinfo->cmdline, "sigma0::mem:");
    if (p > modinfo->cmdline + modinfo->sigma0_cmdlen) p = NULL;
    modinfo->physsize = (p == NULL) ? psize_needed : (strtoul(p+sizeof("sigma0::mem:")-1, 0, 0) << 20);

    // Round up to page size
    modinfo->physsize = (modinfo->physsize + 0xFFF) & ~0xFFF;

    check2(_free_module, (modinfo->physsize > (CLIENT_BOOT_UTCB - MEM_OFFSET)),
           "s0: [%2u] Cannot allocate more than %u KB for client, requested %lu KB.\n", modinfo->id,
           CLIENT_BOOT_UTCB - MEM_OFFSET, modinfo->physsize / 1024);

    check2(_free_module, ((psize_needed > modinfo->physsize) || !elf),
           "s0: [%2u] Could not allocate %ld MB memory. We need %ld MB.\n", modinfo->id, modinfo->physsize >> 20, psize_needed >> 20);

    {
      SemaphoreGuard l(_lock_mem);
      if (!(pmem = _free_phys.alloc(modinfo->physsize, 22))) {
        _free_phys.debug_dump("free phys");
        _virt_phys.debug_dump("virt phys");
        _free_virt.debug_dump("free virt");
      }
      check2(_free_module, !pmem);
    }

    check2(_free_pmem, (!((tmem = map_self(utcb, pmem, modinfo->physsize))))); //don't assign modinfo->mem = map_self directly (double free may happen)
    modinfo->mem = tmem;

    if (verbose & VERBOSE_INFO)
      Logging::printf("s0: [%2u] using memory: %ld MB (%lx) at %lx\n", modinfo->id, modinfo->physsize >> 20, modinfo->physsize, pmem);

    /**
     * We memset the client memory to make sure we get an
     * deterministic run and not leak any information between
     * clients.
     */
    memset(modinfo->mem, 0, modinfo->physsize);

    // decode ELF
    check2(_free_pmem, (Elf::decode_elf(elf, mod_size, modinfo->mem, modinfo->rip, maxptr, modinfo->physsize, MEM_OFFSET, Config::NUL_VERSION)));

    modinfo->hip = new (0x1000) char[0x1000];

    // allocate a console for it
    alloc_console(modinfo, modinfo->cmdline);
    attach_drives(modinfo->cmdline, modinfo->sigma0_cmdlen, modinfo->id);

    // create a HIP for the client
    slen = strlen(client_cmdline) + 1;
    check2(_free_caps, (slen + _hip->length + 2*sizeof(Hip_mem) > 0x1000), "s0: configuration to large\n");

    modhip = reinterpret_cast<Hip *>(modinfo->hip);
    memcpy(reinterpret_cast<char *>(modhip) + 0x1000 - slen, client_cmdline, slen);

    memcpy(modhip, _hip, _hip->mem_offs);
    modhip->length = modhip->mem_offs;
    modhip->append_mem(MEM_OFFSET, modinfo->physsize, 1, pmem);
    modhip->append_mem(0, slen, -2, CLIENT_HIP + 0x1000 - slen);
    modhip->fix_checksum();
    assert(_hip->length > modhip->length);

    // fix the BSP flag in the client HIP
    for (unsigned i=0; i < static_cast<unsigned>(modhip->mem_offs - modhip->cpu_offs) / modhip->cpu_size; i++) {
      Hip_cpu *cpu = reinterpret_cast<Hip_cpu *>(reinterpret_cast<char *>(modhip) + _hip->cpu_offs + i*modhip->cpu_size);
      if (modinfo->cpunr == i)  cpu->flags |= 2; else  cpu->flags &= ~2;
    }

    // create special portal for every module
    pt = CLIENT_PT_OFFSET + (modinfo->id << CLIENT_PT_SHIFT);
    assert(_percpu[modinfo->cpunr].cap_ec_worker);
    check2(_free_caps, nova_create_pt(pt + 14, _percpu[modinfo->cpunr].cap_ec_worker, reinterpret_cast<unsigned long>(do_request_wrapper), MTD_RIP_LEN | MTD_QUAL));
    check2(_free_caps, nova_create_pt(pt + 30, _percpu[modinfo->cpunr].cap_ec_worker, reinterpret_cast<unsigned long>(do_startup_wrapper), 0));

    // create parent portals
    check2(_free_caps, service_parent->create_pt_per_client(pt, this));
    check2(_free_caps, nova_create_sm(pt + ParentProtocol::CAP_PARENT_ID));

    // map idle SCs, so that they can be mapped to the admission service during create_pd
    if (admission && modinfo->type == ModuleInfo::TYPE_ADMISSION) {
      utcb->add_frame();
      *utcb << Crd(0, 31, DESC_CAP_ALL);
      for (unsigned sci=0; sci < _hip->cpu_count(); sci++) {
        Utcb::TypedMapCap(0 + sci).fill_words(utcb->msg + utcb->head.untyped, Crd(pt + ParentProtocol::CAP_PT_PERCPU + MAXCPUS + sci, 0, MAP_HBIT).value());
        utcb->head.untyped += 2;
      }

      check2(_free_caps, nova_call(_percpu[utcb->head.nul_cpunr].cap_pt_echo));

      for (unsigned sci=0; sci < _hip->cpu_count(); sci++) {
        timevalue computetime;
        unsigned char res = nova_ctl_sc(pt + ParentProtocol::CAP_PT_PERCPU + MAXCPUS + sci, computetime);
        assert(res == NOVA_ESUCCESS);
      }

      utcb->drop_frame();
    }

    if (verbose & VERBOSE_INFO)
      Logging::printf("s0: [%2u] creating PD%s on CPU %d\n", modinfo->id, modinfo->dma ? " with DMA" : "", modinfo->cpunr);

    check2(_free_caps, nova_create_pd(pt + NOVA_DEFAULT_PD_CAP, Crd(pt, CLIENT_PT_SHIFT,
           DESC_CAP_ALL & ((!admission || (admission && modinfo->type == ModuleInfo::TYPE_ADMISSION)) ? ~0U : ~(DESC_RIGHT_SC | DESC_RIGHT_PD)))));
    check2(_free_caps, nova_create_ec(pt + ParentProtocol::CAP_CHILD_EC,
			     reinterpret_cast<void *>(CLIENT_BOOT_UTCB), 0U,
			     modinfo->cpunr, 0, false, pt + NOVA_DEFAULT_PD_CAP));

    if (!admission || (admission && modinfo->type == ModuleInfo::TYPE_ADMISSION)) {
      //XXX assign admission cap not to sigma0 !!
      check2(_free_caps, service_admission->alloc_sc(*utcb, pt + ParentProtocol::CAP_CHILD_EC, sched, modinfo->cpunr, this));
    } else {
      //map temporary child id
      utcb->add_frame();
      *utcb << Crd(0, 31, DESC_CAP_ALL);
      Utcb::TypedMapCap(pt + ParentProtocol::CAP_PARENT_ID).fill_words(utcb->msg, Crd(pt + ParentProtocol::CAP_CHILD_ID, 0, MAP_MAP).value());
      utcb->head.untyped += 2;
      check2(_free_caps, nova_call(_percpu[utcb->head.nul_cpunr].cap_pt_echo));
      utcb->drop_frame();

      //allocate sc
      unsigned cap_base = alloc_cap(AdmissionProtocol::CAP_SERVER_PT + _hip->cpu_count());
      AdmissionProtocol * s_ad = new AdmissionProtocol(cap_base, false); //don't block sigma0 when admission service is not available
      res = s_ad->get_pseudonym(*utcb, pt + ParentProtocol::CAP_CHILD_ID);
      if (!res) res = s_ad->alloc_sc(*utcb, pt + ParentProtocol::CAP_CHILD_EC, sched, modinfo->cpunr);

      s_ad->close(*utcb, _hip->cpu_count(), true, false);
      delete s_ad;

      unsigned char res = nova_revoke(Crd(pt + ParentProtocol::CAP_CHILD_ID, 0, DESC_CAP_ALL), true); //revoke tmp child id
      assert(!res);
      dealloc_cap(cap_base, AdmissionProtocol::CAP_SERVER_PT + _hip->cpu_count());
    }

    _free_caps:
    if (res) {
      delete [] reinterpret_cast<char *>(modhip);

      // unmap all caps
      if (NOVA_ESUCCESS != nova_revoke(Crd(pt, CLIENT_PT_SHIFT, DESC_CAP_ALL), true))
        Logging::printf("s0: curiosity - nova_revoke failed\n");
    }

    _free_pmem:
    if (res) {
      SemaphoreGuard l(_lock_mem);
      _free_phys.add(Region(pmem, modinfo->physsize));
    }

    _free_module:
    if (res) free_module(modinfo);

    if (!res) internal_id = modinfo->id;

    return res;
  }

  /**
   * Kill the given module.
   */
  unsigned kill_module(ModuleInfo * modinfo) {
    if (!modinfo || !_modinfo[modinfo->id].mem || !_modinfo[modinfo->id].physsize) return __LINE__;

    if (verbose & VERBOSE_INFO) Logging::printf("s0: [%2u] - initiate destruction of client ... \n", modinfo->id);
    // send kill message to parent service so that client specific data structures within a service can be released
    unsigned err = 0;
    unsigned recv_cap = alloc_cap();
    unsigned map_cap = CLIENT_PT_OFFSET + (modinfo->id << CLIENT_PT_SHIFT) + ParentProtocol::CAP_PARENT_ID;

    if (recv_cap) {
      Utcb &utcb = myutcb()->add_frame();
      Utcb::TypedMapCap(map_cap).fill_words(utcb.msg);
      utcb << Crd(recv_cap, 0, DESC_CAP_ALL);
      utcb.set_header(2, 0);
      if ((err = nova_call(_percpu[myutcb()->head.nul_cpunr].cap_pt_echo))) Logging::printf("s0: [%2u]   couldn't establish mapping %u\n", modinfo->id, err);
      utcb.drop_frame();

      if (!err) {
        utcb = myutcb()->add_frame();
        ParentProtocol::kill(utcb, recv_cap);
        dealloc_cap(recv_cap);
        utcb.drop_frame();
      }
    } else err = !recv_cap;
    if (err) Logging::printf("s0: [%2u]   can not inform service about dying client\n", modinfo->id);

    // unmap all service portals
    if (verbose & VERBOSE_INFO) Logging::printf("s0: [%2u]   revoke all caps\n", modinfo->id);
    unsigned res = nova_revoke(Crd(CLIENT_PT_OFFSET + (modinfo->id << CLIENT_PT_SHIFT), CLIENT_PT_SHIFT, DESC_CAP_ALL), true);
    if (res != NOVA_ESUCCESS) Logging::printf("s0: curiosity - nova_revoke failed %x\n", res);

    // and the memory
    if (verbose & VERBOSE_INFO) Logging::printf("s0: [%2u]   revoke all memory\n", modinfo->id);
    revoke_all_mem(modinfo->mem, modinfo->physsize, DESC_MEM_ALL, false);

    // change the tag
    Vprintf::snprintf(_console_data[modinfo->id].tag, sizeof(_console_data[modinfo->id].tag), "DEAD - CPU(%x) MEM(%ld)", modinfo->cpunr, modinfo->physsize >> 20);
    // switch to view 0 so that you can see the changed tag
    switch_view(global_mb, 0, _console_data[modinfo->id].console);

    // free resources
    SemaphoreGuard l(_lock_mem);
    Region * r = _virt_phys.find(reinterpret_cast<unsigned long>(modinfo->mem));
    assert(r);
    assert(r->size >= modinfo->physsize);
    _free_phys.add(Region(r->phys, modinfo->physsize));
    modinfo->physsize = 0;

    // XXX legacy - should be a service - freeing producer/consumer stuff
    unsigned cap;
    if (cap = _prod_network[modinfo->id].sm()) {
      if (verbose & VERBOSE_INFO) Logging::printf("s0: [%2u]   detach network\n", modinfo->id);
      dealloc_cap(cap);
      memset(&_prod_network[modinfo->id], 0, sizeof(_prod_network[modinfo->id]));
    }
    if (cap = _disk_data[modinfo->id].prod_disk.sm()) {
      if (verbose & VERBOSE_INFO) Logging::printf("s0: [%2u]   detach disks\n", modinfo->id);
      dealloc_cap(cap);
      memset(&_disk_data[modinfo->id], 0, sizeof(_disk_data[modinfo->id]));
    }
    if (cap = _console_data[modinfo->id].prod_stdin.sm()) {
      if (verbose & VERBOSE_INFO) Logging::printf("s0: [%2u]   detach stdin\n", modinfo->id);
      dealloc_cap(cap);
      memset(&_console_data[modinfo->id], 0, sizeof(_console_data[modinfo->id]));
    }

    // XXX free more, such as GSIs, IRQs, Console...

    // XXX mark module as free -> we can not do this currently as we can not free all the resources
    if (verbose & VERBOSE_INFO) Logging::printf("s0: [%2u] - destruction done\n", modinfo->id);
    //free_module(modinfo);
    return 0;
  }


  /**
   * Assign a PCI device to a PD. It makes sure only the first will
   * get it.
   *
   * Returns 0 on success.
   */
  unsigned assign_pci_device(unsigned pd_cap, unsigned bdf, unsigned vfbdf)
  {
    for (unsigned i = 0; i < MAXPCIDIRECT; i++)
      {
	if (!_pcidirect[i])
	  {
	    unsigned res = nova_assign_pci(pd_cap, bdf, vfbdf);
	    if (!res) _pcidirect[i] = vfbdf ? vfbdf : bdf;
	    return res;
	  }
	if ((vfbdf == 0) && (bdf == _pcidirect[i]))
	  return 1;
	if (vfbdf != 0 && vfbdf == _pcidirect[i])
	  return 2;
      }
    return 3;
  }

  /*
   * Sigma0 portal functions.
   */
  #define PT_FUNC_NORETURN(NAME, CODE)					\
    static void NAME##_wrapper(unsigned pid, Sigma0 *tls, Utcb *utcb) __attribute__((regparm(1), noreturn)) \
    { tls->NAME(pid, utcb); }						\
  									\
    void __attribute__((always_inline, noreturn))  NAME(unsigned pid, Utcb *utcb) \
    { CODE;	}

  #define PT_FUNC(NAME, CODE, ...)					\
    static void NAME##_wrapper(unsigned pid, Sigma0 *tls, Utcb *utcb) __attribute__((regparm(1))) \
    { tls->NAME(pid, utcb); asmlinkage_protect("g"(tls), "g"(utcb)); }	\
  									\
    void __attribute__((always_inline))  NAME(unsigned pid, Utcb *utcb) __VA_ARGS__ \
    { CODE; }

  NORETURN
  void internal_error(unsigned pid, Utcb * utcb) {
  #if 0
    {
      SemaphoreGuard l(_lock_mem);
      _free_phys.debug_dump("free phys");
      _free_virt.debug_dump("free virt");
      _virt_phys.debug_dump("virt->phys");
    }
  #endif

    if (consolesem) consolesem->up();

    Logging::printf(">> Unhandled exception %u at EIP %08x!\n>>\n",
                    pid - _percpu[utcb->head.nul_cpunr].exc_base, utcb->eip);
    Logging::printf(">> MTD %08x PF  %016llx ERR %016llx\n",
                    utcb->head.mtr, utcb->qual[1], utcb->qual[0]);
    Logging::printf(">> EAX %08x EBX %08x ECX %08x EDX %08x\n",
                    utcb->eax, utcb->ebx, utcb->ecx, utcb->edx);
    Logging::printf(">> ESI %08x EDI %08x EBP %08x ESP %08x\n>>\n",
                    utcb->esi, utcb->edi, utcb->ebp, utcb->esp);

    Logging::printf(">> Optimistic stack dump (may fault again):\n");
    for (unsigned i = 0; (i < 8); i++) {
       unsigned *esp = reinterpret_cast<unsigned  *>(utcb->esp);
       Logging::printf(">> %p: %08x (decimal %u)\n", &esp[-i], esp[-i], esp[-i]);
    }
    Logging::printf(">> Stack dump done!\n");

    // All those moments will be lost in time like tears in the rain.
    // Time to die.
    unsigned res;
    res = nova_revoke(Crd(14, 0, DESC_CAP_ALL), true); // Unmap PF portal
    res = nova_revoke(Crd(0, 20, DESC_MEM_ALL), true); // Unmap all memory
    Logging::panic("Zombie? %x\n", res);
  }

  PT_FUNC_NORETURN(do_error, internal_error(pid, utcb);)

  PT_FUNC(do_map,
	  // make sure we have got an even number of words
	  assert(~utcb->head.untyped & 1);
	  utcb->set_header(0, utcb->head.untyped / 2);
	  memmove(utcb->item_start(), utcb->msg, sizeof(unsigned) * utcb->head.typed*2);
	  )

  PT_FUNC(do_thread_startup,   utcb->eip = reinterpret_cast<unsigned *>(utcb->esp)[0]; )

  PT_FUNC_NORETURN(do_gsi,
		   unsigned char res;
		   unsigned gsi = utcb->msg[1] & 0xff;
		   bool shared   = (utcb->msg[1] >> 8) & 1;
		   bool locked   = !(utcb->msg[1] & 0x200);
		   unsigned cap_irq = utcb->msg[0];
		   Logging::printf("s0: %s(%x) vec %x %s\n", __func__, cap_irq,  gsi, locked ? "locked" : "unlocked");
		   MessageIrq msg(shared ? MessageIrq::ASSERT_NOTIFY : MessageIrq::ASSERT_IRQ, gsi);
		   while (!(res = nova_semdownmulti(cap_irq))) {
		     COUNTER_INC("GSI");
		     if (locked) _lock_gsi.down();
		     _mb->bus_hostirq.send(msg);
		     if (locked) _lock_gsi.up();
		   }
		   Logging::panic("s0: %s(%x, %x) request failed with %x\n", __func__, gsi, cap_irq, res);
		   )

  PT_FUNC(do_startup,
	  ModuleInfo *modinfo = get_module((pid - CLIENT_PT_OFFSET) >> CLIENT_PT_SHIFT);
	  assert(modinfo);
	  // Logging::printf("s0: [%2u] eip %lx mem %p size %lx\n",
	  // 		  modinfo->id, modinfo->rip, modinfo->mem, modinfo->physsize);
	  utcb->eip = modinfo->rip;
	  utcb->esp = CLIENT_HIP;
	  utcb->mtd = MTD_RIP_LEN | MTD_RSP;
	  )

  PT_FUNC(do_request,
	  ModuleInfo *modinfo = get_module((pid - CLIENT_PT_OFFSET) >> CLIENT_PT_SHIFT);
	  assert(modinfo);

	  COUNTER_INC("request");

	  // XXX check whether we got something mapped and do not map it back but clear the receive buffer instead
	  if (utcb->head.untyped == EXCEPTION_WORDS) {
	    assert(MEM_OFFSET + modinfo->physsize <= CLIENT_BOOT_UTCB);

	    unsigned error =
	        (utcb->qual[1] < MEM_OFFSET) ||
	        (utcb->qual[1] >= MEM_OFFSET + modinfo->physsize && utcb->qual[1] < CLIENT_BOOT_UTCB) ||
	        (utcb->qual[1] >= CLIENT_BOOT_UTCB + 0x2000);

	    if ((verbose & VERBOSE_INFO) || error)
	      Logging::printf("s0: [%2u, %02x] pagefault %x/%x for %llx err %llx at %x\n",
	        modinfo->id, pid, utcb->head.untyped, utcb->head.typed, utcb->qual[1], utcb->qual[0], utcb->eip);

	    if (error)
	    {
	      Logging::printf("s0: [%2u] unresolvable pagefault - killing client ...\n", modinfo->id);
	      kill_module(modinfo);
	      return;
	    }
	    // we can not overmap -> thus remove all rights first if the PTE was present
	    if (utcb->qual[0] & 1) {
	      revoke_all_mem(modinfo->mem, modinfo->physsize, DESC_MEM_ALL, false);
	      revoke_all_mem(modinfo->hip, 0x1000, DESC_MEM_ALL, false);
	    }

	    utcb->mtd = 0;
	    add_mappings(utcb, reinterpret_cast<unsigned long>(modinfo->mem), modinfo->physsize, MEM_OFFSET | MAP_MAP, DESC_MEM_ALL);
	    add_mappings(utcb, reinterpret_cast<unsigned long>(modinfo->hip), 0x1000, CLIENT_HIP | MAP_MAP, DESC_MEM_ALL);
	    if (verbose & VERBOSE_INFO)
	      Logging::printf("s0: [%2u, %02x] map %x/%x for %llx err %llx at %x\n",
	        modinfo->id, pid, utcb->head.untyped, utcb->head.typed, utcb->qual[1], utcb->qual[0], utcb->eip);
	    return;
	  }

	  if (request_pcicfg(modinfo->id, utcb)) return;

	  SemaphoreGuard l(_lock_gsi);

	  if (!request_vnet(modinfo, utcb) && !request_disks(modinfo, utcb) &&
	      !request_console(modinfo, utcb) && !request_network(modinfo, utcb))
		switch (utcb->msg[0])
		  {
		  case REQUEST_HOSTOP:
		    {
		      MessageHostOp *msg = reinterpret_cast<MessageHostOp *>(utcb->msg+1);
		      if (utcb->head.untyped*sizeof(unsigned) < sizeof(unsigned) + sizeof(*msg)) goto fail;

		      switch (msg->type)
			{
			case MessageHostOp::OP_GET_MAC:
			  msg->mac = get_mac(modinfo->mac++ * MAXMODULES + modinfo->id);
			  utcb->msg[0] = 0;
			  break;
			case MessageHostOp::OP_ASSIGN_PCI:
			  if (modinfo->dma) {
			    utcb->msg[0] = assign_pci_device(NOVA_DEFAULT_PD_CAP + (modinfo->id << CLIENT_PT_SHIFT) + CLIENT_PT_OFFSET, msg->value, msg->len);
			      //Logging::printf("s0: assign_pci() PD %x bdf %lx vfbdf %lx = %x\n", modinfo->id, msg->value, msg->len, utcb->msg[0]);
			  } else {
			    Logging::printf("s0: [%02x] DMA access denied.\n", modinfo->id);
			  }
			  break;
			case MessageHostOp::OP_ATTACH_IRQ:
			  if ((msg->value & 0xff) < _hip->cfg_gsi) {
			    // XXX make sure only one gets it
			    unsigned gsi_cap = _hip->cfg_exc + 3 + (msg->value & 0xff);
			    unsigned res = nova_assign_gsi(gsi_cap, modinfo->cpunr);
			    if (res != NOVA_ESUCCESS) goto fail;
			    Logging::printf("s0: [%02x] gsi %lx granted\n", modinfo->id, msg->value);
			    utcb->set_header(1, 0);
			    utcb->msg[0] = 0;
			    add_mappings(utcb, gsi_cap << Utcb::MINSHIFT, 1 << Utcb::MINSHIFT, MAP_MAP, DESC_CAP_ALL);
			  }
			  else {
			    Logging::printf("s0: [%02x] irq request dropped %x pre %x nr %x\n", modinfo->id, utcb->msg[2], _hip->cfg_exc, utcb->msg[2] >> Utcb::MINSHIFT);
			    goto fail;
			  }
			  break;
			case MessageHostOp::OP_ATTACH_MSI:
			  {
			    unsigned cap = attach_msi(msg, modinfo->cpunr);
			    if (!cap) goto fail;
			    utcb->msg[0] = 0;
			    utcb->set_header(1 + sizeof(*msg)/ sizeof(unsigned), 0);
			    add_mappings(utcb, cap << Utcb::MINSHIFT, 1 << Utcb::MINSHIFT, MAP_MAP, DESC_CAP_ALL);
			  }
			  break;
			case MessageHostOp::OP_ALLOC_IOIO_REGION:
			  // XXX make sure only one gets it
			  map_self(utcb, (msg->value >> 8) << Utcb::MINSHIFT, 1 << (Utcb::MINSHIFT + msg->value & 0xff), DESC_IO_ALL);
			  utcb->set_header(1, 0);
			  utcb->msg[0] = 0;
			  add_mappings(utcb, (msg->value >> 8) << Utcb::MINSHIFT, (1 << (Utcb::MINSHIFT + msg->value & 0xff)), MAP_MAP, DESC_CAP_ALL);
			  break;
			case MessageHostOp::OP_ALLOC_IOMEM:
			  {
			    // XXX make sure it is not physmem and only one gets it
			    unsigned long addr = msg->value & ~0xffful;
			    char *ptr = map_self(utcb, addr, msg->len);
			    utcb->set_header(1, 0);
			    utcb->msg[1] = 0;
			    add_mappings(utcb, reinterpret_cast<unsigned long>(ptr), msg->len, MAP_MAP, DESC_MEM_ALL);
			    Logging::printf("s0: [%02x] iomem %lx+%lx granted from %p\n", modinfo->id, addr, msg->len, ptr);
			  }
			  break;
			case MessageHostOp::OP_ALLOC_SEMAPHORE:
			case MessageHostOp::OP_ALLOC_SERVICE_THREAD:
			case MessageHostOp::OP_ALLOC_SERVICE_PORTAL:
			case MessageHostOp::OP_GUEST_MEM:
			case MessageHostOp::OP_ALLOC_FROM_GUEST:
			case MessageHostOp::OP_VIRT_TO_PHYS:
			case MessageHostOp::OP_NOTIFY_IRQ:
			case MessageHostOp::OP_VCPU_CREATE_BACKEND:
			case MessageHostOp::OP_VCPU_BLOCK:
			case MessageHostOp::OP_VCPU_RELEASE:
			case MessageHostOp::OP_REGISTER_SERVICE:
			case MessageHostOp::OP_GET_MODULE:
			default:
			  // unhandled
			  Logging::printf("s0: [%02x] unknown request (%x,%x,%x) dropped \n", modinfo->id, utcb->msg[0],  utcb->msg[1],  utcb->msg[2]);
			  goto fail;
			}
		    }
		    break;
		  case REQUEST_ACPI:
		    {
		      MessageAcpi *msg = reinterpret_cast<MessageAcpi *>(utcb->msg+1);
		      if (utcb->head.untyped*sizeof(unsigned) < sizeof(unsigned) + sizeof(*msg)) goto fail;
		      if (msg->type == MessageAcpi::ACPI_GET_IRQ)
			utcb->msg[0] = !_mb->bus_acpi.send(*msg, true);
		    }
		    break;
		  default:
		    Logging::printf("s0: [%02x] unknown request (%x,%x,%x) dropped \n", modinfo->id, utcb->msg[0],  utcb->msg[1],  utcb->msg[2]);
		    goto fail;
		  }
	      return;
	    fail:
	      utcb->msg[0] = ~0x10u;
	      utcb->set_header(1, 0);
	  )





  bool  receive(MessageHostOp &msg)
  {
    bool res = true;
    switch (msg.type)
      {
      case MessageHostOp::OP_ATTACH_MSI:
        {
          bool unlocked = msg.len;
          unsigned cpu  = (msg.cpu == ~0U) ? _cpunr[CPUGSI % _numcpus] : msg.cpu;
          Logging::printf("Attaching to CPU %x (%x %x)\n", cpu, msg.cpu, _cpunr[CPUGSI % _numcpus]);
          assert(cpu < 32);
          unsigned cap = attach_msi(&msg, cpu);
          check1(false, !cap);
          res = attach_irq(msg.msi_gsi, cap, unlocked, cpu);
        }
        break;
      case MessageHostOp::OP_ATTACH_IRQ:
        {
          unsigned gsi = msg.value & 0xff;
          if (_gsi & (1 << gsi)) return true;
           _gsi |=  1 << gsi;
          unsigned irq_cap = _hip->cfg_exc + 3 + gsi;
          unsigned cpu = (msg.cpu == ~0U) ? _cpunr[CPUGSI % _numcpus] : msg.cpu;
          res = nova_assign_gsi(irq_cap, cpu);
          check1(false, res != NOVA_ESUCCESS);
          res = attach_irq(gsi, irq_cap, msg.len, cpu);
        }
        break;
      case MessageHostOp::OP_ALLOC_IOIO_REGION:
        //Logging::printf("s0: ALLOC_IOIO %lx\n", msg.value);
        map_self(myutcb(), (msg.value >> 8) << Utcb::MINSHIFT, 1 << (Utcb::MINSHIFT + msg.value & 0xff), DESC_IO_ALL);
        break;
      case MessageHostOp::OP_ALLOC_IOMEM:
        msg.ptr = map_self(myutcb(), msg.value, msg.len, DESC_MEM_ALL);
        break;
      case MessageHostOp::OP_ALLOC_SEMAPHORE:
        msg.value = alloc_cap();
        check1(false, nova_create_sm(msg.value));
        break;
      case MessageHostOp::OP_ALLOC_SERVICE_PORTAL:
        {
          unsigned cpu    = msg._alloc_service_portal.cpu;
          if (cpu == ~0U) cpu = _cpunr[CPUGSI % _numcpus];
          Utcb *utcb      = NULL;
          unsigned ec_cap = create_ec_helper(msg._alloc_service_portal.pt_arg,
                                             cpu, _percpu[cpu].exc_base, &utcb);
          unsigned pt = alloc_cap();
          if (!ec_cap || !utcb || !pt) return false;
          utcb->head.crd = msg._alloc_service_portal.crd;
          *msg._alloc_service_portal.pt_out = pt;
          return (nova_create_pt(pt, ec_cap,
                                 reinterpret_cast<mword>(msg._alloc_service_portal.pt),
                                 0) == NOVA_ESUCCESS);
        }
      case MessageHostOp::OP_ALLOC_SERVICE_THREAD:
        {
          unsigned cpu = msg._alloc_service_thread.cpu;
          if (cpu == ~0U) cpu = _cpunr[CPUGSI % _numcpus];
          unsigned cap_ec = create_ec_helper(msg._alloc_service_thread.work_arg, cpu,
                                             _percpu[cpu].exc_base, 0,
                                             reinterpret_cast<void *>(msg._alloc_service_thread.work));
          unsigned prio = msg._alloc_service_thread.prio;

          AdmissionProtocol::sched sched; //Qpd(prio, 10000)
          if (prio != ~0u) //IDLE TYPE_NONPERIODIC -> prio=1 
            sched.type = prio ? AdmissionProtocol::sched::TYPE_APERIODIC : AdmissionProtocol::sched::TYPE_PERIODIC; //XXX don't guess here

          return (service_admission->alloc_sc(*myutcb(), cap_ec, sched, cpu, this) == NOVA_ESUCCESS);
        }
        break;
      case MessageHostOp::OP_VIRT_TO_PHYS:
        {
          Region * r;
          {
            SemaphoreGuard l(_lock_mem);
            r = _virt_phys.find(msg.value);
          }
          if (r) {
            msg.phys_len = r->end() - msg.value;
            msg.phys     = r->phys  + msg.value - r->virt;
          } else
            res = false;
        }
        break;
      case MessageHostOp::OP_ASSIGN_PCI:
        res = !assign_pci_device(NOVA_DEFAULT_PD_CAP, msg.value, msg.len);
        break;
      case MessageHostOp::OP_GET_MAC:
        msg.mac = get_mac(_mac++ * MAXMODULES);
        break;
      case MessageHostOp::OP_REGISTER_SERVICE:
        {
          unsigned service_cap = alloc_cap();
          for (unsigned i = 0; i < _numcpus; i++) {
            Utcb *utcb = NULL;
            unsigned cpu = _cpunr[i];
            unsigned ec_cap = create_ec_helper(msg.obj, cpu, msg.excbase, &utcb);
            unsigned pt = alloc_cap();

            if (!ec_cap || !utcb || !pt) return false;
            if (msg.cap)
              utcb->head.crd = alloc_cap() << Utcb::MINSHIFT | DESC_TYPE_CAP;
            else {
              unsigned long addr;
              {
                SemaphoreGuard l(_lock_mem);
                addr = _free_virt.alloc(1 << 22, 22);
              }
              if (!addr) return false;
              utcb->head.crd = Crd(addr >> Utcb::MINSHIFT, 22 - 12, DESC_MEM_ALL).value();
            }
            if (msg.crd_t) utcb->head.crd_translate = msg.crd_t;

            check1(false, nova_create_pt(pt, ec_cap, msg.portal_func, 0));
            check1(false, ParentProtocol::register_service(*myutcb(), msg.service_name, cpu, pt, service_cap, msg.revoke_mem));
            //Logging::printf("s0: service registered on cpu %x\n", cpu);
            if (msg.portal_pf) {
              unsigned ec_pf = create_ec_helper(msg.obj, cpu, _percpu[cpu].exc_base, &utcb);
              if (!ec_pf || !utcb) return false;
              utcb->head.crd = 0;
              check1(false, nova_create_pt(msg.excbase + 0xe, ec_pf, msg.portal_pf, MTD_GPR_ACDB | MTD_GPR_BSD | MTD_QUAL | MTD_RIP_LEN | MTD_RSP));
            }
            msg.excbase += msg.excinc;
          }
          // XXX transfer the service_cap back
        }
        break;
      case MessageHostOp::OP_NOTIFY_IRQ:
      case MessageHostOp::OP_GUEST_MEM:
      case MessageHostOp::OP_ALLOC_FROM_GUEST:
      case MessageHostOp::OP_VCPU_CREATE_BACKEND:
      case MessageHostOp::OP_VCPU_BLOCK:
      case MessageHostOp::OP_VCPU_RELEASE:
      case MessageHostOp::OP_GET_MODULE:
      default:
        Logging::panic("s0: %s - unimplemented operation %x", __PRETTY_FUNCTION__, msg.type);
      }
    return res;
  }

  /************************************************************
   *   Network support                                        *
   ************************************************************/


  /**
   * Network data.
   */
  NetworkProducer _prod_network[MAXMODULES];

  void init_network() {  _mb->bus_network.add(this, receive_static<MessageNetwork>); }

  /**
   * Handle network requests from other PDs.
   */
  bool request_network(ModuleInfo * modinfo, Utcb *utcb) {

    switch(utcb->msg[0]) {
      case REQUEST_NETWORK_ATTACH:
        handle_attach<NetworkConsumer>(modinfo, _prod_network[modinfo->id], utcb);
        break;
      case REQUEST_NETWORK:
        {
          MessageNetwork *msg = reinterpret_cast<MessageNetwork *>(utcb->msg+1);
          if (utcb->head.untyped*sizeof(unsigned) < sizeof(unsigned) + sizeof(*msg))
            utcb->msg[0] = ~0x10u;
          else
          {
            MessageNetwork msg2 = *msg;
            if ((msg2.type == MessageNetwork::PACKET) &&
                 convert_client_ptr(modinfo, msg2.buffer, msg2.len))
              return false;
            msg2.client = modinfo->id;
            utcb->msg[0] = _mb->bus_network.send(msg2) ? 0 : ~0x10u;
            if (msg2.type == MessageNetwork::QUERY_MAC)
              msg->mac = msg2.mac;
          }
        }
        break;
      default:  return false;
    }
    return true;
  }

  bool  receive(MessageNetwork &msg)
  {
    if (msg.type == MessageNetwork::PACKET) {
      for (unsigned i = 0; i < MAXMODULES; i++) {
        if (i != msg.client) _prod_network[i].produce(msg.buffer, msg.len);
      }
      return true;
    }

    // Don't pass along MAC queries.
    return false;
  }

  // Virtual Network

  unsigned vnet_sm[MAXMODULES];

  void init_vnet()
  {
    _mb->bus_vnetping.add(this, receive_static<MessageVirtualNetPing>);
  }

  bool receive(MessageVirtualNetPing &msg)
  {
    // Client 0 is us. Don't wake up.
    return (msg.client == 0) ? false : (nova_semup(vnet_sm[msg.client]) == 0);
  }


  bool request_vnet(ModuleInfo * modinfo, Utcb *utcb)
  {
    switch (utcb->msg[0]) {
    case REQUEST_VNET_ATTACH: {
      assert(modinfo->id != 0);
      vnet_sm[modinfo->id] = utcb->head.crd >> Utcb::MINSHIFT;
      if (verbose & VERBOSE_INFO)
        Logging::printf("s0: [%2u] - provided VNET wakeup semaphore: %u.\n", modinfo->id, vnet_sm[modinfo->id]);
      utcb->msg[0] = 0;
      utcb->head.crd = alloc_crd();
      utcb->set_header(1, 0);
      return true;
    }
    case REQUEST_VNET: {
      MessageVirtualNet *msg = reinterpret_cast<MessageVirtualNet *>(utcb->msg+1);

      if (utcb->head.untyped*sizeof(unsigned) < sizeof(unsigned) + sizeof(*msg)) {
        return false;
      } else {
        if (convert_client_ptr(modinfo, msg->registers, 0x4000))  return false;
        if (adapt_ptr_map(modinfo, msg->physoffset, msg->physsize)) return false;
        msg->client = modinfo->id;
        utcb->msg[0] = _mb->bus_vnet.send(*msg, true);
      }
      return true;
    }
    }
    
    return false;
  }


  /************************************************************
   *   DISK support                                           *
   ************************************************************/

  enum {
    MAXDISKS           = 32,
    MAXDISKREQUESTS    = DISKS_SIZE  // max number of outstanding disk requests per client
  };

  // per client data
  struct DiskData {
    DiskProducer    prod_disk;
    unsigned char   disks[MAXDISKS];
    unsigned char   disk_count;
    struct {
      unsigned char disk;
      unsigned long usertag;
    } tags [MAXDISKREQUESTS];
  } _disk_data[MAXMODULES];


  /**
   * Global init.
   */
  void init_disks() {  _mb->bus_diskcommit.add(this, receive_static<MessageDiskCommit>); }

  /**
   * Attach drives to a module.
   */
  void attach_drives(char const *cmdline, unsigned long cmdlen, unsigned client)
  {
    for (char *p; p = strstr(cmdline,"sigma0::drive:"); cmdline = p + 1)
      {
        if (p > cmdline + cmdlen) break;
        unsigned long  nr = strtoul(p+14, 0, 0);
        if (nr < _mb->bus_disk.count() && _disk_data[client].disk_count < MAXDISKS)
          _disk_data[client].disks[_disk_data[client].disk_count++] = nr;
        else
          Logging::printf("s0: ignore drive %lx during attach!\n", nr);
      }
  }


  /**
   * Find a free disk tag for a client.
   */
  unsigned long find_free_tag(unsigned short client, unsigned char disknr, unsigned long usertag, unsigned long &tag) {

    DiskData *disk_data = _disk_data + client;

    assert (disknr < disk_data->disk_count);
    for (unsigned i = 0; i < MAXDISKREQUESTS; i++)
      if (!disk_data->tags[i].disk)
	{
	  disk_data->tags[i].disk = disknr + 1;
	  disk_data->tags[i].usertag = usertag;
	  tag = ((i+1) << 16) | client;
	  return MessageDisk::DISK_OK;
	}
    return MessageDisk::DISK_STATUS_BUSY;
  }



  /**
   * Handle disk requests from other PDs.
   */
  bool request_disks(ModuleInfo * modinfo, Utcb *utcb) {

    DiskData  *disk_data = _disk_data + modinfo->id;

    switch(utcb->msg[0]) {
    case REQUEST_DISKS_ATTACH:
      handle_attach<DiskConsumer>(modinfo, disk_data->prod_disk, utcb);
      return true;
    case REQUEST_DISK:
      {
	MessageDisk *msg = reinterpret_cast<MessageDisk *>(utcb->msg+1);
	if (utcb->head.untyped*sizeof(unsigned) < sizeof(unsigned) + sizeof(*msg))
	  return false;
	else
	  {
	    MessageDisk msg2 = *msg;

	    if (msg2.disknr >= disk_data->disk_count) { msg->error = MessageDisk::DISK_STATUS_DEVICE; utcb->head.typed = 0; return true; }
	    switch (msg2.type)
	      {
	      case MessageDisk::DISK_GET_PARAMS:
		if (convert_client_ptr(modinfo, msg2.params, sizeof(*msg2.params))) return false;
		break;
	      case MessageDisk::DISK_WRITE:
	      case MessageDisk::DISK_READ:
		if (convert_client_ptr(modinfo, msg2.dma, sizeof(*msg2.dma)*msg2.dmacount)) return false;
                if (adapt_ptr_map(modinfo, msg2.physoffset, msg2.physsize))                 return false;

		utcb->msg[0] = find_free_tag(modinfo->id, msg2.disknr, msg2.usertag, msg2.usertag);
		if (utcb->msg[0]) return true;
		assert(msg2.usertag);
		break;
	      case MessageDisk::DISK_FLUSH_CACHE:
		break;
	      default:
		return false;
	      }
	    msg2.disknr = disk_data->disks[msg2.disknr];
	    msg->error = _mb->bus_disk.send(msg2) ? MessageDisk::DISK_OK : MessageDisk::DISK_STATUS_DEVICE;
	    utcb->msg[0] = 0;
	  }
      }
      return true;
    default:
      return false;
    }
  }


  bool  receive(MessageDiskCommit &msg)
  {
    // user provided write?
    if (msg.usertag) {
      unsigned client = msg.usertag & 0xffff;
      unsigned short index = msg.usertag >> 16;
      assert(client <  MAXMODULES);
      assert(index);
      assert(index <= MAXDISKREQUESTS);
      MessageDiskCommit item(_disk_data[client].tags[index-1].disk-1, _disk_data[client].tags[index-1].usertag, msg.status);
      if (!_disk_data[client].prod_disk.produce(item))
        Logging::panic("s0: [%02x] produce disk (%x) failed\n", client, index);
      _disk_data[client].tags[index-1].disk = 0;
    }
    return true;
  }



  /************************************************************
   *   CONSOLE support                                        *
   ************************************************************/

  struct ConsoleData {
    unsigned long   console;
    StdinProducer   prod_stdin;
    char            tag[256];
  } _console_data[MAXMODULES];

  /**
   * Init the console subsystem
   */
  void init_console() {

    _mb->bus_console.add(this, receive_static<MessageConsole>);
    // alloc consoles for ourself
    MessageConsole msg1;
    msg1.clientname = 0;
    _mb->bus_console.send(msg1);
    console_id = msg1.id;

    // open 3 views
    MessageConsole msg2("sigma0",  _vga + 0x18000, 0x1000, &_vga_regs);
    msg2.id = console_id;
    _mb->bus_console.send(msg2);
    msg2.name = "HV";
    msg2.ptr += 0x1000;
    _mb->bus_console.send(msg2);
    msg2.name = "boot";
    msg2.ptr += 0x1000;
    _mb->bus_console.send(msg2);
    switch_view(_mb, 0);
  }


  void alloc_console(ModuleInfo const * modinfo, const char *cmdline) {
    unsigned client = modinfo->id;

    // format the tag
    Vprintf::snprintf(_console_data[client].tag, sizeof(_console_data[client].tag), "CPU(%x) MEM(%ld) %s", modinfo->cpunr, modinfo->physsize >> 20, cmdline);

    MessageConsole msg1;
    msg1.clientname = _console_data[client].tag;
    if (_mb->bus_console.send(msg1))  _console_data[client].console = msg1.id;

  };

  /**
   * Switch to our view.
   */
  static void switch_view(Motherboard *mb, int view=0, unsigned short consoleid = console_id)
  {
    MessageConsole msg;
    msg.type = MessageConsole::TYPE_SWITCH_VIEW;
    msg.id = consoleid;
    msg.view = view;
    mb->bus_console.send(msg);
  }


  /**
   * Handle console requests from other PDs.
   */
  bool request_console(ModuleInfo * modinfo, Utcb *utcb) {

    switch(utcb->msg[0]) {
    case REQUEST_STDIN_ATTACH:
      handle_attach<StdinConsumer>(modinfo, _console_data[modinfo->id].prod_stdin, utcb);
      break;
    case REQUEST_CONSOLE:
      {
        MessageConsole *msg = reinterpret_cast<MessageConsole *>(utcb->msg+1);
        if (utcb->head.untyped*sizeof(unsigned) < sizeof(unsigned) + sizeof(*msg)) return false;
        {
          MessageConsole msg2 = *msg;
          if ((msg2.type != MessageConsole::TYPE_ALLOC_VIEW
              && msg2.type != MessageConsole::TYPE_SWITCH_VIEW
              && msg2.type != MessageConsole::TYPE_GET_MODEINFO
              && msg2.type != MessageConsole::TYPE_GET_FONT)
            || (msg2.type == MessageConsole::TYPE_ALLOC_VIEW
              && (convert_client_ptr(modinfo, msg2.ptr, msg2.size)
                || convert_client_ptr(modinfo, msg2.name, 4096)
                || convert_client_ptr(modinfo, msg2.regs, sizeof(*msg2.regs))))
            || (msg2.type == MessageConsole::TYPE_GET_FONT
	            &&  convert_client_ptr(modinfo, msg2.ptr, 0x1000))
            || (msg2.type == MessageConsole::TYPE_GET_MODEINFO
              && convert_client_ptr(modinfo, msg2.info, sizeof(*msg2.info)))
            || !_console_data[modinfo->id].console)
            break;
          msg2.id = _console_data[modinfo->id].console;
          // alloc a new console and set the name from the commandline
          utcb->msg[0] = !_mb->bus_console.send(msg2);
          if (!utcb->msg[0])      msg->view = msg2.view;
        }
      }
      break;
    default: return false;
    }
    return true;
  }



  bool  receive(MessageConsole &msg) {

    switch (msg.type) {
    case MessageConsole::TYPE_KEY:
      // forward the key to the named console
      for (unsigned i=1; i < MAXMODULES; i++)
        if (_console_data[i].console == msg.id)
        {
          MessageInput item((msg.view << 16) | (msg.input_device & 0xffff), msg.input_data);
          _console_data[i].prod_stdin.produce(item);
          return true;
        }
      Logging::printf("s0: drop input %x at console %x.%x\n", msg.input_data, msg.id, msg.view);
      break;
    case MessageConsole::TYPE_RESET:
      if (msg.id == 0)
        {
          Logging::printf("s0: flush disk caches for reboot\n");
          for (unsigned i = 0; i < _mb->bus_disk.count(); i++) {
            MessageDisk msg2(MessageDisk::DISK_FLUSH_CACHE, i, 0, 0, 0, 0, 0, 0);
            if (!_mb->bus_disk.send(msg2))  Logging::printf("s0: could not flush disk %d\n", i);
          }
          return true;
        }
      break;
    case MessageConsole::TYPE_START:
      {
        unsigned res;
        unsigned internal_id;
        if (msg.id == (~0U & ((1 << (sizeof(msg.id) * 8)) - 1) )) {
          res = start_config(myutcb(), msg.cmdline, internal_id);
          if (res) {
            Logging::printf("s0: start of config failed, error line = %d, config :\n'", res);
            fancy_output(msg.cmdline, 4096);
          } else msg.id = internal_id;
        } else {
          res = start_config(myutcb(), msg.id, internal_id);
          if (res)
            Logging::printf("s0: start of config failed, error line = %d, config id=%d\n", res, msg.id);
          else msg.id = internal_id;
        }
        return !res;
      }
    case MessageConsole::TYPE_KILL:
      {
        if (admission && msg.id < 2) return false;
        unsigned res = kill_module(get_module(msg.id));
        if (res) Logging::printf("s0: [%02x] kill module = %u\n", msg.id, res);
      }
      return true;
    case MessageConsole::TYPE_DEBUG:
      switch (msg.id) {
      case 0:  _mb->dump_counters(); break;
      case 3:
        {
          static unsigned unmap_count;
          unmap_count--;
          for (unsigned i = 1; i <= MAXMODULES; i++)
            if (_modinfo[i].mem) {
              unsigned rights = (unmap_count & 7) << 2;
              Logging::printf("s0: revoke all rights %x\n", rights);
              revoke_all_mem(_modinfo[i].mem, _modinfo[i].physsize, rights, false);
            }
        }
        break;
      }
      Logging::printf("s0: DEBUG(%x) = %x time %llx\n", msg.id, nova_syscall(15, msg.id, 0, 0, 0), _mb->clock()->time());
      return true;
    case MessageConsole::TYPE_ALLOC_CLIENT:
    case MessageConsole::TYPE_ALLOC_VIEW:
    case MessageConsole::TYPE_SWITCH_VIEW:
    case MessageConsole::TYPE_GET_MODEINFO:
    case MessageConsole::TYPE_GET_FONT:
    default:
      break;
    }
    return false;
  }



  /************************************************************
   * PCI Cfg                                                  *
   ************************************************************/

  bool request_pcicfg(unsigned client, Utcb *utcb) {
    switch(utcb->msg[0]) {
    case REQUEST_PCICFG:
      {
        MessagePciConfig *msg = reinterpret_cast<MessagePciConfig *>(utcb->msg+1);
        if (utcb->head.untyped*sizeof(unsigned) < sizeof(unsigned) + sizeof(*msg))
          utcb->msg[0] = ~0x10u;
        else {
          COUNTER_INC("request pci");
          utcb->msg[0] = !_mb->bus_hwpcicfg.send(*msg, true);
        }
      }
      break;
    default:
      return false;
    }
    return true;
  }


  /************************************************************
   * Application                                              *
   ************************************************************/

  /**
   * A very simple hash function.
   */
  unsigned hash(unsigned state, unsigned value) {  return ((state << 1) | (state >> 31)) ^ value; }


  /**
   * Generate a pseudo-random but persistent hostmac by hashing all
   * PCI device IDs, their BDFs and the serial numbers of devices.
   */
  unsigned generate_hostmac() {
    HostPci pci(_mb->bus_hwpcicfg, _mb->bus_hostop);
    unsigned state = 0;

    for (unsigned bus = 0; bus < 256; bus++) {
      for (unsigned dev=0; dev < 32; dev++) {
        unsigned char maxfunc = 1;
        for (unsigned func=0; func < maxfunc; func++) {
          unsigned short bdf =  (bus << 8) | (dev << 3) | func;
          unsigned devid = pci.conf_read(bdf, 0);
          if (devid == ~0UL) continue;
          if (maxfunc == 1 && pci.conf_read(bdf, 3) & 0x800000) maxfunc = 8;

          state = hash(state, bdf);
          state = hash(state, devid);

          // find device serial number
          unsigned offset = pci.find_extended_cap(bdf, 0x0003);
          if (offset)
            state = hash(hash(state, pci.conf_read(bdf, offset + 1)), pci.conf_read(bdf, offset + 2));
        }
      }
    }
    return state;
  }


  /**
   * Return a MAC by adding the MAC-prefix, the host-MAC and a client
   * specific number.
   */
  unsigned long long get_mac(unsigned clientnr) {
    return (static_cast<unsigned long long>(mac_prefix) << 16) + (static_cast<unsigned long long>(mac_host) << 8) + clientnr;
  }


  void  __attribute__((noreturn)) run(Utcb *utcb, Hip *hip)
  {
    unsigned res;

    if ((res = preinit(utcb, hip)))              Logging::panic("s0: init() failed with %x\n", res);
    // This printf() does not produce anything. The output appears
    // only after create_host_devices() is called below.
    Logging::printf("s0:  hip %p caps %x memsize %x\n", hip, hip->cfg_cap, hip->mem_size);
    if ((res = init_memmap(utcb)))                    Logging::panic("s0: init memmap failed %x\n", res);
    if ((res = create_worker_threads(hip, -1)))       Logging::panic("s0: create worker threads failed %x\n", res);

    // init rest which could be postponed until now
    postinit(__hip);
    // now service_parent exists create parent threads
    if ((res = service_parent->create_threads(this))) Logging::panic("s0: create parent threads failed %x\n", res);

    // unblock the worker and IRQ threads
    _lock_gsi.up();

    if ((res = create_host_devices(utcb, __hip)))  Logging::panic("s0: create host devices failed %x\n", res);

    if (!mac_host) mac_host = generate_hostmac();

    if (res = service_admission->push_scs(*utcb)) Logging::printf("s0: WARNING admission service missing - error %x\n", res);
    else service_admission->set_name(*utcb, "sigma0");

    Logging::printf("s0:\t=> INIT done <=\n\n");

    // block ourself since we have finished initialization
    block_forever();
  }

  static void start(Hip *hip, Utcb *utcb) asm ("start") __attribute__((noreturn));
  Sigma0() :  _numcpus(0), _modinfo(), _gsi(0), _pcidirect()  {}
};

char const * s0_ParentProtocol::get_client_cmdline(unsigned identity, unsigned long &s0_cmdlen) {
  unsigned clientnr = get_client_number(identity);
  if (clientnr >= MAXMODULES) return 0;
  s0_cmdlen = sigma0->_modinfo[clientnr].sigma0_cmdlen;
  return sigma0->_modinfo[clientnr].cmdline;
}

char * s0_ParentProtocol::get_client_memory(unsigned identity, unsigned client_mem_revoke) {
  unsigned clientnr = get_client_number(identity);
  //that are we - so service is in this pd
  if (clientnr == 0) return reinterpret_cast<char *>(client_mem_revoke);

  ModuleInfo * modinfo = sigma0->get_module(clientnr);
  if (!modinfo) return 0;
  char * mem_revoke = reinterpret_cast<char *>(client_mem_revoke);
  if (sigma0->convert_client_ptr(modinfo, mem_revoke, 0x1000U)) return 0;
  else return mem_revoke;
}

void Sigma0::start(Hip *hip, Utcb *utcb) {
  extern unsigned __nova_api_version;
  assert(hip->api_ver == __nova_api_version);
  assert(hip->cfg_exc + 0 ==  NOVA_DEFAULT_PD_CAP);
  static Sigma0 s0;
  sigma0 = &s0;
  s0.run(utcb, hip);
}


void  do_exit(const char *msg)
{
  if (consolesem)  consolesem->up();
  Logging::printf("s0:__exit(%s)\n", msg);
  if (global_mb) Sigma0::switch_view(global_mb);

  while (1)
    asm volatile ("ud2a" : : "a"(msg));
}

Semaphore Sigma0::_lock_mem;


//  LocalWords:  utcb
