/*
 * Main code and static vars of vancouver.nova.
 *
 * Copyright (C) 2007-2010, Bernhard Kauer <bk@vmmon.org>
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
#ifndef  VM_FUNC
#include "host/keyboard.h"
#include "nul/motherboard.h"
#include "nul/program.h"
#include "nul/vcpu.h"
#include "sigma0/console.h"
#include "nul/service_timer.h"
#include "nul/service_fs.h"

/**
 * Layout of the capability space.
 */
enum Cap_space_layout
  {
    PT_IRQ       = 0x20,
    PT_VMX       = 0x100,
    PT_SVM       = 0x200
  };

/****************************************************/
/* Static Variables                                 */
/****************************************************/

Motherboard   *_mb;
unsigned       _debug;
const void *   _forward_pkt;
Semaphore      _lock;
long           _consolelock;
TimeoutList<32, void>_timeouts;
unsigned       _shared_sem[256];
unsigned       _keyboard_modifier = KBFLAG_RWIN;
bool           _dpci;
unsigned       _ncpu=1;
PARAM(kbmodifier,
      _keyboard_modifier = argv[0];
      ,
      "kbmodifier:value - change the kbmodifier. Default: RWIN.",
      "Example: 'kbmodifier:0x40000' uses LWIN as modifier.",
      "See keyboard.h for definitions.")
PARAM(panic, if (argv[0]) Logging::panic("%s", __func__); ,
      "panic - panic the system at creation time" )
PARAM_ALIAS(PC_PS2, "an alias to create an PS2 compatible PC",
	    " mem:0,0xa0000 mem:0x100000 ioio nullio:0x80 pic:0x20,,0x4d0 pic:0xa0,2,0x4d1"
	    " pit:0x40,0 scp:0x92,0x61 kbc:0x60,1,12 keyb:0,0x10000 mouse:1,0x10001 rtc:0x70,8"
	    " serial:0x3f8,0x4,0x4711 hostsink:0x4712,80 vga:0x03c0"
	    " vbios_disk vbios_keyboard vbios_mem vbios_time vbios_reset vbios_multiboot"
	    " msi ioapic pcihostbridge:0,0x10,0xcf8,0xe0000000 pmtimer:0x8000 vcpus")
PARAM(ncpu, _ncpu = argv[0];, "ncpu - change the number of vcpus that are created" );
PARAM(vcpus,
      for (unsigned count = 0; count < _ncpu; count++)
	mb.parse_args("vcpu halifax vbios lapic");
      ,
      " instantiate the vcpus defined with 'ncpu'");

/****************************************************/
/* Vancouver class                                  */
/****************************************************/


class Vancouver : public NovaProgram, public ProgramConsole, public StaticReceiver<Vancouver>
{

  unsigned long  _physmem;
  unsigned long  _original_physsize;
  unsigned long  _physsize;
  unsigned long  _iomem_start;
  static TimerProtocol   *timer_service;
  #define VANCOUVER_CONFIG_SEPARATOR "||"

#define PT_FUNC(NAME)  static void  NAME(unsigned pid, Vancouver *tls, Utcb *utcb) __attribute__((regparm(1)))
#define VM_FUNC(NR, NAME, INPUT, CODE)					             \
  static void  NAME(unsigned pid, VCpu *tls, Utcb *utcb) __attribute__((regparm(1))) \
  {  CODE;								\
    asmlinkage_protect("g"(tls), "g"(utcb));				\
  }
  #include "vancouver.cc"

  // the portal functions follow

  PT_FUNC(got_exception) __attribute__((noreturn))
  {
    // make sure we can print something
    _consolelock = ~0;
    Logging::printf("%s() #%x ",  __func__, pid);
    Logging::printf("rip %x rsp %x  %x:%x %x:%x %x", utcb->eip, utcb->esp,
		    utcb->edx, utcb->eax,
		    utcb->edi, utcb->esi,
		    utcb->ecx);
    tls->block_forever();
  }


  PT_FUNC(do_gsi_pf)
  {

    Logging::printf("%s eip %x qual %llx\n", __func__, utcb->eip, utcb->qual[1]);
    request_mapping(0, ~0ul, utcb->qual[1]);
    utcb->set_header(0, 0);
    asmlinkage_protect("g"(tls), "g"(utcb));
  }

  PT_FUNC(do_gsi_boot)
  {
    utcb->eip = reinterpret_cast<unsigned *>(utcb->esp)[0];
    Logging::printf("%s eip %x esp %x\n", __func__, utcb->eip, utcb->esp);
    asmlinkage_protect("g"(tls), "g"(utcb));
  }


  PT_FUNC(do_gsi) __attribute__((noreturn))
  {
    unsigned res;
    bool shared = utcb->msg[1] >> 8;
    Logging::printf("%s(%x, %x, %x) %p\n", __func__, utcb->msg[0], utcb->msg[1], utcb->msg[2], utcb);
    while (1) {
      if ((res = nova_semdown(utcb->msg[0])))
	Logging::panic("%s(%x) request failed with %x\n", __func__, utcb->msg[0], res);
      {
	SemaphoreGuard l(_lock);
	MessageIrq msg(shared ? MessageIrq::ASSERT_NOTIFY : MessageIrq::ASSERT_IRQ, utcb->msg[1] & 0xff);
	_mb->bus_hostirq.send(msg);
      }
      if (shared)  nova_semdown(utcb->msg[2]);
    }
  }


