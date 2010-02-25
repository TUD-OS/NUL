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


#include "models/keyboard.h"
#include "sigma0/console.h"
#include "vmm/motherboard.h"

/**
 * Layout of the capability space.
 */
enum Cap_space_layout
  {
    PT_IRQ       = 0x20,
    PT_VMX       = 0x100,
    PT_SVM       = 0x200,
  };

/****************************************************/
/* Static Variables                                 */
/****************************************************/

Motherboard   *_mb;
unsigned       _debug;
const void *   _forward_pkt;
long           _lockcount;
Semaphore      _lock;
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

  const char *debug_getname() { return "Vancouver"; };
  unsigned long  _physmem;
  unsigned long  _physsize;
  unsigned long  _iomem_start;

#define PT_FUNC(NAME)  static void  NAME(Utcb *utcb) __attribute__((regparm(0)))
#define EXCP_FUNC(NAME) PT_FUNC(NAME) __attribute__((noreturn))
#define VM_FUNC(NR, NAME, INPUT, CODE)					\
  PT_FUNC(NAME)								\
  {  CODE; }
  #include "vmx_funcs.h"

  // the portal functions follow

  PT_FUNC(do_gsi_boot)
  {
    utcb->eip = reinterpret_cast<unsigned *>(utcb->esp)[0];
    Logging::printf("%s eip %x esp %x\n", __func__, utcb->eip, utcb->esp);
  }

  EXCP_FUNC(got_exception)
  {
    Logging::printf("%s() #%x ",  __func__, utcb->head.pid);
    Logging::printf("rip %x rsp %x  %x:%x %x:%x %x", utcb->eip, utcb->esp,
		    utcb->edx, utcb->eax,
		    utcb->edi, utcb->esi,
		    utcb->ecx);
    reinterpret_cast<Vancouver*>(utcb->head.tls)->block_forever();
  }

  EXCP_FUNC(do_gsi)
  {
    unsigned res;
    bool shared = utcb->msg[1] >> 8;
    Logging::printf("%s(%x, %x, %x) %p\n", __func__, utcb->msg[0], utcb->msg[1], utcb->msg[2], utcb);
    while (1)
      {
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


  EXCP_FUNC(do_stdin)
  {
    StdinConsumer *stdinconsumer = new StdinConsumer(reinterpret_cast<Vancouver*>(utcb->head.tls)->_cap_free++);
    Sigma0Base::request_stdin(stdinconsumer, stdinconsumer->sm());

    while (1)
      {
	MessageKeycode *msg = stdinconsumer->get_buffer();
	switch ((msg->keycode & ~KBFLAG_NUM) ^ _keyboard_modifier)
	  {
	  case KBFLAG_EXTEND0 | 0x7c: // printscr
	    recall(_mb->vcpustate(0)->cap_vcpu);
	    break;
	  case KBCODE_SCROLL: // scroll lock
	    Logging::printf("toggle HLT\n");
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
	    Logging::printf("hz %x\n", _mb->vcpustate(0)->hazard);
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

  EXCP_FUNC(do_disk)
  {
    DiskConsumer *diskconsumer = new DiskConsumer(reinterpret_cast<Vancouver*>(utcb->head.tls)->_cap_free++);
    Sigma0Base::request_disks_attach(diskconsumer, diskconsumer->sm());

    while (1)
      {
	MessageDiskCommit *msg = diskconsumer->get_buffer();
	SemaphoreGuard l(_lock);
	_mb->bus_diskcommit.send(*msg);
	diskconsumer->free_buffer();
      }
  }

  EXCP_FUNC(do_timer)
  {
    TimerConsumer *timerconsumer = new TimerConsumer(reinterpret_cast<Vancouver*>(utcb->head.tls)->_cap_free++);
    Sigma0Base::request_timer_attach(timerconsumer, timerconsumer->sm());
    while (1)
      {
	COUNTER_INC("timer");
	timerconsumer->get_buffer();
	timerconsumer->free_buffer();

	SemaphoreGuard l(_lock);
	timeout_trigger();
      }
  }

  EXCP_FUNC(do_network)
  {
    NetworkConsumer *network_consumer = new NetworkConsumer(reinterpret_cast<Vancouver*>(utcb->head.tls)->_cap_free++);
    Sigma0Base::request_network_attach(network_consumer, network_consumer->sm());
    while (1)
      {
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
    utcb->head.mtr = Mtd(MTD_CTRL, 0);
    Cpu::atomic_or<volatile unsigned>(&_mb->vcpustate(0)->hazard, VirtualCpuState::HAZARD_CTRL);
  };

  static void force_invalid_gueststate_intel(Utcb *utcb)
  {
    utcb->efl &= ~2;
    utcb->head.mtr = Mtd(MTD_RFLAGS, 0);
  };



  void create_devices(Hip *hip, char *args)
  {
    _timeouts.init();

    _mb = new Motherboard(new Clock(hip->freq_tsc*1000));
    _mb->bus_irqlines.add(this, &Vancouver::receive_static<MessageIrq>);
    _mb->bus_hostop.add(this, &Vancouver::receive_static<MessageHostOp>);
    _mb->bus_console.add(this, &Vancouver::receive_static<MessageConsole>);
    _mb->bus_disk.add(this, &Vancouver::receive_static<MessageDisk>);
    _mb->bus_timer.add(this, &Vancouver::receive_static<MessageTimer>);
    _mb->bus_time.add(this, &Vancouver::receive_static<MessageTime>);
    _mb->bus_network.add(this, &Vancouver::receive_static<MessageNetwork>);
    _mb->bus_legacy.add(this, &Vancouver::receive_static<MessageLegacy>);
    _mb->bus_hwpcicfg.add(this, &Vancouver::receive_static<MessagePciConfig>);
    _mb->bus_acpi.add(this, &Vancouver::receive_static<MessageAcpi>);

    // create default devices
    char default_devices [] = "mem:0,0xa0000 mem:0x100000 init triplefault msr cpuid irq novahalifax ioio";
    _mb->parse_args(default_devices);

    // create devices from cmdline
    _mb->parse_args(args);
    _mb->bus_hwioin.debug_dump();
  }


  unsigned create_irq_thread(unsigned hostirq, void __attribute__((regparm(0))) (*func)(Utcb *utcb))
  {
    Logging::printf("%s %x\n", __PRETTY_FUNCTION__, hostirq);

    if (hostirq != ~0u) check(Sigma0Base::request_irq((hostirq & 0xff) + _hip->cfg_exc));

    unsigned stack_size = 0x1000;
    Utcb *utcb = alloc_utcb();
    void **stack = reinterpret_cast<void **>(memalign(0x1000, stack_size));
    stack[stack_size/sizeof(void *) - 1] = utcb;
    stack[stack_size/sizeof(void *) - 2] = reinterpret_cast<void *>(func);

    check(create_sm(_shared_sem[hostirq & 0xff] = _cap_free++));

    unsigned cap_ec =  _cap_free++;
    check(create_ec(cap_ec, utcb,  stack + stack_size/sizeof(void *) -  2, Cpu::cpunr(), PT_IRQ, false));
    utcb->head.tls = reinterpret_cast<unsigned>(this);
    utcb->msg[0] = (hostirq & 0xff) + _hip->cfg_exc; // the caps for irq threads start here
    utcb->msg[1] = hostirq;
    utcb->msg[2] = _shared_sem[hostirq & 0xff];

    // XXX How many time should an IRQ thread get?
    check(create_sc(_cap_free++, cap_ec, Qpd(2, 10000)));
    return cap_ec;
  }


  unsigned init_caps(Hip *hip)
  {
    _lock = Semaphore(&_lockcount, _cap_free++);
    check(create_sm(_lock.sm()));

    // create exception EC
    unsigned cap_ex = create_ec_helper(reinterpret_cast<unsigned>(this), 0, true);

    // create portals for exceptions
    for (unsigned i=0; i < 32; i++)
      if (i!=14 && i != 30) check(create_pt(i, cap_ex, got_exception, Mtd(MTD_ALL, 0)));

    // create the gsi boot portal
    create_pt(PT_IRQ + 30, cap_ex, do_gsi_boot,  Mtd(MTD_RSP | MTD_RIP_LEN, 0));

    // create worker
    unsigned cap_worker = create_ec_helper(reinterpret_cast<unsigned>(this), 0, true);


    // create portals for VCPU faults
#undef VM_FUNC
#define VM_FUNC(NR, NAME, INPUT, CODE) {NR, NAME, INPUT},
    struct vm_caps {
      unsigned nr;
      void __attribute__((regparm(0))) (*func)(Utcb *);
      unsigned mtd;
    } vm_caps[] = {
#include "vmx_funcs.h"
    };
    for (unsigned i=0; i < sizeof(vm_caps)/sizeof(vm_caps[0]); i++)
      {
	Logging::printf("create pt %x\n", vm_caps[i].nr);
	check(create_pt(vm_caps[i].nr, cap_worker, vm_caps[i].func, Mtd(vm_caps[i].mtd, 0)));
      }
    return 0;
  }


  static void instruction_emulation(Utcb *utcb, bool long_run)
  {
    if (_debug)
      Logging::printf("execute %s at %x:%x pid %d cr3 %x inj_info %x hazard %x\n", __func__, utcb->cs.sel, utcb->eip, utcb->head.pid,
		      utcb->cr3, utcb->inj_info, _mb->vcpustate(0)->hazard);
    // enter singlestep
    utcb->head.pid = MessageExecutor::DO_ENTER;
    execute_all(static_cast<CpuState*>(utcb), _mb->vcpustate(0), false);

    utcb->head.pid = MessageExecutor::DO_SINGLESTEP;
    do {
      if (!execute_all(static_cast<CpuState*>(utcb), _mb->vcpustate(0)))
	Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, utcb->cs.sel, utcb->eip, utcb->head.pid);
      do_recall(utcb);
    }
    while (long_run && utcb->head.pid);

    // leave singlestep
    utcb->head.pid = MessageExecutor::DO_LEAVE;
    execute_all(static_cast<CpuState*>(utcb), _mb->vcpustate(0), false);


    if (~_mb->vcpustate(0)->hazard & VirtualCpuState::HAZARD_CRWRITE)
      utcb->head.mtr =  Mtd(utcb->head.mtr.untyped() & ~MTD_CR, 0);
    else
      Cpu::atomic_and<volatile unsigned>(&_mb->vcpustate(0)->hazard, ~VirtualCpuState::HAZARD_CRWRITE);
  }


  static bool execute_all(CpuState *cpu, VirtualCpuState *vcpu, bool early_out = true)
  {
    SemaphoreGuard l(_lock);
    MessageExecutor msg(cpu, vcpu);
    return _mb->bus_executor.send(msg, early_out, cpu->head.pid);
  }

  static void skip_instruction(Utcb *utcb)
  {
    assert(utcb->head.mtr.untyped() & MTD_RIP_LEN);
    assert(utcb->head.mtr.untyped() & MTD_STATE);
    utcb->eip += utcb->inst_len;
    /**
     * Cancel sti and mov-ss blocking as we emulated an instruction.
     */
    utcb->intr_state &= ~3;
  }

  static void ioio_helper(Utcb *utcb, bool is_in, unsigned order)
  {
    SemaphoreGuard l(_lock);
    if (is_in)
      {
	COUNTER_INC("IN");
	if ((utcb->qual[0] >> 16) == 0x40) COUNTER_INC("in(0x40)");
	if ((utcb->qual[0] >> 16) == 0x21) COUNTER_INC("in(0x21)");
	MessageIOIn msg(static_cast<MessageIOIn::Type>(order), utcb->qual[0] >> 16);
	if (!_mb->bus_ioin.send(msg))
	  Logging::printf("could not IN from port %x\n", msg.port);
	memcpy(&utcb->eax, &msg.value, 1 << order);
      }
    else
      {
	COUNTER_INC("OUT");
	if ((utcb->qual[0] >> 16) == 0x40) COUNTER_INC("out(0x40)");
	if ((utcb->qual[0] >> 16) == 0x20) COUNTER_INC("out(0x20)");
	if ((utcb->qual[0] >> 16) == 0x21) COUNTER_INC("out(0x21)");
	MessageIOOut msg(static_cast<MessageIOOut::Type>(order), utcb->qual[0] >> 16, utcb->eax);
	_mb->bus_ioout.send(msg);
      }
    skip_instruction(utcb);
  }


  static void wakeup_cpu(unsigned i)
  {
    if (_mb->vcpustate(i)->hazard & VirtualCpuState::HAZARD_INHLT && _mb->vcpustate(0)->block_sem)
      _mb->vcpustate(0)->block_sem->up();
    else
      recall(_mb->vcpustate(i)->cap_vcpu);
  }

  static bool map_memory_helper(Utcb *utcb)
  {
    MessageMemMap msg(utcb->qual[1] & ~0xfff, 0, 0);

    // do we have not mapped physram yet?
    if (_mb->bus_memmap.send(msg, true))
      {
	Logging::printf("%s(%llx) phys %lx ptr %p+%x eip %x\n", __func__, utcb->qual[1], msg.phys, msg.ptr, msg.count, utcb->eip);
	utcb->head.mtr = Mtd();
	utcb->add_mappings(true, reinterpret_cast<unsigned long>(msg.ptr), msg.count, msg.phys, 0x1c | 1);
	return true;
      }
    return false;
  }


  static bool handle_special_cpuid(Utcb *utcb)
  {
    switch (utcb->eax)
      {
      case 0x40000000:
	//syscall(254, utcb->ebx, 0, 0, 0);
	break;
      case 0x40000001:
	_mb->dump_counters();
	break;
      case 0x40000002:
	{
	  unsigned long long c1=0;
	  unsigned long long c2=0;
	  perfcount(utcb->ebx, utcb->ecx, c1, c2);
	  utcb->eax = c1 >> 32;
	  utcb->ebx = c1;
	  utcb->ecx = c2 >> 32;
	  utcb->edx = c2;
	}
	break;
      default:
	return false;
      }

    // we are sure here, since this is our experimental code
    utcb->inst_len = 2;
    skip_instruction(utcb);
    return true;
  }

public:

  bool  receive(MessageIrq &msg)
  {
    if (msg.line == MessageIrq::LINT0 || msg.line > 0x40)
      {
	for (unsigned i=0; i < Motherboard::MAX_VCPUS; i++)
	  if (msg.type == MessageIrq::ASSERT_IRQ)
	    {
	      if (msg.line > 0x40 && msg.line != MessageIrq::LINT0) _mb->vcpustate(i)->lastmsi = msg.line;
	      if (~_mb->vcpustate(i)->hazard & VirtualCpuState::HAZARD_IRQ)
		{
		  Cpu::atomic_or<volatile unsigned>(&_mb->vcpustate(i)->hazard, VirtualCpuState::HAZARD_IRQ);
		  wakeup_cpu(i);
		}
	    }
	  else
	    Cpu::atomic_and<volatile unsigned>(&_mb->vcpustate(i)->hazard, ~VirtualCpuState::HAZARD_IRQ);
	return true;
      }
    return false;
  };


  bool  receive(MessageLegacy &msg)
  {
    if (msg.type == MessageLegacy::RESET || msg.type == MessageLegacy::INIT)
      {
	Logging::printf("try to RESET the machine (%x, %x)\n", msg.type, msg.value);
	for (unsigned i=0; i < Motherboard::MAX_VCPUS; i++)
	  {
	    Cpu::atomic_or<volatile unsigned>(&_mb->vcpustate(i)->hazard, VirtualCpuState::HAZARD_INIT);
	    wakeup_cpu(i);
	  }
      }
    else if (msg.type == MessageLegacy::GATE_A20 || msg.type == MessageLegacy::FAST_A20)
      {
	if (!msg.value)
	  Logging::printf("no idea how to toggle A20 (%x, %x)", msg.type, msg.value);
	else
	  Logging::printf("no idea how to toggle A20 (%x, %x) - already enabled!\n",  msg.type,  msg.value);
      }
    else
      return false;
    return true;
  }


  bool  receive(MessageHostOp &msg)
  {
    bool res = true;
    switch (msg.type)
      {
      case MessageHostOp::OP_ALLOC_IOIO_REGION:
	res = ! Sigma0Base::request_io(msg.value >> 8, 1 << (msg.value & 0xff), true, _iomem_start);
	Logging::printf("alloc ioio region %lx %s\n", msg.value, res ? "done" :  "failed");
	break;
      case MessageHostOp::OP_ALLOC_IOMEM:
	msg.ptr = reinterpret_cast<char *>(Sigma0Base::request_io(msg.value, msg.len, false, _iomem_start));
	res = msg.value;
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
      case MessageHostOp::OP_ATTACH_IRQ:
	create_irq_thread(msg.value, do_gsi);
      case MessageHostOp::OP_GET_MODULE:
      case MessageHostOp::OP_ASSIGN_PCI:
      case MessageHostOp::OP_GET_UID:
	res = Sigma0Base::hostop(msg);
	break;
      case MessageHostOp::OP_ATTACH_MSI:
	res  = Sigma0Base::hostop(msg);
	create_irq_thread(msg.msi_gsi, do_gsi);
	break;
      case MessageHostOp::OP_VIRT_TO_PHYS:
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
    bool reprogram = false;
    while ((nr = _timeouts.trigger(now)))
      {
	reprogram |= _timeouts.cancel(nr) == 0;
	MessageTimeout msg(nr);
	_mb->bus_timeout.send(msg);
      }
    if (reprogram &&  _timeouts.timeout() != ~0ull) {
      // update timeout in sigma0
      MessageTimer msg2(0, _timeouts.timeout());
      Sigma0Base::timer(msg2);
    }
  }


  bool  receive(MessageTimer &msg)
  {
    COUNTER_INC("requestTO");
    int res = 1;
    switch (msg.type)
      {
      case MessageTimer::TIMER_NEW:
	msg.nr = _timeouts.alloc();
	return true;
      case MessageTimer::TIMER_REQUEST_TIMEOUT:
	res = _timeouts.request(msg.nr, msg.abstime);
	break;
      default:
	return false;
      }
    if (res == 0 && _timeouts.timeout() != ~0ull)
      {
	// update timeout in sigma0
	MessageTimer msg2(0, _timeouts.timeout());
	Sigma0Base::timer(msg2);
      }
    return true;
  }

  bool  receive(MessageTime &msg) {  return Sigma0Base::time(msg);  }

public:
  void __attribute__((noreturn)) run(Hip *hip)
  {
    console_init("VMM");
    assert(hip);
    unsigned res;
    if ((res = init(hip))) Logging::panic("init failed with %x", res);

    char *args = reinterpret_cast<char *>(hip->get_mod(0)->aux);
    Logging::printf("Vancouver: hip %p utcb %p args '%s'\n", hip, _boot_utcb, args);

    extern char __freemem;
    _physmem = reinterpret_cast<unsigned long>(&__freemem);
    _physsize = 0;
    // get physsize
    for (int i=0; i < (hip->length - hip->mem_offs) / hip->mem_size; i++)
      {
	Hip_mem *hmem = reinterpret_cast<Hip_mem *>(reinterpret_cast<char *>(hip) + hip->mem_offs) + i;
	if (hmem->type == 1 && hmem->addr <= _physmem)
	  {
	    _physsize = hmem->size - (_physmem - hmem->addr);
	    _iomem_start = hmem->addr + hmem->size;
	    break;
	  }
      }

    if (init_caps(hip))
      Logging::panic("init_caps() failed\n");

    create_devices(hip, args);

    // create backend connections
    create_irq_thread(~0u, do_stdin);
    create_irq_thread(~0u, do_disk);
    create_irq_thread(~0u, do_timer);
    create_irq_thread(~0u, do_network);

    Logging::printf("create VCPU\n");
    // create a single VCPU
    _mb->vcpustate(0)->block_sem = new KernelSemaphore(_cap_free++);
    if (create_sm(_mb->vcpustate(0)->block_sem->sm()))
      Logging::panic("could not create blocking semaphore\n");
    _mb->vcpustate(0)->cap_vcpu = _cap_free++;
    if (create_ec(_mb->vcpustate(0)->cap_vcpu, 0, 0, Cpu::cpunr(), hip->has_svm() ? PT_SVM : PT_VMX, false)
	|| create_sc(_cap_free++, _mb->vcpustate(0)->cap_vcpu, Qpd(1, 10000)))
      Logging::panic("creating a VCPU failed - does your CPU support VMX?");

    _lock.up();
    Logging::printf("INIT done\n");

    // block ourself since we have finished initialization
    block_forever();
  }


  static void  exit(unsigned long status)
  {
    // switch to our view
    MessageConsole msg;
    msg.type = MessageConsole::TYPE_SWITCH_VIEW;
    msg.view = 0;
    Sigma0Base::console(msg);

    Logging::printf("%s(%lx)\n", __func__, status);
  }

};

ASMFUNCS(Vancouver, Vancouver);
