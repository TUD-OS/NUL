/*
 * Main sigma0 code.
 *
 * Copyright (C) 2008-2010, Bernhard Kauer <bk@vmmon.org>
 * Copyright (C) 2011, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
 * Copyright (C) 2011-2012, Alexander Boettcher <boettcher@tudos.org>
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
#include <nul/service_events.h>
#include "s0_admission.h"

// global VARs
Motherboard *global_mb;
class Sigma0;
Sigma0      *sigma0;
Semaphore   *consolesem;
unsigned     mac_prefix = 0x42000000;
unsigned     mac_host;

// extra_HANDLER_HANDLER params
PARAM_HANDLER(mac_prefix, "mac_prefix:value=0x42000000 - override the MAC prefix.") { mac_prefix = argv[0]; }
PARAM_HANDLER(mac_host, "mac_host:value - override the host part of the MAC.") 	    { mac_host = argv[0]; }
enum { VERBOSE_ERRORS = 0, VERBOSE_INFO = 1 } verbose = VERBOSE_ERRORS;
PARAM_HANDLER(verbose, "verbose - print additional information during runtime") { verbose = VERBOSE_INFO; }

PARAM_ALIAS(S0_DEFAULT,   "an alias for the default sigma0 parameters",
            " ioio hostacpi pcicfg mmconfig atare"
            " hostreboot:0 hostreboot:1 hostreboot:2 hostreboot:3"
            " service_tracebuffer service_per_cpu_timer service_romfs service_embeddedromfs boot_s0_services script")

#define S0_DEFAULT_CMDLINE "namespace::/s0 name::/s0/timer name::/s0/fs/rom name::/s0/admission name::/s0/fs/embedded quota::guid"

#define LOG_VERBOSE(X, ...) do { if (verbose & VERBOSE_INFO) Logging::printf(X, __VA_ARGS__); } while (0)

  // module data
  struct ModuleInfo
  {
    unsigned        id;
    enum { TYPE_APP, TYPE_S0, TYPE_ADMISSION } type;
    unsigned        cpunr;
    unsigned long   rip;
    bool            dma;
    unsigned long   physsize;
    unsigned        mac;
    void *          hip;
    char const *    cmdline;
    unsigned long   sigma0_cmdlen;

    // Network statistics
    unsigned long long net_rx;
    unsigned long long net_rx_packets;
    unsigned long long net_rx_drop;
    unsigned long long net_tx;
    unsigned long long net_tx_packets;

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

    CLIENT_PT_SHIFT    = Config::CAP_RESERVED_ORDER,
    CLIENT_PT_ORDER    = CLIENT_PT_SHIFT + Config::MAX_CLIENTS_ORDER,

    // The offset at which the client capability space starts. This
    // needs to be naturally aligned to CLIENT_PT_ORDER+1 to make
    // service_parent happy who puts all its caps after
    // [CLIENT_PT_OFFSET, CLIENT_PT_OFFSET + (1U << CLIENT_PT_ORDER))
    // and expects to be able to use a single translation window to
    // cover both.
    CLIENT_PT_OFFSET   = 1U << (CLIENT_PT_ORDER+1),
  };

// See comment above.
static_assert((CLIENT_PT_OFFSET & ((1U << (CLIENT_PT_ORDER+1))-1)) == 0, "Capability space misconfiguration");

#include "parent_protocol.h"

/**
 * Sigma0 application class.
 */
struct Sigma0 : public Sigma0Base, public NovaProgram, public StaticReceiver<Sigma0>
{
  // a mapping from virtual cpus to the physical numbers
  phy_cpu_no _cpunr[MAXCPUS];
  unsigned   _numcpus;
  unsigned   _last_affinity;

  // Capabilities
  unsigned   _irq_cap_base;

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

  //initialization tracking
  bool initialized_console;
  bool initialized_s0_tasks;

  // Hip containing virtual address for aux, use _hip if physical addresses are needed
  Hip * __hip;

  // IRQ handling
  unsigned  _msivector;        // next gsi vector
  unsigned long long _gsi;     // bitfield per used GSI
  unsigned _pcidirect[MAXPCIDIRECT];

  // parent protocol
  friend class s0_ParentProtocol;

  // services
  s0_AdmissionProtocol * service_admission;
  s0_ParentProtocol    * service_parent;