  PT_FUNC(do_stdin) __attribute__((noreturn))
  {
    KernelSemaphore *sem = new KernelSemaphore(alloc_cap(), true);
    StdinConsumer *stdinconsumer = new StdinConsumer();
    assert(stdinconsumer);
    Sigma0Base::request_stdin(utcb, stdinconsumer, sem->sm());

    while (1) {
      sem->downmulti();
      while (stdinconsumer->has_data()) {
      MessageInput *msg = stdinconsumer->get_buffer();
      switch ((msg->data & ~KBFLAG_NUM) ^ _keyboard_modifier)
	{
	case KBFLAG_EXTEND0 | 0x7c: // printscr
	  {
	    Logging::printf("DEBUG key\n");
	    // we send an empty event
	    CpuEvent msg(VCpu::EVENT_DEBUG);
	    for (VCpu *vcpu = _mb->last_vcpu; vcpu; vcpu=vcpu->get_last())
	      vcpu->bus_event.send(msg);
	  }
	  break;
	case KBCODE_SCROLL: // scroll lock
	  Logging::printf("revoke all memory\n");
	  extern char __freemem;
	  revoke_all_mem(&__freemem, 0x30000000, DESC_MEM_ALL, true);
	  break;
	case KBFLAG_EXTEND1 | KBFLAG_RELEASE | 0x77: // break
	  _debug = true;
	  _mb->dump_counters();
	  nova_syscall(15, 0, 0, 0, 0);
	  break;
	case KBCODE_HOME: // reset VM
	  {
	    SemaphoreGuard l(_lock);
	    MessageLegacy msg2(MessageLegacy::RESET, 0);
	    _mb->bus_legacy.send_fifo(msg2);
	  }
	  break;
	case KBFLAG_LCTRL | KBFLAG_RWIN |  KBFLAG_LWIN | 0x5:
	  _mb->dump_counters();
	  break;
	default:
	  break;
	}

	SemaphoreGuard l(_lock);
	_mb->bus_input.send(*msg);
	stdinconsumer->free_buffer();
      }
    }
  }

  PT_FUNC(do_disk) __attribute__((noreturn))
  {
    KernelSemaphore *sem = new KernelSemaphore(alloc_cap(), true);
    DiskConsumer *diskconsumer = new DiskConsumer();
    assert(diskconsumer);
    Sigma0Base::request_disks_attach(utcb, diskconsumer, sem->sm());
    while (1) {
      sem->downmulti();
      while (diskconsumer->has_data()) {
        MessageDiskCommit *msg = diskconsumer->get_buffer();
        SemaphoreGuard l(_lock);
        _mb->bus_diskcommit.send(*msg);
        diskconsumer->free_buffer();
      }
    }
  }

  PT_FUNC(do_timer) __attribute__((noreturn))
  {
    KernelSemaphore timer_sem = KernelSemaphore(timer_service->get_notify_sm());

    while (1) {
      timer_sem.down();
      {
        COUNTER_INC("timer");
        SemaphoreGuard l(_lock);
        timeout_trigger();
      }
    }
  }

  PT_FUNC(do_network) __attribute__((noreturn))
  {
    KernelSemaphore *sem = new KernelSemaphore(alloc_cap(), true);
    NetworkConsumer *network_consumer = new NetworkConsumer();
    Sigma0Base::request_network_attach(utcb, network_consumer, sem->sm());
    while (1) {
      sem->downmulti();
      while (network_consumer->has_data()) {
        unsigned char *buf;
        unsigned size = network_consumer->get_buffer(buf);

        MessageNetwork msg(buf, size, 0);
        assert(!_forward_pkt);
        _forward_pkt = msg.buffer;
        {
          SemaphoreGuard l(_lock);
          _mb->bus_network.send(msg);
        }
        _forward_pkt = 0;
        network_consumer->free_buffer();
      }
    }
  }

  PT_FUNC(do_vnet) __attribute__((noreturn))
  {
    unsigned sm = alloc_cap();
    if (nova_create_sm(sm)) Logging::panic("VNET semaphore creation failed.\n");
    Sigma0Base::request_vnet_attach(utcb, sm);

    while (1) {
      if (nova_semdownmulti(sm)) Logging::panic("VNET b0rken?\n");
      {
	SemaphoreGuard l(_lock);

	MessageVirtualNetPing p(0);
	_mb->bus_vnetping.send(p);
      }
    }
  }


  static void force_invalid_gueststate_amd(Utcb *utcb)
  {
    utcb->ctrl[1] = 0;
    utcb->mtd = MTD_CTRL;
  };

  static void force_invalid_gueststate_intel(Utcb *utcb)
  {
    assert(utcb->mtd & MTD_RFLAGS);
    utcb->efl &= ~2;
    utcb->mtd = MTD_RFLAGS;
  };



