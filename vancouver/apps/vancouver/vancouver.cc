/*
 * Main code and static vars of vancouver.nova.
 *
 * Copyright (C) 2007-2009, Bernhard Kauer <bk@vmmon.org>
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
#include "sigma0/console.h"
#include "nul/motherboard.h"
#include "nul/program.h"
#include "nul/vcpu.h"

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
long           _lockcount;
Semaphore      _lock;
long           _consolelock;
TimeoutList<32>_timeouts;
unsigned       _shared_sem[256];
unsigned       _keyboard_modifier = KBFLAG_RWIN;
PARAM(kbmodifier,
      _keyboard_modifier = argv[0];
      ,
      "kbmodifier:value - change the kbmodifier. Default: RWIN.",
      "Example: 'kbmodifier:0x40000' uses LWIN as modifier.",
      "See keyboard.h for definitions.")

/****************************************************/
/* Vancouver class                                  */
/****************************************************/



class Vancouver : public NovaProgram, public ProgramConsole, public StaticReceiver<Vancouver>
{

  unsigned long  _physmem;
  unsigned long  _physsize;
  unsigned long  _iomem_start;

#define PT_FUNC(NAME)  static unsigned long  NAME(unsigned pid, Utcb *utcb) __attribute__((regparm(1)))
#define VM_FUNC(NR, NAME, INPUT, CODE)					\
  PT_FUNC(NAME)								\
  {  CODE; return utcb->head.mtr.value(); }
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
    reinterpret_cast<Vancouver*>(utcb->head.tls)->block_forever();
  }


  PT_FUNC(do_gsi_pf)
  {

    Logging::printf("%s eip %x qual %llx\n", __func__, utcb->eip, utcb->qual[1]);
    asm volatile("orl $0, (%0)": : "r"(utcb->qual[1]) : "memory");
    return 0;
  }

  PT_FUNC(do_gsi_boot)
  {
    utcb->eip = reinterpret_cast<unsigned *>(utcb->esp)[0];
    Logging::printf("%s eip %x esp %x\n", __func__, utcb->eip, utcb->esp);
    return  utcb->head.mtr.value();
  }


  PT_FUNC(do_gsi) __attribute__((noreturn))
  {
    unsigned res;
    bool shared = utcb->msg[1] >> 8;
    Logging::printf("%s(%x, %x, %x) %p\n", __func__, utcb->msg[0], utcb->msg[1], utcb->msg[2], utcb);
    while (1) {
      if ((res = semdown(utcb->msg[0])))
	Logging::panic("%s(%x) request failed with %x\n", __func__, utcb->msg[0], res);
      {
	SemaphoreGuard l(_lock);
	MessageIrq msg(shared ? MessageIrq::ASSERT_NOTIFY : MessageIrq::ASSERT_IRQ, utcb->msg[1] & 0xff);
	_mb->bus_hostirq.send(msg);
      }
      if (shared)  semdown(utcb->msg[2]);
    }
  }


  PT_FUNC(do_stdin) __attribute__((noreturn))
  {
    StdinConsumer *stdinconsumer = new StdinConsumer(reinterpret_cast<Vancouver*>(utcb->head.tls)->alloc_cap());
    assert(stdinconsumer);
    Sigma0Base::request_stdin(utcb, stdinconsumer, stdinconsumer->sm());

    while (1) {
      MessageKeycode *msg = stdinconsumer->get_buffer();
      switch ((msg->keycode & ~KBFLAG_NUM) ^ _keyboard_modifier)
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
	  revoke_all_mem(&__freemem, 0x30000000, 0x1c, true);
	  break;
	case KBFLAG_EXTEND1 | KBFLAG_RELEASE | 0x77: // break
	  _debug = true;
	  _mb->dump_counters();
	  syscall(254, 0, 0, 0, 0);
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
	_mb->bus_keycode.send(*msg);
	stdinconsumer->free_buffer();
    }
  }

  PT_FUNC(do_disk) __attribute__((noreturn))
  {
    DiskConsumer *diskconsumer = new DiskConsumer(reinterpret_cast<Vancouver*>(utcb->head.tls)->alloc_cap());
    assert(diskconsumer);
    Sigma0Base::request_disks_attach(utcb, diskconsumer, diskconsumer->sm());
    while (1) {

      MessageDiskCommit *msg = diskconsumer->get_buffer();
      SemaphoreGuard l(_lock);
      _mb->bus_diskcommit.send(*msg);
      diskconsumer->free_buffer();
    }
  }

  PT_FUNC(do_timer) __attribute__((noreturn))
  {
    TimerConsumer *timerconsumer = new TimerConsumer(reinterpret_cast<Vancouver*>(utcb->head.tls)->alloc_cap());
    assert(timerconsumer);
    Sigma0Base::request_timer_attach(utcb, timerconsumer, timerconsumer->sm());
    while (1) {

      COUNTER_INC("timer");
      timerconsumer->get_buffer();
      timerconsumer->free_buffer();

      SemaphoreGuard l(_lock);
      timeout_trigger();
    }
  }

  PT_FUNC(do_network) __attribute__((noreturn))
  {
    NetworkConsumer *network_consumer = new NetworkConsumer(reinterpret_cast<Vancouver*>(utcb->head.tls)->alloc_cap());
    Sigma0Base::request_network_attach(utcb, network_consumer, network_consumer->sm());
    while (1) {
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


  static void force_invalid_gueststate_amd(Utcb *utcb)
  {
    utcb->ctrl[1] = 0;
    utcb->head.mtr = MTD_CTRL;
  };

  static void force_invalid_gueststate_intel(Utcb *utcb)
  {
    utcb->efl &= ~2;
    utcb->head.mtr = MTD_RFLAGS;
  };



  void create_devices(Hip *hip, char *args)
  {
    _timeouts.init();

    _mb = new Motherboard(new Clock(hip->freq_tsc*1000));
    _mb->bus_hostop.add(this, &Vancouver::receive_static<MessageHostOp>);
    _mb->bus_console.add(this, &Vancouver::receive_static<MessageConsole>);
    _mb->bus_disk.add(this, &Vancouver::receive_static<MessageDisk>);
    _mb->bus_timer.add(this, &Vancouver::receive_static<MessageTimer>);
    _mb->bus_time.add(this, &Vancouver::receive_static<MessageTime>);
    _mb->bus_network.add(this, &Vancouver::receive_static<MessageNetwork>);
    _mb->bus_hwpcicfg.add(this, &Vancouver::receive_static<MessagePciConfig>);
    _mb->bus_acpi.add(this, &Vancouver::receive_static<MessageAcpi>);

    _mb->parse_args(args);
    _mb->bus_hwioin.debug_dump();
  }


  unsigned create_irq_thread(unsigned hostirq, unsigned irq_cap, unsigned long __attribute__((regparm(1))) (*func)(unsigned, Utcb *))
  {
    Logging::printf("%s %x\n", __PRETTY_FUNCTION__, hostirq);

    unsigned stack_size = 0x1000;
    Utcb *utcb = alloc_utcb();
    void **stack = new(0x1000) void *[stack_size / sizeof(void *)];
    stack[stack_size/sizeof(void *) - 1] = utcb;
    stack[stack_size/sizeof(void *) - 2] = reinterpret_cast<void *>(func);

    check1(~1u, create_sm(_shared_sem[hostirq & 0xff] = alloc_cap()));

    unsigned cap_ec =  alloc_cap();
    check1(~2u, create_ec(cap_ec, utcb,  stack + stack_size/sizeof(void *) -  2, Cpu::cpunr(), PT_IRQ, false));
    utcb->head.tls = reinterpret_cast<unsigned>(this);
    utcb->msg[0] = irq_cap;
    utcb->msg[1] = hostirq;
    utcb->msg[2] = _shared_sem[hostirq & 0xff];

    // XXX How many time should an IRQ thread get?
    check1(~3u, create_sc(alloc_cap(), cap_ec, Qpd(2, 10000)));
    return cap_ec;
  }


  unsigned init_caps()
  {
    _lock = Semaphore(&_lockcount, alloc_cap());
    check1(1, create_sm(_lock.sm()));

    _consolelock = 1;
    _console_data.sem = new Semaphore(&_consolelock, alloc_cap());
    check1(2, create_sm(_console_data.sem->sm()));


    // create exception EC
    unsigned cap_ex = create_ec_helper(reinterpret_cast<unsigned>(this), 0, true);

    // create portals for exceptions
    for (unsigned i=0; i < 32; i++)
      if (i!=14 && i != 30) check1(3, create_pt(i, cap_ex, got_exception, Mtd(MTD_ALL, 0)));

    // create the gsi boot portal
    create_pt(PT_IRQ + 14, cap_ex, do_gsi_pf,    Mtd(MTD_RIP_LEN | MTD_QUAL, 0));
    create_pt(PT_IRQ + 30, cap_ex, do_gsi_boot,  Mtd(MTD_RSP | MTD_RIP_LEN, 0));
    return 0;
  }

  unsigned create_vcpu(VCpu *vcpu, bool use_svm)
  {
    // create worker
    unsigned cap_worker = create_ec_helper(reinterpret_cast<unsigned>(vcpu), 0, true);

    // create portals for VCPU faults
#undef VM_FUNC
#define VM_FUNC(NR, NAME, INPUT, CODE) {NR, NAME, INPUT},
    struct vm_caps {
      unsigned nr;
      unsigned long __attribute__((regparm(1))) (*func)(unsigned, Utcb *);
      unsigned mtd;
    } vm_caps[] = {
#include "vancouver.cc"
    };
    unsigned cap_start = alloc_cap(0x100);
    for (unsigned i=0; i < sizeof(vm_caps)/sizeof(vm_caps[0]); i++) {
      if (use_svm == (vm_caps[i].nr < PT_SVM)) continue;
      //Logging::printf("\tcreate pt %x\n", vm_caps[i].nr);
      check1(0, create_pt(cap_start + (vm_caps[i].nr & 0xff), cap_worker, vm_caps[i].func, Mtd(vm_caps[i].mtd, 0)));
    }

    Logging::printf("\tcreate VCPU\n");
    unsigned cap_block = alloc_cap(3);
    if (create_sm(cap_block))
      Logging::panic("could not create blocking semaphore\n");
    if (create_ec(cap_block + 1, 0, 0, Cpu::cpunr(), cap_start, false)
	|| create_sc(cap_block + 2, cap_block + 1, Qpd(1, 10000)))
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


  static void handle_io(Utcb *utcb, bool is_in, unsigned io_order, unsigned port) {
    CpuMessage msg(is_in, static_cast<CpuState *>(utcb), io_order, port, &utcb->eax, utcb->head.mtr.untyped());
    skip_instruction(msg);

    VCpu *vcpu= reinterpret_cast<VCpu*>(utcb->head.tls);
    {
      SemaphoreGuard l(_lock);
      if (!vcpu->executor.send(msg, true))
	Logging::panic("nobody to execute %s at %x:%x\n", __func__, utcb->cs.sel, utcb->eip);
    }
    //Logging::printf("\tio type %x port %x eax %x\n", is_in, port, utcb->eax);
    utcb->head.mtr = msg.mtr_out;
  }


  static void handle_vcpu(unsigned pid, Utcb *utcb, CpuMessage::Type type, bool skip=false)
  {

    CpuMessage msg(type, static_cast<CpuState *>(utcb), utcb->head.mtr.untyped());
    if (skip) skip_instruction(msg);

    VCpu *vcpu= reinterpret_cast<VCpu*>(utcb->head.tls);
    {
      SemaphoreGuard l(_lock);
      if (!vcpu->executor.send(msg, true))
	Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, utcb->cs.sel, utcb->eip, pid);
    }

    /**
     * check whether we should inject something
     */
    if (msg.mtr_in & MTD_INJ) {
      msg.type = CpuMessage::TYPE_CHECK_IRQ;
      if (!vcpu->executor.send(msg, true))
	Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, utcb->cs.sel, utcb->eip, pid);
    }


    /**
     * If the IRQ injection is performed, recalc the IRQ window.
     */
    if (msg.mtr_out & MTD_INJ) {
      msg.type = CpuMessage::TYPE_CALC_IRQWINDOW;
      if (!vcpu->executor.send(msg, true))
	Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, utcb->cs.sel, utcb->eip, pid);
    }
    utcb->head.mtr = msg.mtr_out;
    if (utcb->head.mtr.typed())
      Logging::printf("> %s pid %p,%d eip %x:%x out %x inj %x typed %x\n", __func__, utcb, pid, utcb->cs.sel, utcb->eip, utcb->head.mtr.untyped(), utcb->inj_info, utcb->head.mtr.typed());
  }


  static bool map_memory_helper(Utcb *utcb)
  {
    MessageMemRegion msg(utcb->qual[1] >> 12);

    // XXX use a push model on _startup instead
    // do we have not mapped physram yet?
    if (_mb->bus_memregion.send(msg, true) && msg.ptr) {

      // get the memory ourself -> XXX this would break IOMEM!
      asm volatile("orl $0, (%0)": : "r"(msg.ptr) : "memory");

      Logging::printf("%s(%llx, %llx) phys %lx ptr %p pages %x eip %x\n", __func__, utcb->qual[1], utcb->qual[0], msg.start_page << 12, msg.ptr, msg.count, utcb->eip);
      if (utcb->qual[0] & 0x38) revoke_all_mem(msg.ptr, msg.count << 12, 0x1c, false);

      utcb->head.mtr = Mtd();
      utcb->add_mappings(true, reinterpret_cast<unsigned long>(msg.ptr), msg.count << 12, msg.start_page << 12, 0x1c | 1);
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
      case 0x40000000:
	//syscall(254, msg.cpu->ebx, 0, 0, 0);
	break;
      case 0x40000001:
	_mb->dump_counters();
	break;
      case 0x40000002:
	{
	  unsigned long long c1=0;
	  unsigned long long c2=0;
	  perfcount(msg.cpu->ebx, msg.cpu->ecx, c1, c2);
	  msg.cpu->eax = c1 >> 32;
	  msg.cpu->ebx = c1;
	  msg.cpu->ecx = c2 >> 32;
	  msg.cpu->edx = c2;
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
	  myutcb()->head.crd = Crd(msg.value >> 8, msg.value & 0xff, 0x1c | 2).value();
	  res = ! Sigma0Base::hostop(msg);
	  Logging::printf("alloc ioio region %lx %s\n", msg.value, res ? "done" :  "failed");
	}
	break;
      case MessageHostOp::OP_ALLOC_IOMEM:
	{
	  _iomem_start = (_iomem_start + msg.len - 1) & ~(msg.len-1);
	  myutcb()->head.crd = Crd(_iomem_start >> 12, Cpu::bsr(msg.len) - 12, 1).value();
	  res = ! Sigma0Base::hostop(msg);
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
	if (msg.value <= _physsize)
	  {
	    _physsize -= msg.value;
	    msg.phys =  _physsize;
	  }
	else
	  res = false;
	break;
      case MessageHostOp::OP_NOTIFY_IRQ:
	semup(_shared_sem[msg.value & 0xff]);
	res = true;
	break;
      case MessageHostOp::OP_GET_MODULE:
      case MessageHostOp::OP_ASSIGN_PCI:
      case MessageHostOp::OP_GET_UID:
	res = Sigma0Base::hostop(msg);
	break;
      case MessageHostOp::OP_ATTACH_MSI:
      case MessageHostOp::OP_ATTACH_IRQ:
	{
	  unsigned irq_cap = alloc_cap();
	  myutcb()->head.crd = Crd(irq_cap, 0, 0x1c | 3).value();
	  res  = Sigma0Base::hostop(msg);
	  create_irq_thread(msg.type == MessageHostOp::OP_ATTACH_IRQ ? msg.value : msg.msi_gsi, irq_cap, do_gsi);
	}
	break;
      case MessageHostOp::OP_VCPU_CREATE_BACKEND:
	msg.value = create_vcpu(msg.vcpu, _hip->has_svm());

	// handle cpuid overrides
	msg.vcpu->executor.add(this, &Vancouver::receive_static<CpuMessage>);
	break;
      case MessageHostOp::OP_VCPU_BLOCK:
	_lock.up();
	semdown(msg.value);
	_lock.down();
	break;
      case MessageHostOp::OP_VCPU_RELEASE:
	if (msg.len)  semup(msg.value);
	recall(msg.value + 1);
	break;
      case MessageHostOp::OP_VIRT_TO_PHYS:
      case MessageHostOp::OP_RERAISE_IRQ:
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
    return Sigma0Base::disk(msg);
  }


  bool  receive(MessageNetwork &msg)
  {
    if (_forward_pkt == msg.buffer) return false;
    Sigma0Base::network(msg);
    return true;
  }

  bool  receive(MessageConsole &msg)   {  return Sigma0Base::console(msg); }
  bool  receive(MessagePciConfig &msg) {  return Sigma0Base::pcicfg(msg);  }
  bool  receive(MessageAcpi      &msg) {  return Sigma0Base::acpi(msg);    }

  static void timeout_trigger()
  {
    timevalue now = _mb->clock()->time();
    unsigned nr;
    while ((nr = _timeouts.trigger(now))) {
      _timeouts.cancel(nr);
      MessageTimeout msg(nr);
      _mb->bus_timeout.send(msg);
    }
    if (_timeouts.timeout() != ~0ull) {
      // update timeout in sigma0
      MessageTimer msg2(0, _timeouts.timeout());
      Sigma0Base::timer(msg2);
    }
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
	timeout_trigger();
	break;
      default:
	return false;
      }
    return true;
  }

  bool  receive(MessageTime &msg) {  return Sigma0Base::time(msg);  }

public:
  void __attribute__((noreturn)) run(Utcb *utcb, Hip *hip)
  {
    console_init("VMM");
    assert(hip);
    unsigned res;
    if ((res = init(hip))) Logging::panic("init failed with %x", res);

    char *args = reinterpret_cast<char *>(hip->get_mod(0)->aux);
    Logging::printf("Vancouver: hip %p utcb %p args '%s'\n", hip, utcb, args);

    extern char __freemem;
    _physmem = reinterpret_cast<unsigned long>(&__freemem);
    _physsize = 0;
    // get physsize
    for (int i=0; i < (hip->length - hip->mem_offs) / hip->mem_size; i++) {
	Hip_mem *hmem = reinterpret_cast<Hip_mem *>(reinterpret_cast<char *>(hip) + hip->mem_offs) + i;
	if (hmem->type == 1 && hmem->addr <= _physmem) {
	  _physsize = hmem->size - (_physmem - hmem->addr);
	  _iomem_start = hmem->addr + hmem->size;
	  break;
	}
    }

    if (init_caps())
      Logging::panic("init_caps() failed\n");

    create_devices(hip, args);

    // create backend connections
    create_irq_thread(~0u, 0, do_stdin);
    create_irq_thread(~0u, 0, do_disk);
    create_irq_thread(~0u, 0, do_timer);
    create_irq_thread(~0u, 0, do_network);


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
      //Logging::printf("SET cpuid edx %x mask %x\n", edx_1, 0x0f88a9bf);
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

ASMFUNCS(Vancouver, Vancouver)

#else // !VM_FUNC

// the VMX portals follow
VM_FUNC(PT_VMX + 2,  vmx_triple, MTD_ALL,
	handle_vcpu(pid, utcb, CpuMessage::TYPE_TRIPLE);
	)
VM_FUNC(PT_VMX +  3,  vmx_init, MTD_ALL,
	handle_vcpu(pid, utcb, CpuMessage::TYPE_INIT);
	)
VM_FUNC(PT_VMX +  7,  vmx_irqwin, MTD_IRQ,
	COUNTER_INC("irqwin");
	handle_vcpu(pid, utcb, CpuMessage::TYPE_CHECK_IRQ);
	)
//VM_FUNC(PT_VMX +  9,  vmx_taskswitch, MTD_ALL,
//	Logging::printf("TASK qual %llx eip %x cr0 %x cr3 %x inj %x\n", utcb->qual[0], utcb->eip, utcb->cr0, utcb->cr3, utcb->inj_info);
//     	handle_vcpu(pid, utcb, CpuMessage::TYPE_SINGLE_STEP);
//	)
VM_FUNC(PT_VMX + 10,  vmx_cpuid, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_STATE,
	COUNTER_INC("cpuid");
	//Logging::printf("CPUID eip %x %x\n", utcb->eip, utcb->eax);
	handle_vcpu(pid, utcb, CpuMessage::TYPE_CPUID, true);
	//Logging::printf("CPUID -> %x %x %x %x\n", utcb->eax, utcb->ebx, utcb->ecx, utcb->edx);
	)
VM_FUNC(PT_VMX + 12,  vmx_hlt, MTD_RIP_LEN | MTD_IRQ | MTD_STATE,
	handle_vcpu(pid, utcb, CpuMessage::TYPE_HLT, true);
	)
VM_FUNC(PT_VMX + 18,  vmx_vmcall, MTD_RIP_LEN | MTD_GPR_ACDB,
	Logging::printf("vmcall eip %x eax %x,%x,%x\n", utcb->eip, utcb->eax, utcb->ecx, utcb->edx);
	utcb->eip += utcb->inst_len;
	)
VM_FUNC(PT_VMX + 30,  vmx_ioio, MTD_RIP_LEN | MTD_QUAL | MTD_GPR_ACDB | MTD_STATE,
	//if (_debug) Logging::printf("guest ioio at %x port %llx len %x\n", utcb->eip, utcb->qual[0], utcb->inst_len);
	if (utcb->qual[0] & 0x10)
	  {
	    COUNTER_INC("IOS");
	    force_invalid_gueststate_intel(utcb);
	  }
	else
	  {
	    unsigned order = utcb->qual[0] & 7;
	    if (order > 2) order = 2;
	    handle_io(utcb, utcb->qual[0] & 8, order, utcb->qual[0] >> 16);
	  }
	)
VM_FUNC(PT_VMX + 31,  vmx_rdmsr, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_TSC | MTD_SYSENTER | MTD_STATE,
	COUNTER_INC("rdmsr");
	handle_vcpu(pid, utcb, CpuMessage::TYPE_RDMSR, true);
	//Logging::printf("RDMSR addr %x eip %x %x:%8x\n", utcb->ecx, utcb->eip, utcb->edx, utcb->eax);
)
VM_FUNC(PT_VMX + 32,  vmx_wrmsr, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_SYSENTER | MTD_STATE,
	COUNTER_INC("wrmsr");
	handle_vcpu(pid, utcb, CpuMessage::TYPE_WRMSR, true);)
VM_FUNC(PT_VMX + 33,  vmx_invalid, MTD_ALL,
	utcb->efl |= 2;
	handle_vcpu(pid, utcb, CpuMessage::TYPE_SINGLE_STEP);
	utcb->head.mtr.add(MTD_RFLAGS);
	)
VM_FUNC(PT_VMX + 40,  vmx_pause, MTD_RIP_LEN | MTD_STATE,
	CpuMessage msg(CpuMessage::TYPE_SINGLE_STEP, static_cast<CpuState *>(utcb), utcb->head.mtr.untyped());
	skip_instruction(msg);
	COUNTER_INC("pause");
	)
VM_FUNC(PT_VMX + 48,  vmx_mmio, MTD_ALL,
	COUNTER_INC("MMIO");
	/**
	 * Idea: optimize the default case - mmio to general purpose register
	 * Need state: GPR_ACDB, GPR_BSD, RIP_LEN, RFLAGS, CS, DS, SS, ES, RSP, CR, EFER
	 */
	if (!map_memory_helper(utcb))
	  // this is an access to MMIO
	  handle_vcpu(pid, utcb, CpuMessage::TYPE_SINGLE_STEP);
	)
VM_FUNC(PT_VMX + 0xfe,  vmx_startup, MTD_IRQ,
	Logging::printf("startup\n");
	handle_vcpu(pid, utcb, CpuMessage::TYPE_HLT);
	utcb->head.mtr.add(MTD_CTRL);
	utcb->ctrl[0] = (1 << 3); // tscoffs
	utcb->ctrl[1] = 0;
	)
#define EXPERIMENTAL
VM_FUNC(PT_VMX + 0xff,  do_recall,
#ifdef EXPERIMENTAL
MTD_IRQ | MTD_RIP_LEN,
#else
MTD_IRQ,
#endif
	COUNTER_INC("recall");
	COUNTER_SET("REIP", utcb->eip);
	handle_vcpu(pid, utcb, CpuMessage::TYPE_CHECK_IRQ);
	)


// and now the SVM portals
VM_FUNC(PT_SVM + 0x64,  svm_vintr,   MTD_IRQ, vmx_irqwin(pid, utcb); )
VM_FUNC(PT_SVM + 0x72,  svm_cpuid,   MTD_RIP_LEN | MTD_GPR_ACDB | MTD_IRQ, utcb->inst_len = 2; vmx_cpuid(pid, utcb); )
VM_FUNC(PT_SVM + 0x78,  svm_hlt,     MTD_RIP_LEN | MTD_IRQ,  utcb->inst_len = 1; vmx_hlt(pid, utcb); )
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
	      handle_io(utcb, utcb->qual[0] & 1, order, utcb->qual[0] >> 16);
	    }
	}
	)
VM_FUNC(PT_SVM + 0x7c,  svm_msr,     MTD_ALL, svm_invalid(pid, utcb); )
VM_FUNC(PT_SVM + 0x7f,  svm_shutdwn, MTD_ALL, vmx_triple(pid, utcb); )
VM_FUNC(PT_SVM + 0xfc,  svm_npt,     MTD_ALL,
	if (!map_memory_helper(utcb))
	  svm_invalid(pid, utcb);
	)
VM_FUNC(PT_SVM + 0xfd, svm_invalid, MTD_ALL,
	COUNTER_INC("invalid");
	handle_vcpu(pid, utcb, CpuMessage::TYPE_SINGLE_STEP);
	utcb->head.mtr.add(MTD_CTRL);
	utcb->ctrl[0] = 1 << 18; // cpuid
	utcb->ctrl[1] = 1 << 0;  // vmrun
	)
VM_FUNC(PT_SVM + 0xfe,  svm_startup,MTD_ALL,  vmx_irqwin(pid, utcb); )
VM_FUNC(PT_SVM + 0xff,  svm_recall, MTD_IRQ,  do_recall(pid, utcb); )
#endif