  char *map_self(Utcb *utcb, unsigned long physmem, unsigned long size, unsigned rights, bool frame = false)
  {
    unsigned long virt = 0;
    unsigned old_crd = utcb->head.crd;
    // we align to order but not more than 4M
    unsigned order = Cpu::bsr(size | 1);
    if (order < 12 || order > 22) order = 22;
    unsigned alignment = (1 << order) - 1;
    unsigned long ofs = physmem & alignment;
    unsigned long offset = 0;

    assert(size);
    size    += ofs;
    physmem -= ofs;

    if (size & alignment) size += alignment + 1 - (size & alignment);

    if (frame) utcb->add_frame();
    *utcb << Crd(0, 20, rights);

    if ((rights & 3) == DESC_TYPE_MEM)
    {
      SemaphoreGuard l(_lock_mem);

      unsigned long s = _virt_phys.find_phys(physmem, size);
      if (s) { virt = s; goto cleanup; }
      if (!(virt = _free_virt.alloc(size, MAX(Cpu::minshift(physmem, size), 22U)))) goto cleanup;
    }
    while (size > offset)
    {
      utcb->set_header(0, 0);
      unsigned long s = utcb->add_mappings(physmem + offset, size - offset, (virt + offset) | MAP_DPT | MAP_HBIT, rights, frame, Utcb::STACK_START / 2 );
      LOG_VERBOSE("s0: map self %lx -> %lx size %lx offset %lx s %lx typed %x\n",
                  physmem, virt, size, offset, s, utcb->head.typed);

      memmove(utcb->msg, utcb->item_start(), sizeof(unsigned) * utcb->head.typed * 2);
      utcb->set_header(2*utcb->head.typed, 0);

      unsigned err;
      if ((err = nova_call(_percpu[utcb->head.nul_cpunr].cap_pt_echo)))
      {
        Logging::printf("s0: map_self failed with %#x\n", err);
        break;
      }
      offset = size - s;
    }
    if ((rights & 3) == DESC_TYPE_MEM && offset) {
      SemaphoreGuard l(_lock_mem);
      _virt_phys.add(Region(virt, offset, physmem)); //add memory which where mapped (offset) --> offset != size in case of an error
    }
    if (offset != size) virt = 0; //Not all memory could be mapped

    cleanup:

    if (frame) utcb->drop_frame();
    else {
      //XXX if a mapping fails undo already done mappings and add free space back to free_virt !
      utcb->head.crd = old_crd;
      //take care that utcb/frame code don't fail if it is used after map_self
      utcb->reset();
    }

    return (!virt ? 0 : reinterpret_cast<char *>(virt) + ofs);
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
      res = map_self(utcb, src, size, DESC_MEM_ALL, true) + offset;
    } while (strnlen(res, size - offset) == (size - offset));
    return res;
  }

  bool attach_irq(unsigned gsi, unsigned cap_sm, bool unlocked, phy_cpu_no cpunr, char const * _name = 0)
  {
    char name[32];
    Vprintf::snprintf(name, sizeof(name), "irq %3u - ", gsi);
    if (_name)
      memcpy(name + strlen(name), _name, MIN((sizeof(name) - strlen(name)), (strlen(_name) + 1)));
    name[sizeof(name) - 1] = 0;

    Utcb *u = 0;
    unsigned cap_ec = create_ec_helper(this, cpunr, _percpu[cpunr].exc_base,  &u,
                                       reinterpret_cast<void *>(&do_gsi_wrapper));
    u->msg[0] =  cap_sm;
    u->msg[1] =  gsi | (unlocked ? 0x200 : 0x0);
    AdmissionProtocol::sched sched(AdmissionProtocol::sched::TYPE_SYSTEM); //XXX special handling here to get it to prio 4, get rid of that

    return (service_admission->alloc_sc(*myutcb(), cap_ec, sched, cpunr, this, name) == NOVA_ESUCCESS);
  }

  /**
   * Returns a pointer to the memory-mapped config space of a PCI device.
   */
  void *get_config_space(uint16 rid)
  {
    MessageHwPciConfig msg_resolve(rid);
    msg_resolve.ptr = NULL;
    if (not _mb->bus_hwpcicfg.send(msg_resolve))
      return NULL;
    return msg_resolve.ptr;
  }

  // HPET is special because, we give the kernel its MMIO space and
  // not the MMCONFIG page.
  unsigned attach_msi(MessageHostOp *msg, phy_cpu_no cpunr) {
    void    *mconf   = msg->is_hpet ?
      reinterpret_cast<void *>(msg->value) : get_config_space(msg->value);
    if (mconf == NULL) {
      Logging::printf("s0: could not assign gsi for device %lx:%lx:%lx. No mmconfig?\n", msg->value >> 8 & 0xff, msg->value >> 3 & 0x1f, msg->value & 0x7);
      return false;
    }
    //XXX if attach failed msivector should be reused!
    msg->msi_gsi = _hip->cfg_gsi - ++_msivector;
    if (msg->msi_gsi >= _hip->cfg_gsi) return 0;
    unsigned irq_cap = _irq_cap_base + msg->msi_gsi;

    unsigned res = nova_assign_gsi(irq_cap, cpunr, mconf, &msg->msi_address, &msg->msi_value);
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
   * Convert client's virtual address (physoffset) to a physical
   * address. Also truncates size if it points behind memory allocated
   * to the client.
   *
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
	// Create producer attached to consumer's buffer and semaphore 
        res = PRODUCER(con, utcb->head.crd >> Utcb::MINSHIFT);
        utcb->msg[0] = 0;
      }

    utcb->head.crd = alloc_crd();
    utcb->set_header(1, 0);
  }

  void postinit(Hip * hip) {
    // parse cmdline of sigma0
    char * cmdline = reinterpret_cast<char *>(hip->get_mod(0)->aux);
    memset(&_modinfo[0], 0, sizeof(_modinfo[0]));
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

    // init services required or provided by sigma0
    service_parent    = new (sizeof(void *)*2) s0_ParentProtocol(CLIENT_PT_OFFSET + (1 << CLIENT_PT_ORDER), CLIENT_PT_ORDER, CLIENT_PT_OFFSET, CLIENT_PT_ORDER + 1);
    service_admission = new s0_AdmissionProtocol(alloc_cap(AdmissionProtocol::CAP_SERVER_PT + hip->cpu_desc_count()), true, hip->cpu_count() + 16);
  }

  /**
   * Create the needed host devices aka instantiate the drivers.
   */
  unsigned create_host_devices(Utcb *utcb, Hip *hip)
  {
    _mb = new Motherboard(new Clock(hip->freq_tsc*1000), hip);
    global_mb = _mb;
    _mb->bus_hostop.add(this,  receive_static<MessageHostOp>);

    char * cmdline = reinterpret_cast<char *>(hip->get_mod(0)->aux);
    if (strstr(cmdline, "service_disk") == NULL) // Legacy protocol will be used
      init_disks();
    init_network();
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

    if (!st) Logging::printf("NULL");
    else
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


  // Returns the exception handler address for a given exception.
  unsigned long exception_handler_address(unsigned vector)
  {
    switch (vector) {
    case 1:  return reinterpret_cast<unsigned long>(do_singlestep_wrapper);
    case 3:  return reinterpret_cast<unsigned long>(do_breakpoint_wrapper);
    case 0x1E: return reinterpret_cast<unsigned long>(do_thread_startup_wrapper);
    case 0x1F: return reinterpret_cast<unsigned long>(do_recall_wrapper);
    default: return reinterpret_cast<unsigned long>(do_error_wrapper);
    }
  }

  /// Create exception and startup handling portals. If cpunr is
  /// given, it only initializes this single CPU and assumes that this
  /// is the bootstrap CPU, i.e. exception handling portals start at
  /// capability index 0.
  unsigned create_worker_threads(Hip *hip, phy_cpu_no cpunr) {
    for (unsigned i = 0; i < hip->cpu_desc_count(); i++) {
      Hip_cpu const *cpu = &hip->cpus()[i];
      if (not cpu->enabled() or (cpunr != ~0U && i != cpunr)) continue;
      Logging::printf("s0: cpu[%x]: %x:%x:%x\n", i, cpu->package, cpu->core, cpu->thread);
      // have we created it already?
      if (_percpu[i].cap_ec_echo)  continue;
      _cpunr[_numcpus++] = i;

      // Create the ec_echo with an invalid exception base.
      _percpu[i].cap_ec_echo = create_ec4pt(this, i, 0);
      _percpu[i].cap_pt_echo = alloc_cap();
      check1(1, nova_create_pt(_percpu[i].cap_pt_echo, _percpu[i].cap_ec_echo, reinterpret_cast<unsigned long>(do_map_wrapper), 0));

      if (i == cpunr)
        // Special case for the bootstrap thread.
        _percpu[i].exc_base = 0;
      else
        _percpu[i].exc_base = alloc_cap(0x20);

      Utcb *utcb = 0;
      _percpu[i].cap_ec_worker = create_ec4pt(this, i, _percpu[i].exc_base, &utcb);
      utcb->head.crd = alloc_crd();

      // Create error and startup handling portals. Threads that use
      // ec_echo have broken error handling (because there is no one
      // to handle them left...)
      for (unsigned pt = 0; pt <= 0x1F; pt++)
        check1(4, nova_create_pt(_percpu[i].exc_base + pt, _percpu[i].cap_ec_echo,
                                 exception_handler_address(pt),
                                 MTD_ALL));
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
    //reserve cap range from CLIENT_PT_OFFSET - is used by sigma0 implicitly without a cap allocator !
    assert(_cap_start == 0 && _cap_order == 16);
    Region * reserve = _cap_region.find(CLIENT_PT_OFFSET);
    assert(reserve and reserve->virt <= CLIENT_PT_OFFSET and
           reserve->size - (reserve->virt - CLIENT_PT_OFFSET) >= (1 << (CLIENT_PT_ORDER+1)));

    // Clients range + parent_protocol range.
    _cap_region.del(Region(CLIENT_PT_OFFSET, 1U << (CLIENT_PT_ORDER+1)));

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
    _vga = map_self(utcb, 0xa0000, 1<<17, DESC_MEM_ALL, true);

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
    Utcb::TypedMapCap(CLIENT_PT_OFFSET + ParentProtocol::CAP_PARENT_ID, DESC_CAP_ALL, ParentProtocol::CAP_PARENT_ID)
      .fill_words(utcb->msg);
    *utcb << Crd(0, 31, DESC_CAP_ALL);
    utcb->set_header(2, 0);

    // map all IRQs
    _irq_cap_base = alloc_cap(hip->cfg_gsi);

    for (unsigned gsi=0; gsi < hip->cfg_gsi; gsi++) {
      Utcb::TypedMapCap(hip->gsi_cap_base() + gsi, DESC_CAP_ALL, _irq_cap_base + gsi, MAP_HBIT)
        .fill_words(utcb->msg + utcb->head.untyped);
      utcb->head.untyped += 2;
    }

    check1(8, nova_call(_percpu[utcb->head.nul_cpunr].cap_pt_echo));

    //reset utcb
    *utcb << Crd(0,0,0);
    utcb->reset();
    return 0;
  };


  /**
   * Request memory from the memmap. Minimum alignment is 16-bytes
   * (for SSE stuff).
   */
  static void *sigma0_memalloc(unsigned long size, unsigned long align) {
    unsigned long offset = 0x10;

    if (!size) return 0;
    if (align < 0x10) align = 0x10;

    size = (size + 0xF) & ~0xF;
    unsigned long long pmem;
    {
      SemaphoreGuard l(_lock_mem);
      pmem = sigma0->_free_phys.alloc(size, Cpu::bsr(align | 1), offset);
    }
    assert ((pmem == 0) or ((pmem + offset) & 0xF) == 0);

    char *res;
    if (!pmem || !(res = sigma0->map_self(myutcb(), pmem, size + offset, DESC_MEM_ALL, true))) {
      Logging::printf("s0: %s(%lx, %lx) EOM!\n", __func__, size, align); return 0; }

    assert ((reinterpret_cast<unsigned long>(res + offset) & 0xF) == 0);
    memset(res, 0, size + offset);
    *(reinterpret_cast<unsigned long *>(res + offset) - 2) = 0x55555555;
    *(reinterpret_cast<unsigned long *>(res + offset) - 1) = size;

    //Logging::printf("memalloc pmem-%lu=%#llx, res-%lu=%p res=%p size=%#lx\n", offset, pmem, offset, res, res + offset, size);

    return res + offset;
  }

  static void sigma0_memfree(void * ptr) {
    //static unsigned long long sum;
    unsigned offset = 0x10;

    if (!ptr) return;
    unsigned long size  = *(reinterpret_cast<unsigned long *>(ptr) - 1);
    unsigned long magic = *(reinterpret_cast<unsigned long *>(ptr) - 2);
    if (magic != 0x55555555) Logging::panic("memfree %p - corrupted memory pointer\n", ptr);

    //Logging::printf("memfree - size=%#lx (%p) - freed overall %#llx Bytes\n", size, ptr, (sum += size));//__builtin_return_address(0), __builtin_return_address(1));
    {
      SemaphoreGuard l(_lock_mem);

      unsigned long vptr = reinterpret_cast<unsigned long>(ptr) - offset;
      Region * r = sigma0->_virt_phys.find(vptr);
      assert(r);
      assert(vptr - r->virt + size + offset <= r->size);

      Region * s = sigma0->_free_phys.find(r->phys + (vptr - r->virt));
      assert(!s);
      if (r->phys + (vptr - r->virt) >= (1ULL << 20))
        sigma0->_free_phys.add(Region(r->phys + (vptr - r->virt) , size + offset, r->phys + (vptr - r->virt)));
      else
        Logging::printf("memfree - skipped phys memory below first 1M\n");
    }
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
          map_self(utcb, hmem->addr, (hmem->size + 0xfff) & ~0xffful, DESC_MEM_ALL, true);
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
    memfree  = sigma0_memfree;

    // create a hip containing virtual addresses for aux
    if (_hip->length > 0x1000) return 1;
    __hip = reinterpret_cast<Hip *>(new (0x1000) char[0x1000]);
    if (!__hip) return 1;
    memcpy(__hip, _hip, _hip->length);

    // Size of all command lines combined. Used later to evacuate them
    // into high memory (above 1M).
    size_t cmdlines_length = 0;
    bool   need_evacuation = false;

    for (int i = 0; i < (__hip->length - __hip->mem_offs) / __hip->mem_size; i++) {
      Hip_mem *hmem = reinterpret_cast<Hip_mem *>(reinterpret_cast<char *>(__hip) + __hip->mem_offs) + i;
      if (hmem->type == -2) {
        if (hmem->size == 0) {
          // It is an error to access this module, because it is zero bytes long.
          hmem->addr = 0;
        } else 
          hmem->addr = reinterpret_cast<unsigned long>(map_self(utcb, hmem->addr, (hmem->size + 0xfff) & ~0xffful, DESC_MEM_ALL, true));
      } 
      if (hmem->aux) {
        need_evacuation = need_evacuation or (hmem->aux < (1U<<20));
        char *cmdline = map_string(utcb, hmem->aux);
        cmdlines_length += strlen(cmdline) + 1;
        hmem->aux = reinterpret_cast<unsigned>(cmdline);
      }
    }

    if (need_evacuation) {
      char *cmdline_buf = new char[cmdlines_length];
      char *cur = cmdline_buf;

      for (int i = 0; i < (__hip->length - __hip->mem_offs) / __hip->mem_size; i++) {
        Hip_mem *hmem = reinterpret_cast<Hip_mem *>(reinterpret_cast<char *>(__hip) + __hip->mem_offs) + i;
        if (hmem->aux) {
          char *orig  = reinterpret_cast<char *>(hmem->aux);
          hmem->aux = reinterpret_cast<unsigned long>(cur);
          strcpy(cur, orig);
          cur += strlen(orig)+1;
        }
      }
    }
    __hip->fix_checksum();

    return 0;
  }

  #include "s0_modules.h"

  /**
   * Assign a PCI device to a PD. It makes sure only the first will
   * get it.
   * \param bdf Bus/Device/Function
   */
  bool assign_pci_device(unsigned pd_cap, unsigned bdf, unsigned hint)
  {
    // Check if the device is already allocated.
    for (unsigned i = 0; i < MAXPCIDIRECT; i++)
      if (bdf == _pcidirect[i]) {
        Logging::printf("s0: PCI device %x:%x:%x cannot be directly assigned again!\n", bdf >> 8 & 0xff, bdf >> 3 & 0x1f, bdf & 0x7);
        return false;
      }

    // Allocate a new slot and assign it.
    for (unsigned i = 0; i < MAXPCIDIRECT; i++)
      if (!_pcidirect[i]) {
        MessageHwPciConfig msg_resolve(bdf);
        if (not _mb->bus_hwpcicfg.send(msg_resolve)) {
          Logging::printf("s0: could not assign device %x:%x:%x. No mmconfig?\n", bdf >> 8 & 0xff, bdf >> 3 & 0x1f, bdf & 0x7);
          return false;
        }
        
        unsigned res = nova_assign_pci(pd_cap, msg_resolve.ptr, hint);
        if (res == NOVA_ESUCCESS) {
          _pcidirect[i] = bdf;
          return true;
        } else {
          Logging::printf("s0: assign_pci(%p, %x) failed: %u\n", msg_resolve.ptr, hint, res);
          return false;
        }
      }

    return false;
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

  PT_FUNC(do_breakpoint,
          if (consolesem) consolesem->up();
          Logging::printf(">>> Break EIP %08x ESP %08x EFLAGS %08x\n", utcb->eip, utcb->esp, utcb->efl);
          if (consolesem) consolesem->down();
          )

  PT_FUNC(do_singlestep,
          if (consolesem) consolesem->up();
          //const uint8 *prefix = reinterpret_cast<const uint8 *>(utcb->eip);
          Logging::printf(">>> Step  EIP %08x ESP %08x EFLAGS %08x\n", utcb->eip, utcb->esp, utcb->efl);
          if (consolesem) consolesem->down();
          )

  PT_FUNC(do_recall,
          utcb->efl ^= 0x100; // TF
          Logging::printf("RECALL EFLAGS %08x EIP %08x\n", utcb->efl, utcb->eip);
          utcb->mtd  = MTD_RFLAGS;
          )

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
		   //Logging::printf("s0: %s(%x) vec %x %s\n", __func__, cap_irq,  gsi, locked ? "locked" : "unlocked");
		   MessageIrq msg(shared ? MessageIrq::ASSERT_NOTIFY : MessageIrq::ASSERT_IRQ, gsi);
		   while (!(res = nova_semdownmulti(cap_irq))) {
		     COUNTER_INC("GSI");
		     if (locked) _lock_gsi.down();
		     _mb->bus_hostirq.send(msg);
		     if (locked) _lock_gsi.up();
		   }
		   Logging::panic("s0: %s(%x, %x) request failed with %x\n", __func__, gsi, cap_irq, res);
		   )

  /**
   * Handle startup exception of spawned childs.
   */
  PT_FUNC(do_startup,
	  ModuleInfo *modinfo = get_module((pid - CLIENT_PT_OFFSET) >> CLIENT_PT_SHIFT);
	  assert(modinfo);
	  // Logging::printf("s0: [%2u] eip %lx mem %p size %lx\n",
	  // 		  modinfo->id, modinfo->rip, modinfo->mem, modinfo->physsize);

          utcb->eax = modinfo->cpunr;
          utcb->ecx = utcb->edx = utcb->ebx = 0;
	  utcb->eip = modinfo->rip;
	  utcb->esp = CLIENT_HIP;
	  utcb->mtd = MTD_RIP_LEN | MTD_RSP | MTD_GPR_ACDB;
	  )


  /** Translate a page fault (given by fault and err) into a memory
   * region [begin, begin+size).
   * TODO Don't hardcode memory layout.
   */
  bool translate_client_fault(ModuleInfo *modinfo, mword fault, mword err,
                              mword &translated, mword &begin, mword &size, unsigned &rights)
  {
    if (in_range(fault, CLIENT_HIP, 0x1000)) {
      // Fault in HIP
      translated = mword(modinfo->hip) + (fault & 0xFFF);
      begin      = mword(modinfo->hip);
      size       = 0x1000;
      rights     = DESC_MEM_ALL;
      return true;
    } else if ((fault < CLIENT_BOOT_UTCB) and
               in_range(fault, MEM_OFFSET, modinfo->physsize)) {
      // Fault in application heap (includes code). The
      // assertion above (1) prevents mapping too much memory
      // due to misconfiguration.

      translated = mword(modinfo->mem) + fault - MEM_OFFSET;
      begin      = mword(modinfo->mem);
      rights     = DESC_MEM_ALL;
      size       = modinfo->physsize;
      return true;
    }

    // No region matches
    size   = 0;
    rights = 0;
    return false;
  }

  /**
   * Handle child page-faults and "legacy call interface" requests.
   */
  PT_FUNC(do_request,
	  ModuleInfo *modinfo = get_module((pid - CLIENT_PT_OFFSET) >> CLIENT_PT_SHIFT);
	  assert(modinfo);

	  COUNTER_INC("request");

	  // XXX check whether we got something mapped and do not map it back but clear the receive buffer instead
	  if (utcb->head.untyped == EXCEPTION_WORDS) {
	    assert(MEM_OFFSET + modinfo->physsize <= CLIENT_BOOT_UTCB);

	    LOG_VERBOSE("s0: [%2u, %02x] pagefault %x/%x for %llx err %llx at %x\n",
	        modinfo->id, pid, utcb->head.untyped, utcb->head.typed, utcb->qual[1], utcb->qual[0], utcb->eip);

            // Figure out memory region. This can be replaced by
            // region list lookup in the future.
            mword fault = utcb->qual[1];
            mword begin;
            mword size;
            mword translated; // Memory region hit by page fault
            unsigned rights;

            if (not translate_client_fault(modinfo, fault, utcb->qual[0],
                                           translated, begin, size, rights)) {
              // Print pagefault info for unresolvable pfs even when verbose is not specified
              Logging::printf("s0: [%2u, %02x] pagefault %x/%x for %llx err %llx at %x\n",
                              modinfo->id, pid, utcb->head.untyped, utcb->head.typed, utcb->qual[1], utcb->qual[0], utcb->eip);
	      Logging::printf("s0: [%2u] unresolvable pagefault - killing client ...\n", modinfo->id);
	      utcb->reset(); //make sure that utcb/frame will work
	      kill_module(modinfo);
	      return;
	    }

           assert((begin <= translated) && (translated < (begin + size)));

            // We've figured out what memory to map. Map a single
            // naturally aligned flexpage to the client.
            const mword page_mask = ((1UL << Utcb::MINSHIFT)-1);
            assert((size >= 0x1000) and ((begin & page_mask) == 0));
            unsigned order = Cpu::maxalign(translated - begin, begin, fault - (translated - begin), size);

            LOG_VERBOSE("s0: [%2u, %02x] %lx %lx+%lx -> order %u: %lx+%lx\n", modinfo->id, pid, translated, begin, size, order,
                              translated & ~((1UL << order)-1), 1UL << order);

            assert(order >= Utcb::MINSHIFT);
            assert((1UL << order) <= size);

            mword aligned_begin = translated & ~((1UL << order)-1);
            const Crd region = Crd(aligned_begin >> Utcb::MINSHIFT, order - Utcb::MINSHIFT, rights);
	    // we can not overmap -> thus remove all rights first if the PTE was present
	    if (utcb->qual[0] & 1) {
              Logging::printf("s0: potential overmap. revoke!\n");
              nova_revoke(region, false);
            }

            utcb->reset();
            *utcb << Utcb::TypedMapCap(region, (fault & ~page_mask));

	    utcb->set_header(0,0);
	    if ((utcb->qual[1] & CLIENT_HIP) == CLIENT_HIP) {
	      unsigned long rest = utcb->add_mappings(reinterpret_cast<unsigned long>(modinfo->hip), 0x1000, CLIENT_HIP | MAP_MAP, DESC_MEM_ALL);
	      assert(!rest); //should ever succeed, one page should ever fit
	    } else {
	      unsigned long rest = utcb->add_mappings(reinterpret_cast<unsigned long>(modinfo->mem), modinfo->physsize, MEM_OFFSET | MAP_MAP, DESC_MEM_ALL);
	      if (rest) {
	        utcb->set_header(0,0);
	        unsigned long offset = (utcb->qual[1] & ~((1UL << Utcb::MINSHIFT) - 1)) - MEM_OFFSET;  //XXX be more clever here, find out which is the biggest junk we can map
	        rest = utcb->add_mappings(reinterpret_cast<unsigned long>(modinfo->mem) + offset, 1UL << Utcb::MINSHIFT, (MEM_OFFSET + offset) | MAP_MAP, DESC_MEM_ALL);
	        assert(!rest); //should ever succeed, one page should ever fit
	      }
	    }
	    LOG_VERBOSE("s0: [%2u, %02x] map %x/%x for %llx err %llx at %x\n",
	        modinfo->id, pid, utcb->head.untyped, utcb->head.typed, utcb->qual[1], utcb->qual[0], utcb->eip);

	    return;
	  }

	  SemaphoreGuard l(_lock_gsi);

	  if (request_pcicfg(modinfo->id, utcb)) return;

	  if (!request_disks(modinfo, utcb) &&
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
                            if (assign_pci_device(NOVA_DEFAULT_PD_CAP + (modinfo->id << CLIENT_PT_SHIFT) + CLIENT_PT_OFFSET, msg->value, msg->len))
                              utcb->msg[0] = 0;
                            else goto fail;
                            //Logging::printf("s0: assign_pci() PD %x bdf %lx vfbdf %lx = %x\n", modinfo->id, msg->value, msg->len, utcb->msg[0]);
			  } else {
			    Logging::printf("s0: [%2u] DMA access denied.\n", modinfo->id);
                            goto fail;
			  }
			  break;
			case MessageHostOp::OP_ATTACH_IRQ:
			  if ((msg->value & 0xff) < _hip->cfg_gsi) {
			    // XXX make sure only one gets it
			    unsigned gsi_cap = _irq_cap_base + (msg->value & 0xff);
			    unsigned res = nova_assign_gsi(gsi_cap, modinfo->cpunr);
			    if (res != NOVA_ESUCCESS) goto fail;
			    Logging::printf("s0: [%2u] gsi %lx granted\n", modinfo->id, msg->value);
			    utcb->set_header(1, 0);
			    utcb->msg[0] = utcb->add_mappings(gsi_cap << Utcb::MINSHIFT, 1 << Utcb::MINSHIFT, MAP_MAP, DESC_CAP_ALL);
			    if (utcb->msg[0]) goto fail;
			  }
			  else {
			    Logging::printf("s0: [%2u] irq request dropped %x nr %x\n", modinfo->id, utcb->msg[2], utcb->msg[2] >> Utcb::MINSHIFT);
			    goto fail;
			  }
			  break;
			case MessageHostOp::OP_ATTACH_MSI:
			  {
                            if (msg->is_hpet) {
                              Logging::printf("s0: Client tried to map HPET MSI. He can do that himself!\n");
                              goto fail;
                            }
			    unsigned cap = attach_msi(msg, modinfo->cpunr);
			    if (!cap) {
                              Logging::printf("s0: [%2u] granting msi to cpu %u failed\n", modinfo->id, modinfo->cpunr);
                              goto fail;
                            }
                            utcb->set_header(1 + sizeof(*msg)/ sizeof(unsigned), 0);
			    unsigned long res = utcb->add_mappings(cap << Utcb::MINSHIFT, 1 << Utcb::MINSHIFT, MAP_MAP, DESC_CAP_ALL);
                            assert(res == 0); // Cannot fail
                            utcb->msg[0] = 0;
			    Logging::printf("s0: [%2u] msi granted to cpu %u\n", modinfo->id, modinfo->cpunr);
			  }
			  break;
			case MessageHostOp::OP_ALLOC_IOIO_REGION:
			  // XXX make sure only one gets it
			  map_self(utcb, (msg->value >> 8) << Utcb::MINSHIFT, 1 << (Utcb::MINSHIFT + msg->value & 0xff), DESC_IO_ALL);
			  utcb->set_header(1, 0);
			  utcb->msg[0] = utcb->add_mappings((msg->value >> 8) << Utcb::MINSHIFT, (1 << (Utcb::MINSHIFT + msg->value & 0xff)), MAP_MAP, DESC_IO_ALL);
			  if (utcb->msg[0]) goto fail;
			  break;
			case MessageHostOp::OP_ALLOC_IOMEM:
			  {
			    // XXX make sure it is not physmem and only one gets it
			    unsigned long addr = msg->value & ~0xffful;
			    char *ptr = map_self(utcb, addr, msg->len, DESC_MEM_ALL);
			    utcb->set_header(1, 0);
			    utcb->msg[0] = utcb->add_mappings(reinterpret_cast<unsigned long>(ptr), msg->len, MAP_MAP, DESC_MEM_ALL);
			    if (utcb->msg[0] == 0)
			      Logging::printf("s0: [%2u] iomem %lx+%lx granted from %p\n", modinfo->id, addr, msg->len, ptr);
			    else {
			      Logging::printf("s0: [%2u] iomem mapping failed.\n", modinfo->id);
			      goto fail;
			    }
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
			case MessageHostOp::OP_WAIT_CHILD:
			case MessageHostOp::OP_CREATE_EC4PT:
			default:
			  // unhandled
			  Logging::printf("s0: [%2u] unknown request (%x,%x,%x) dropped \n", modinfo->id, utcb->msg[0],  utcb->msg[1],  utcb->msg[2]);
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
		    Logging::printf("s0: [%2u] unknown request (%x,%x,%x) dropped \n", modinfo->id, utcb->msg[0],  utcb->msg[1],  utcb->msg[2]);
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
          const char * desc = msg.desc;
          unsigned cpu  = (msg.cpu == ~0U) ? _cpunr[CPUGSI % _numcpus] : msg.cpu;
          LOG_VERBOSE("s0: Attaching to CPU %x (%x %x)\n", cpu, msg.cpu, _cpunr[CPUGSI % _numcpus]);
          assert(cpu < Config::MAX_CPUS);
          unsigned cap = attach_msi(&msg, cpu);
          check1(false, !cap);
          res = attach_irq(msg.msi_gsi, cap, unlocked, cpu, desc);
        }
        break;
      case MessageHostOp::OP_ATTACH_IRQ:
        {
          unsigned gsi = msg.value & 0xff;
          if (_gsi & (1 << gsi)) return true;
           _gsi |=  1 << gsi;
          unsigned irq_cap = _irq_cap_base + gsi;
          unsigned cpu = (msg.cpu == ~0U) ? _cpunr[CPUGSI % _numcpus] : msg.cpu;
          unsigned ret = nova_assign_gsi(irq_cap, cpu);
          if (ret == NOVA_EDEV)
            Logging::printf("s0: assign_gsi returned BAD_DEV. If this is a QEMU VM, try \"qemu -no-kvm-irqchip\".\n");
          check1(false, ret != NOVA_ESUCCESS);
          res = attach_irq(gsi, irq_cap, msg.len, cpu, msg.desc);
        }
        break;
      case MessageHostOp::OP_ALLOC_IOIO_REGION:
        //Logging::printf("s0: ALLOC_IOIO %lx\n", msg.value);
        map_self(myutcb(), (msg.value >> 8) << Utcb::MINSHIFT, 1 << (Utcb::MINSHIFT + msg.value & 0xff), DESC_IO_ALL);
        break;
      case MessageHostOp::OP_ALLOC_IOMEM:
        msg.ptr = map_self(myutcb(), msg.value, msg.len, DESC_MEM_ALL, true);
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
          unsigned ec_cap = create_ec4pt(msg._alloc_service_portal.pt_arg,
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
          const char * name = msg._alloc_service_thread.name ? msg._alloc_service_thread.name : "service";

          AdmissionProtocol::sched sched; //Qpd(prio, 10000)
          if (prio != ~0u) //IDLE TYPE_APERIODIC -> prio=1
            sched.type = prio ? AdmissionProtocol::sched::TYPE_SPORADIC : AdmissionProtocol::sched::TYPE_PERIODIC; //XXX don't guess here

          return (service_admission->alloc_sc(*myutcb(), cap_ec, sched, cpu, this, name) == NOVA_ESUCCESS);
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
        res = assign_pci_device(NOVA_DEFAULT_PD_CAP, msg.value, msg.len);
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
            unsigned ec_cap = create_ec4pt(msg.obj, cpu, msg.excbase, &utcb);
            unsigned pt = alloc_cap();

            if (!ec_cap || !utcb || !pt) return false;
            if (msg.cap)
              // XXX This is broken. It needs to call alloc_cap() of
              // the service object, which we don't have here.
              utcb->head.crd = alloc_cap() << Utcb::MINSHIFT | DESC_CAP_ALL;
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
              unsigned ec_pf = create_ec4pt(msg.obj, cpu, _percpu[cpu].exc_base, &utcb);
              if (!ec_pf || !utcb) return false;
              utcb->head.crd = 0;
              check1(false, nova_create_pt(msg.excbase + 0xe, ec_pf, msg.portal_pf, MTD_GPR_ACDB | MTD_GPR_BSD | MTD_QUAL | MTD_RIP_LEN | MTD_RSP));
            }
            msg.excbase += msg.excinc;
          }
          // XXX transfer the service_cap back
        }
        break;
      case MessageHostOp::OP_WAIT_CHILD: {
	unsigned res = nova_semdown(CLIENT_PT_OFFSET + (msg.value << CLIENT_PT_SHIFT) + ParentProtocol::CAP_PARENT_ID);
	assert(res == 0);
	break;
      }
      case MessageHostOp::OP_CREATE_EC4PT:
	msg._create_ec4pt.ec = create_ec4pt(msg.obj, msg._create_ec4pt.cpu,
                                            _percpu[msg._create_ec4pt.cpu].exc_base,
                                            msg._create_ec4pt.utcb_out, msg._create_ec4pt.ec);
	return msg._create_ec4pt.ec != 0;

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

            if (msg2.type == MessageNetwork::PACKET) {
              modinfo->net_tx         += msg2.len;
              modinfo->net_tx_packets += 1;
            }

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
        if (i != msg.client) {
          _modinfo[i].net_rx         += msg.len;
          _modinfo[i].net_rx_packets += 1;
          bool success = _prod_network[i].produce(msg.buffer, msg.len);
          _modinfo[i].net_rx_drop += success ? 0 : 1;
        }
      }
      return true;
    }

    // Don't pass along MAC queries.
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
	    if (msg2.disknr >= disk_data->disk_count) { msg->error = MessageDisk::DISK_STATUS_DEVICE; utcb->msg[0] = ERESOURCE; utcb->head.typed = 0; return true; }
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
        Logging::panic("s0: [%2u] produce disk (%x) failed\n", client, index);
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
    if (initialized_console) return;

    _mb->bus_console.add(this, receive_static<MessageConsole>);

    // alloc consoles for ourself
    MessageConsole msg1;
    msg1.clientname = 0;
    _mb->bus_console.send(msg1);
    _console_data[0].console = msg1.id;

    // open 3 views
    MessageConsole msg2("sigma0",  _vga + 0x18000, 0x1000, &_vga_regs);
    msg2.id = _console_data[0].console;
    _mb->bus_console.send(msg2);
    msg2.name = "HV";
    msg2.ptr += 0x1000;
    _mb->bus_console.send(msg2);
    msg2.name = "boot";
    msg2.ptr += 0x1000;
    _mb->bus_console.send(msg2);
    switch_view(_mb, 0, _console_data[0].console);

    initialized_console = true;
  }


  void alloc_console(ModuleInfo const * modinfo, const char *cmdline, bool bswitch) {
    unsigned client = modinfo->id;

    // format the tag
    Vprintf::snprintf(_console_data[client].tag, sizeof(_console_data[client].tag), "CPU(%x) MEM(%ld) %s", modinfo->cpunr, modinfo->physsize >> 20, cmdline);

    if (modinfo->type == ModuleInfo::TYPE_ADMISSION) _console_data[client].console = _console_data[0].console;
    else {
      MessageConsole msg1;
      msg1.view = bswitch; //switch view when creating client (!0) ?
      msg1.clientname = _console_data[client].tag;
      msg1.id = client;
      _console_data[client].console = _mb->bus_console.send(msg1) ? msg1.id : ~0UL;
      if (_console_data[client].console == ~0UL) Logging::printf("s0: [%2u] allocation of console failed\n", modinfo->id);
    }
  };

  /**
   * Switch to our view.
   */
  void switch_view(Motherboard *mb, int view, unsigned short consoleid)
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
              && convert_client_ptr(modinfo, msg2.info, sizeof(*msg2.info))))
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
      LOG_VERBOSE("s0: drop input %x at console %x.%x\n", msg.input_data, msg.id, msg.view);
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
        unsigned internal_id;
        if (msg.id == (~0U & ((1 << (sizeof(msg.id) * 8)) - 1) )) {
          msg.res = start_config(myutcb(), msg.cmdline, internal_id, msg.cap_sc_usage, msg.mem, false);
          if (msg.res) {
            Logging::printf("s0: start of config failed, res = %d, config :\n'", msg.res);
            fancy_output(msg.cmdline, 4096);
          } else msg.id = internal_id;
        } else {
          msg.res = start_config(myutcb(), msg.id, internal_id);
          if (msg.res) {
            Logging::printf("s0: start of config failed, res = %d, config id=%d\n", msg.res, msg.id);
            switch_view(_mb, 0, _console_data[0].console);
          }
          else msg.id = internal_id;
        }
        return !msg.res;
      }
    case MessageConsole::TYPE_KILL:
      {
        msg.res = kill_module(get_module(msg.id));
        if (msg.res) Logging::printf("s0: [%2u] killing module failed = %#x\n", msg.id, msg.res);
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
      case 1: //XXX statistics about network interface - drop this if we have service_net and service_disk fully available
        {
          ModuleInfo * modinfo = sigma0->get_module(msg.view); //msg.view contains module id
          if (!modinfo) break;

          msg.net_rx         = modinfo->net_rx;
          msg.net_rx_packets = modinfo->net_rx_packets;
          msg.net_rx_drop    = modinfo->net_rx_drop;
          msg.net_tx         = modinfo->net_tx;
          msg.net_tx_packets = modinfo->net_tx_packets;

          return true;
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
        MessageHwPciConfig *msg = reinterpret_cast<MessageHwPciConfig *>(utcb->msg+1);
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
   * Generate a pseudo-random but persistent hostmac by hashing all
   * PCI device IDs, their BDFs and the serial numbers of devices.
   */
  unsigned generate_hostmac() {
    HostPci pci(_mb->bus_hwpcicfg, _mb->bus_hostop);

    struct {
      unsigned state;
      void hash(unsigned value) {  state = ((state << 1) | (state >> 31)) ^ value; }
    } x = { 0 };

    for (unsigned bus = 0; bus < 256; bus++) {
      for (unsigned dev=0; dev < 32; dev++) {
        unsigned char maxfunc = 1;
        for (unsigned func=0; func < maxfunc; func++) {
          unsigned short bdf =  (bus << 8) | (dev << 3) | func;
          unsigned devid = pci.conf_read(bdf, 0);
          if (devid == ~0UL) continue;
          if (maxfunc == 1 && pci.conf_read(bdf, 3) & 0x800000) maxfunc = 8;

          x.hash(bdf); x.hash(devid);

          // find device serial number
          unsigned offset = pci.find_extended_cap(bdf, 0x0003);
          if (offset) {
            x.hash(pci.conf_read(bdf, offset + 1));
            x.hash(pci.conf_read(bdf, offset + 2));
          }
        }
      }
    }
    // Make sure not to return zero.
    if (x.state == 0) x.state = 0xCAFEBABE;
    return x.state;
  }


  /**
   * Return a MAC by adding the MAC-prefix, the host-MAC and a client
   * specific number.
   */
  unsigned long long get_mac(unsigned clientnr) {
    if (!mac_host) mac_host = generate_hostmac();
    return (static_cast<unsigned long long>(mac_prefix) << 16) + (static_cast<unsigned long long>(mac_host) << 8) + clientnr;
  }


  void run(Utcb *utcb, Hip *hip) NORETURN
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
    if (!initialized_s0_tasks)                     Logging::panic("s0: embedded s0 services not running - you forget the boot_s0_services option\n");

    Logging::printf("s0:\t=> INIT done <=\n\n");

    // We use insider knowledge here to figure out NUM_CPU of the hypervisor.
    if (hip->gsi_cap_base() != hip->cpu_desc_count())
      Logging::printf("s0: NUL is built for %u CPU%s, but the hypervisor supports %u.\n",
                      unsigned(Config::MAX_CPUS), (Config::MAX_CPUS == 1) ? "" : "s",
                      hip->gsi_cap_base());

    // block ourself since we have finished initialization
    block_forever();
  }

  static void start(phy_cpu_no cpu, Utcb *utcb) asm ("start") REGPARM(3) NORETURN;
  Sigma0() :  _numcpus(0), _modinfo(), _gsi(0), _pcidirect()  {}
};