  void create_devices(Utcb *utcb, Hip *hip, char *args)
  {
    _timeouts.init();

    _mb = new Motherboard(new Clock(hip->freq_tsc*1000), hip);
    _mb->bus_hostop.add  (this, receive_static<MessageHostOp>);
    _mb->bus_console.add (this, receive_static<MessageConsole>);
    _mb->bus_disk.add    (this, receive_static<MessageDisk>);
    _mb->bus_timer.add   (this, receive_static<MessageTimer>);
    _mb->bus_time.add    (this, receive_static<MessageTime>);
    _mb->bus_network.add (this, receive_static<MessageNetwork>);
    _mb->bus_vnet.add    (this, receive_static<MessageVirtualNet>);
    _mb->bus_hwpcicfg.add(this, receive_static<MessagePciConfig>);
    _mb->bus_acpi.add    (this, receive_static<MessageAcpi>);

    timer_service = new TimerProtocol(alloc_cap(TimerProtocol::CAP_NUM));
    TimerProtocol::MessageTimer msg(_mb->clock()->abstime(0, 1000));

    unsigned res;
    if ((res = timer_service->timer(*utcb, msg)))
      Logging::panic("Timer service unreachable (error: %x).\n", res);

    _mb->parse_args(args, VANCOUVER_CONFIG_SEPARATOR);

    _mb->bus_hwioin.debug_dump();
  }


  unsigned create_irq_thread(unsigned hostirq, unsigned irq_cap, void __attribute__((regparm(1))) (*func)(unsigned, Vancouver *, Utcb *))
  {
    //Logging::printf("%s %x\n", __PRETTY_FUNCTION__, hostirq);
    Utcb *utcb;
    unsigned cap_ec = create_ec_helper(this, myutcb()->head.nul_cpunr, PT_IRQ, &utcb, reinterpret_cast<void *>(func));
    check1(~1u, nova_create_sm(_shared_sem[hostirq & 0xff] = alloc_cap()));
    utcb->head.untyped = 3;
    utcb->head.typed = 0;
    utcb->msg[0] = irq_cap;
    utcb->msg[1] = hostirq;
    utcb->msg[2] = _shared_sem[hostirq & 0xff];

    // XXX How many time should an IRQ thread get?
    check1(~3u, nova_create_sc(alloc_cap(), cap_ec, Qpd(2, 10000)));
    return cap_ec;
  }


  unsigned init_caps()
  {
    _lock = Semaphore(alloc_cap());
    check1(1, nova_create_sm(_lock.sm()));

    Semaphore *sem = new Semaphore(alloc_cap());
    check1(2, nova_create_sm(sem->sm()));
    sem->up();
    _console_data.lock.sem = sem;

    // create exception EC
    unsigned cap_ex = create_ec_helper(this,  myutcb()->head.nul_cpunr);
    // create portals for exceptions
    for (unsigned i=0; i < 32; i++)
      if ((i != 14) && (i != 30)) check1(3, nova_create_pt(i, cap_ex, reinterpret_cast<unsigned long>(got_exception), MTD_ALL));
    // create the gsi boot portal
    check1(4, nova_create_pt(PT_IRQ + 14, cap_ex, reinterpret_cast<unsigned long>(do_gsi_pf),    MTD_RIP_LEN | MTD_QUAL));
    check1(5, nova_create_pt(PT_IRQ + 30, cap_ex, reinterpret_cast<unsigned long>(do_gsi_boot),  MTD_RSP | MTD_RIP_LEN));
    return 0;
  }

  unsigned create_vcpu(VCpu *vcpu, bool use_svm, unsigned cpunr)
  {
    // create worker
    unsigned cap_worker = create_ec_helper(vcpu, cpunr);

    // create portals for VCPU faults
#undef VM_FUNC
#define VM_FUNC(NR, NAME, INPUT, CODE) {NR, NAME, INPUT},
    struct vm_caps {
      unsigned nr;
      void __attribute__((regparm(1))) (*func)(unsigned, VCpu *, Utcb *);
      unsigned mtd;
    } vm_caps[] = {
#include "vancouver.cc"
    };
    unsigned cap_start = alloc_cap(0x100);
    for (unsigned i=0; i < sizeof(vm_caps)/sizeof(vm_caps[0]); i++) {
      if (use_svm == (vm_caps[i].nr < PT_SVM)) continue;
      check1(0, nova_create_pt(cap_start + (vm_caps[i].nr & 0xff), cap_worker, reinterpret_cast<unsigned long>(vm_caps[i].func), vm_caps[i].mtd));
    }

    Logging::printf("\tcreate VCPU\n");
    unsigned cap_block = alloc_cap(3);
    if (nova_create_sm(cap_block))
      Logging::panic("could not create blocking semaphore\n");
    if (nova_create_ec(cap_block + 1, 0, 0, cpunr, cap_start, false)
	|| nova_create_sc(cap_block + 2, cap_block + 1, Qpd(1, 10000)))
      Logging::panic("creating a VCPU failed - does your CPU support VMX/SVM?");
    return cap_block;
  }


  static void skip_instruction(CpuMessage &msg) {

    // advance EIP
    assert(msg.mtr_in & MTD_RIP_LEN);
    msg.cpu->eip += msg.cpu->inst_len;
    msg.mtr_out |= MTD_RIP_LEN;

    // cancel sti and mov-ss blocking as we emulated an instruction
    assert(msg.mtr_in & MTD_STATE);
    if (msg.cpu->intr_state & 3) {
      msg.cpu->intr_state &= ~3;
      msg.mtr_out |= MTD_STATE;
    }
  }


  static void handle_io(VCpu *vcpu, Utcb *utcb, bool is_in, unsigned io_order, unsigned port) {

    assert(vcpu);
    CpuMessage msg(is_in, static_cast<CpuState *>(utcb), io_order, port, &utcb->eax, utcb->mtd);
    skip_instruction(msg);
    SemaphoreGuard l(_lock);
    if (!vcpu->executor.send(msg, true))
      Logging::panic("nobody to excute %s at %x:%x\n", __func__, msg.cpu->cs.sel, msg.cpu->eip);
  }



  static void handle_vcpu(unsigned pid, bool skip, CpuMessage::Type type, VCpu *vcpu, Utcb *utcb) {

    assert(vcpu);
    CpuMessage msg(type, static_cast<CpuState *>(utcb), utcb->mtd);
    if (skip) skip_instruction(msg);

    SemaphoreGuard l(_lock);

    /**
     * Send the message to the VCpu.
     */
    if (!vcpu->executor.send(msg, true))
      Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, msg.cpu->cs.sel, msg.cpu->eip, pid);

    /**
     * Check whether we should inject something...
     */
    if (msg.mtr_in & MTD_INJ && msg.type != CpuMessage::TYPE_CHECK_IRQ) {
      msg.type = CpuMessage::TYPE_CHECK_IRQ;
      if (!vcpu->executor.send(msg, true))
	Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, msg.cpu->cs.sel, msg.cpu->eip, pid);
    }

    /**
     * If the IRQ injection is performed, recalc the IRQ window.
     */
    if (msg.mtr_out & MTD_INJ) {
      msg.type = CpuMessage::TYPE_CALC_IRQWINDOW;
      if (!vcpu->executor.send(msg, true))
	Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, msg.cpu->cs.sel, msg.cpu->eip, pid);
    }
    msg.cpu->mtd = msg.mtr_out;
  }


  static bool map_memory_helper(VCpu *vcpu, Utcb *utcb, bool need_unmap)
  {

    MessageMemRegion msg(utcb->qual[1] >> 12);

    // XXX use a push model on _startup instead
    // do we have not mapped physram yet?
    if (_mb->bus_memregion.send(msg, true) && msg.ptr) {

      Crd own = request_mapping(msg.ptr, msg.count << 12, utcb->qual[1] - (msg.start_page << 12));

      if (need_unmap) revoke_all_mem(reinterpret_cast<void *>(own.base()), own.size(), DESC_MEM_ALL, false);

      utcb->mtd = 0;
      add_mappings(utcb, own.base(), own.size(), (msg.start_page << 12) + (own.base() - reinterpret_cast<unsigned long>(msg.ptr)) | MAP_EPT | (_dpci ? MAP_DPT : 0), own.attr());

      // EPT violation during IDT vectoring?
      if (utcb->inj_info & 0x80000000) {
	utcb->mtd |= MTD_INJ;
	CpuMessage msg(CpuMessage::TYPE_CALC_IRQWINDOW, static_cast<CpuState *>(utcb), utcb->mtd);
	msg.mtr_out = MTD_INJ;
	if (!vcpu->executor.send(msg, true))
	  Logging::panic("nobody to execute %s at %x:%x\n", __func__, utcb->cs.sel, utcb->eip);
      }

      return true;
    }
    return false;
  }