PARAM_HANDLER(boot_s0_services, "Start embedded services running as separate task which are essential to sigma0, e.g. admission service.") {
  const char * cmdline = reinterpret_cast<char *>(mb.hip()->get_mod(0)->aux);

  // Take care that hostvga and init_consoles start before
  // boot_s0_services so that allocation of consoles can succeed and
  // we don't miss output.
  // XXX This should work on the expanded command line...

  const char *hostvga = NULL; size_t hvlength = 0;
  const char *s0_serv = NULL; const char *current;
  size_t length;
  Utcb * utcb = BaseProgram::myutcb();

  while ((current = mb.next_arg(cmdline, length))) {
    if (strstr(current, "boot_s0_services") == current)
      s0_serv = current;

    if (strstr(current, "hostvga") == current) {
      hostvga = current; hvlength = length;
    }
  }

  if ((not s0_serv and hostvga) or (s0_serv < hostvga)) {
    Logging::printf("Start hostvga now to avoid dependency problems.\n");
    // hostvga is configured to start after boot_s0_services. Do
    // that now instead and don't start it twice.
    mb.parse_args(hostvga, hvlength);
    // XXX This is evil...
    memset(const_cast<char *>(hostvga), ' ', hvlength);
  }

  if (hostvga)
    sigma0->init_console();

  if (sigma0->boot_s0_services(utcb)) Logging::panic("s0: could not start embedded s0 services\n");

  unsigned res;
  if (res = sigma0->service_admission->push_scs(*utcb, NOVA_DEFAULT_PD_CAP + 2, utcb->head.nul_cpunr))
    Logging::panic("s0: could not start admission service - error %x\n", res);
  sigma0->service_admission->set_name(*utcb, "sigma0");

  sigma0->initialized_s0_tasks = true;
}

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

void Sigma0::start(phy_cpu_no cpu, Utcb *utcb) {
  extern unsigned __nova_api_version;
  assert(Global::hip.api_ver == __nova_api_version);
  assert(Global::hip.cfg_exc + 0 ==  NOVA_DEFAULT_PD_CAP);
  static Sigma0 s0;
  sigma0 = &s0;
  utcb->head.nul_cpunr = cpu;
  s0.run(utcb, &Global::hip);
}


void  do_exit(const char *msg)
{
  if (consolesem)  consolesem->up();
  Logging::printf("s0:__exit(%s)\n", msg);
  if (global_mb && sigma0) sigma0->switch_view(global_mb, 0, sigma0->_console_data[0].console);

  while (1)
    asm volatile ("ud2a" : : "a"(msg));
}

Semaphore Sigma0::_lock_mem;
//  LocalWords:  utcb