public:
  bool receive(CpuMessage &msg) {
    if (msg.type != CpuMessage::TYPE_CPUID) return false;

    // XXX locking?
    // XXX use the reserved CPUID regions
    switch (msg.cpuid_index) {
    case 0x40000020:
      // NOVA debug leaf
      nova_syscall(15, msg.cpu->ebx, 0, 0, 0);
      break;
    case 0x40000021:
      // Vancouver debug leaf
      _mb->dump_counters();
      break;
    case 0x40000022:
      {
	// time leaf
	unsigned long long tsc = Cpu::rdtsc();
	msg.cpu->eax = tsc;
	msg.cpu->edx = tsc >> 32;
	msg.cpu->ecx = _hip->freq_tsc;
      }
      break;
    default:
      /*
       * We have to return true here, to make handle_vcpu happy.
       * The values are already set in VCpu.
       */
      return true;
    }
    return true;
  }


  bool  receive(MessageHostOp &msg)
  {
    bool res = true;
    switch (msg.type)
      {
      case MessageHostOp::OP_ALLOC_IOIO_REGION:
	{
	  myutcb()->head.crd = Crd(msg.value >> 8, msg.value & 0xff, DESC_IO_ALL).value();
	  res = Sigma0Base::hostop(msg);
	  Logging::printf("alloc ioio region %lx %s\n", msg.value, res ? "done" :  "failed");
	}
	break;
      case MessageHostOp::OP_ALLOC_IOMEM:
	{
	  _iomem_start = (_iomem_start + msg.len - 1) & ~(msg.len-1);
	  myutcb()->head.crd = Crd(_iomem_start >> 12, Cpu::bsr(msg.len) - 12, 1).value();
	  res = Sigma0Base::hostop(msg);
	  if (res) {
	    msg.ptr = reinterpret_cast<char *>(_iomem_start);
	    _iomem_start += msg.len;
	  }
	}
	break;
      case MessageHostOp::OP_GUEST_MEM:
	if (msg.value >= _physsize)
	  msg.value = 0;
	else
	  {
	    extern char __freemem;
	    msg.len    = _physsize - msg.value;
	    msg.ptr    = &__freemem + msg.value;
	  }
	break;
      case MessageHostOp::OP_ALLOC_FROM_GUEST:
	assert((msg.value & 0xFFF) == 0);
	if (msg.value <= _physsize)
	  {
	    _physsize -= msg.value;
	    msg.phys =  _physsize;
            Logging::printf("Allocating from guest %08lx+%lx\n",
                            _physsize, msg.value);
	  }
	else
	  res = false;
	break;
      case MessageHostOp::OP_NOTIFY_IRQ:
	nova_semup(_shared_sem[msg.value & 0xff]);
	res = true;
	break;
      case MessageHostOp::OP_ASSIGN_PCI:
	res = !Sigma0Base::hostop(msg);
	_dpci |= res;
	Logging::printf("%s\n",_dpci ? "DPCI device assigned" : "DPCI failed");
	break;
      case MessageHostOp::OP_GET_MODULE:
        {
          char *args = reinterpret_cast<char *>(_hip->get_mod(0)->aux);
          char *cmdline = args, *name, *end;
          unsigned cmdlen, namelen, num = msg.module;

          if (!cmdline) return false;
          while (num-- && cmdline) cmdline = strstr(++cmdline, VANCOUVER_CONFIG_SEPARATOR);
          if (num != ~0U || !cmdline) return false;
          cmdline +=2;

          cmdline += strspn(cmdline, " \t\r\n\f"); //skip characters
          name = strstr(cmdline, "://");
          if (!name) return false;
          name += 3;
          if (cmdline + strcspn(cmdline, " \t\r\n\f") < name) return false; //don't allow some characters in fs name
          if (msg.size < (0UL + (name - cmdline))) return false;

          namelen = strcspn(name, " \t\r\n\f");
          if (!namelen) return false;
          end  = strstr(cmdline, VANCOUVER_CONFIG_SEPARATOR);
          if (end && name > end) return false; //don't use 'xxx://' of next entry accidentally
          if (end) cmdlen = end - cmdline + 1;
          else cmdlen = strlen(cmdline) + 1;

          //lookup file
          memcpy(msg.start, "fs/", 3);
          memcpy(msg.start + 3, cmdline, name - cmdline);
          msg.start[name - cmdline] = 0;

          unsigned cap_base;
          FsProtocol::dirent fileinfo;
          FsProtocol fs_obj(cap_base = alloc_cap(FsProtocol::CAP_NUM), msg.start);
          if (fs_obj.get_file_info(*myutcb(), fileinfo, name, namelen) || msg.size < fileinfo.size) {
            Logging::printf("FAILED: loading file ...%10s\n", namelen >= 10 ? name + namelen - 10 : "");
            return false;
          }
          res = fs_obj.get_file_copy(*myutcb(), msg.start, fileinfo.size, name, namelen);
          //Note: msg.start used as service name in fs_obj is now overwritten
          fs_obj.close(*myutcb());
          dealloc_cap(cap_base, FsProtocol::CAP_NUM);
          if (res) return false;
          
          //align the end of the module to get the cmdline on a new page.
          char *s = reinterpret_cast<char *>((reinterpret_cast<unsigned long>(msg.start) + fileinfo.size + 0xffful) & ~0xffful);
          memcpy(s, cmdline, cmdlen - 1);
          s[cmdlen - 1] = 0;

          //build response 
          msg.size    = fileinfo.size;
          msg.cmdline = s;
          msg.cmdlen  = cmdlen;

          res = true;
        }
        break;
      case MessageHostOp::OP_GET_MAC:
	res = !Sigma0Base::hostop(msg);
	break;
      case MessageHostOp::OP_ATTACH_MSI:
      case MessageHostOp::OP_ATTACH_IRQ:
	{
	  unsigned irq_cap = alloc_cap();
	  myutcb()->head.crd = Crd(irq_cap, 0, DESC_CAP_ALL).value();
	  res  = !Sigma0Base::hostop(msg);
	  create_irq_thread(msg.type == MessageHostOp::OP_ATTACH_IRQ ? msg.value : msg.msi_gsi, irq_cap, do_gsi);
	}
	break;
      case MessageHostOp::OP_VCPU_CREATE_BACKEND:
	msg.value = create_vcpu(msg.vcpu, _hip->has_svm(), myutcb()->head.nul_cpunr);

	// handle cpuid overrides
	msg.vcpu->executor.add(this, receive_static<CpuMessage>);
	break;
      case MessageHostOp::OP_VCPU_BLOCK:
	_lock.up();
	nova_semdown(msg.value);
	_lock.down();
	break;
      case MessageHostOp::OP_VCPU_RELEASE:
	if (msg.len)  nova_semup(msg.value);
	nova_recall(msg.value + 1);
	break;
      case MessageHostOp::OP_ALLOC_SEMAPHORE:
	msg.value = alloc_cap();
	if (nova_create_sm(msg.value) != 0) Logging::panic("??");
	break;
      case MessageHostOp::OP_ALLOC_SERVICE_THREAD:
	{
	  unsigned ec_cap = create_ec_helper(msg.obj, myutcb()->head.nul_cpunr, PT_IRQ, 0, msg.ptr);
	  // XXX Priority?
	  return !nova_create_sc(alloc_cap(), ec_cap, Qpd(2, 10000));
	}
	break;
      case MessageHostOp::OP_VIRT_TO_PHYS:
      case MessageHostOp::OP_REGISTER_SERVICE:
      default:
	Logging::panic("%s - unimplemented operation %x", __PRETTY_FUNCTION__, msg.type);
      }
      return res;
  }


  bool  receive(MessageDisk &msg)    {
    if (msg.type == MessageDisk::DISK_READ || msg.type == MessageDisk::DISK_WRITE) {
      msg.physsize = _physsize;
      msg.physoffset = _physmem;
    }
    return !Sigma0Base::disk(msg);
  }


  bool  receive(MessageNetwork &msg)
  {
    if (_forward_pkt == msg.buffer) return false;
    Sigma0Base::network(msg);
    return true;
  }

  bool receive(MessageVirtualNet &msg)
  {
    // We need access to memory allocated via ALLOC_FROM_GUEST. That's
    // why we use _original_physsize here.
    msg.physsize   = _original_physsize;
    msg.physoffset = _physmem;
    return Sigma0Base::vnetop(msg);
  }

  bool receive(MessageConsole &msg)    { return !Sigma0Base::console(msg); }
  bool receive(MessagePciConfig &msg)  { return !Sigma0Base::pcicfg(msg);  }
  bool receive(MessageAcpi      &msg)  { return !Sigma0Base::acpi(msg);    }

  /**
   * update timeout in sigma0
   */
  static void timeout_request() {
    if (_timeouts.timeout() != ~0ull) {
      TimerProtocol::MessageTimer msg2(_timeouts.timeout());
      unsigned res = timer_service->timer(*myutcb(), msg2);
      assert(!res);
    }
  }


  static void timeout_trigger() {
    timevalue now = _mb->clock()->time();

    // trigger all timeouts that are due
    unsigned nr;
    while ((nr = _timeouts.trigger(now))) {
      MessageTimeout msg(nr, _timeouts.timeout());
      _timeouts.cancel(nr);
      _mb->bus_timeout.send(msg);
    }
    // request a new timeout upstream
    timeout_request();
  }


  bool  receive(MessageTimer &msg)
  {
    COUNTER_INC("requestTO");
    switch (msg.type)
      {
      case MessageTimer::TIMER_NEW:
	msg.nr = _timeouts.alloc();
	return true;
      case MessageTimer::TIMER_REQUEST_TIMEOUT:
	_timeouts.request(msg.nr, msg.abstime);
	timeout_request();
	break;
      default:
	return false;
      }
    return true;
  }

  bool  receive(MessageTime &msg) {
    TimerProtocol::MessageTime _msg;
    _msg.wallclocktime = msg.wallclocktime;
    _msg.timestamp = msg.timestamp;
    bool res = !timer_service->time(*myutcb(), _msg);
    msg.wallclocktime = _msg.wallclocktime;
    msg.timestamp = _msg.timestamp;
    return res;
  }

public:
  void __attribute__((noreturn)) run(Utcb *utcb, Hip *hip)
  {
    assert(hip);
    char *args = reinterpret_cast<char *>(hip->get_mod(0)->aux);
    console_init("VMM");
    unsigned res;
    if ((res = init(hip))) Logging::panic("init failed with %x", res);

    Logging::printf("Vancouver: hip %p utcb %p args '%s'\n", hip, utcb, args);
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_NUM));

    extern char __freemem;
    _physmem = reinterpret_cast<unsigned long>(&__freemem);
    _original_physsize = 0;
    // get physsize
    for (int i=0; i < (hip->length - hip->mem_offs) / hip->mem_size; i++) {
	Hip_mem *hmem = reinterpret_cast<Hip_mem *>(reinterpret_cast<char *>(hip) + hip->mem_offs) + i;
	if (hmem->type == 1 && hmem->addr <= _physmem) {
	  _original_physsize = hmem->size - (_physmem - hmem->addr);
	  _iomem_start = hmem->addr + hmem->size;
	  break;
	}
    }
    _physsize = _original_physsize;

    if (init_caps())
      Logging::panic("init_caps() failed\n");

    create_devices(utcb, hip, args);

    // create backend connections
    create_irq_thread(~0u, 0, do_stdin);
    create_irq_thread(~0u, 0, do_disk);
    create_irq_thread(~0u, 0, do_timer);
    create_irq_thread(~0u, 0, do_network);
    create_irq_thread(~0u, 0, do_vnet);

    // init VCPUs
    for (VCpu *vcpu = _mb->last_vcpu; vcpu; vcpu=vcpu->get_last()) {

      // init CPU strings
      const char *short_name = "NOVA microHV";
      vcpu->set_cpuid(0, 1, reinterpret_cast<const unsigned *>(short_name)[0]);
      vcpu->set_cpuid(0, 3, reinterpret_cast<const unsigned *>(short_name)[1]);
      vcpu->set_cpuid(0, 2, reinterpret_cast<const unsigned *>(short_name)[2]);
      const char *long_name = "Vancouver VMM proudly presents this VirtualCPU. ";
      for (unsigned i=0; i<12; i++)
	vcpu->set_cpuid(0x80000002 + (i / 4), i % 4, reinterpret_cast<const unsigned *>(long_name)[i]);

      // propagate feature flags from the host
      unsigned ebx_1=0, ecx_1=0, edx_1=0;
      Cpu::cpuid(1, ebx_1, ecx_1, edx_1);
      vcpu->set_cpuid(1, 1, ebx_1 & 0xff00, 0xff00ff00); // clflush size
      vcpu->set_cpuid(1, 2, ecx_1, 0x00000201); // +SSE3,+SSSE3
      vcpu->set_cpuid(1, 3, edx_1, 0x0f88a9bf | (1 << 28)); // -PAE,-PSE36, -MTRR,+MMX,+SSE,+SSE2,+CLFLUSH,+SEP
    }

    Logging::printf("RESET device state\n");
    MessageLegacy msg2(MessageLegacy::RESET, 0);
    _mb->bus_legacy.send_fifo(msg2);

    _lock.up();
    Logging::printf("INIT done\n");

    // block ourself since we have finished initialization
    block_forever();
  }


  static void  exit(const char *value)
  {
    // switch to our view
    MessageConsole msg;
    msg.type = MessageConsole::TYPE_SWITCH_VIEW;
    msg.view = 0;
    Sigma0Base::console(msg);

    Logging::printf("%s() %s\n", __func__, value);
  }

};

TimerProtocol * Vancouver::timer_service;

ASMFUNCS(Vancouver, Vancouver)

#else // !VM_FUNC

// the VMX portals follow
VM_FUNC(PT_VMX + 2,  vmx_triple, MTD_ALL,
	handle_vcpu(pid, false, CpuMessage::TYPE_TRIPLE, tls, utcb);
	)
VM_FUNC(PT_VMX +  3,  vmx_init, MTD_ALL,
	handle_vcpu(pid, false, CpuMessage::TYPE_INIT, tls, utcb);
	)
VM_FUNC(PT_VMX +  7,  vmx_irqwin, MTD_IRQ,
	COUNTER_INC("irqwin");
	handle_vcpu(pid, false, CpuMessage::TYPE_CHECK_IRQ, tls, utcb);
	)
VM_FUNC(PT_VMX + 10,  vmx_cpuid, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_STATE,
	COUNTER_INC("cpuid");
	handle_vcpu(pid, true, CpuMessage::TYPE_CPUID, tls, utcb);
	)
VM_FUNC(PT_VMX + 12,  vmx_hlt, MTD_RIP_LEN | MTD_IRQ,
	handle_vcpu(pid, true, CpuMessage::TYPE_HLT, tls, utcb);
	)
VM_FUNC(PT_VMX + 18,  vmx_vmcall, MTD_RIP_LEN | MTD_GPR_ACDB,
	Logging::printf("vmcall eip %x eax %x,%x,%x\n", utcb->eip, utcb->eax, utcb->ecx, utcb->edx);
	utcb->eip += utcb->inst_len;
	)
VM_FUNC(PT_VMX + 30,  vmx_ioio, MTD_RIP_LEN | MTD_QUAL | MTD_GPR_ACDB | MTD_STATE | MTD_RFLAGS,
	if (utcb->qual[0] & 0x10)
	  {
	    COUNTER_INC("IOS");
	    force_invalid_gueststate_intel(utcb);
	  }
	else
	  {
	    unsigned order = utcb->qual[0] & 7;
	    if (order > 2) order = 2;
	    handle_io(tls, utcb, utcb->qual[0] & 8, order, utcb->qual[0] >> 16);
	  }
	)
VM_FUNC(PT_VMX + 31,  vmx_rdmsr, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_TSC | MTD_SYSENTER | MTD_STATE,
	COUNTER_INC("rdmsr");
	handle_vcpu(pid, true, CpuMessage::TYPE_RDMSR, tls, utcb);)
VM_FUNC(PT_VMX + 32,  vmx_wrmsr, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_SYSENTER | MTD_STATE | MTD_TSC,
	COUNTER_INC("wrmsr");
	handle_vcpu(pid, true, CpuMessage::TYPE_WRMSR, tls, utcb);)
VM_FUNC(PT_VMX + 33,  vmx_invalid, MTD_ALL,
	utcb->efl |= 2;
	handle_vcpu(pid, false, CpuMessage::TYPE_SINGLE_STEP, tls, utcb);
	utcb->mtd |= MTD_RFLAGS;
	)
VM_FUNC(PT_VMX + 40,  vmx_pause, MTD_RIP_LEN | MTD_STATE,
	CpuMessage msg(CpuMessage::TYPE_SINGLE_STEP, static_cast<CpuState *>(utcb), utcb->mtd);
	skip_instruction(msg);
	COUNTER_INC("pause");
	)
VM_FUNC(PT_VMX + 48,  vmx_mmio, MTD_ALL,
	COUNTER_INC("MMIO");
	/**
	 * Idea: optimize the default case - mmio to general purpose register
	 * Need state: GPR_ACDB, GPR_BSD, RIP_LEN, RFLAGS, CS, DS, SS, ES, RSP, CR, EFER
	 */
	if (!map_memory_helper(tls, utcb, utcb->qual[0] & 0x38))
	  // this is an access to MMIO
	  handle_vcpu(pid, false, CpuMessage::TYPE_SINGLE_STEP, tls, utcb);
	)
VM_FUNC(PT_VMX + 0xfe,  vmx_startup, MTD_IRQ,
	Logging::printf("startup\n");
	handle_vcpu(pid, false, CpuMessage::TYPE_HLT, tls, utcb);
	utcb->mtd |= MTD_CTRL;
	utcb->ctrl[0] = (1 << 3); // tscoffs
	utcb->ctrl[1] = 0;
	)
#define EXPERIMENTAL
VM_FUNC(PT_VMX + 0xff,  do_recall,
#ifdef EXPERIMENTAL
MTD_IRQ | MTD_RIP_LEN | MTD_GPR_BSD | MTD_GPR_ACDB,
#else
MTD_IRQ,
#endif
	COUNTER_INC("recall");
	COUNTER_SET("REIP", utcb->eip);
	handle_vcpu(pid, false, CpuMessage::TYPE_CHECK_IRQ, tls, utcb);
	)


// and now the SVM portals
VM_FUNC(PT_SVM + 0x64,  svm_vintr,   MTD_IRQ, vmx_irqwin(pid, tls, utcb); )
VM_FUNC(PT_SVM + 0x72,  svm_cpuid,   MTD_RIP_LEN | MTD_GPR_ACDB | MTD_IRQ, utcb->inst_len = 2; vmx_cpuid(pid, tls, utcb); )
VM_FUNC(PT_SVM + 0x78,  svm_hlt,     MTD_RIP_LEN | MTD_IRQ,  utcb->inst_len = 1; vmx_hlt(pid, tls, utcb); )
VM_FUNC(PT_SVM + 0x7b,  svm_ioio,    MTD_RIP_LEN | MTD_QUAL | MTD_GPR_ACDB | MTD_STATE,
	{
	  if (utcb->qual[0] & 0x4)
	    {
	      COUNTER_INC("IOS");
	      force_invalid_gueststate_amd(utcb);
	    }
	  else
	    {
	      unsigned order = ((utcb->qual[0] >> 4) & 7) - 1;
	      if (order > 2)  order = 2;
	      utcb->inst_len = utcb->qual[1] - utcb->eip;
	      handle_io(tls, utcb, utcb->qual[0] & 1, order, utcb->qual[0] >> 16);
	    }
	}
	)
VM_FUNC(PT_SVM + 0x7c,  svm_msr,     MTD_ALL, svm_invalid(pid, tls, utcb); )
VM_FUNC(PT_SVM + 0x7f,  svm_shutdwn, MTD_ALL, vmx_triple(pid, tls, utcb); )
VM_FUNC(PT_SVM + 0xfc,  svm_npt,     MTD_ALL,
	if (!map_memory_helper(tls, utcb, utcb->qual[0] & 1))
	  svm_invalid(pid, tls, utcb);
	)
VM_FUNC(PT_SVM + 0xfd, svm_invalid, MTD_ALL,
	COUNTER_INC("invalid");
	handle_vcpu(pid, false, CpuMessage::TYPE_SINGLE_STEP, tls, utcb);
	utcb->mtd |= MTD_CTRL;
	utcb->ctrl[0] = 1 << 18; // cpuid
	utcb->ctrl[1] = 1 << 0;  // vmrun
	)
VM_FUNC(PT_SVM + 0xfe,  svm_startup,MTD_ALL,  vmx_irqwin(pid, tls, utcb); )
VM_FUNC(PT_SVM + 0xff,  svm_recall, MTD_IRQ,  do_recall(pid, tls, utcb); )
#endif
