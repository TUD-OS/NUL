/*
 * Main code and static vars of vancouver.nova.
 *
 * Copyright (C) 2007-2009, Bernhard Kauer <bk@vmmon.org>
 *
 * This file is part of Vancouver.nova.
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


#include "host/screen.h"
#include "models/keyboard.h"
#include <sigma0.h>
#include "sys/console.h"
#include "vmm/motherboard.h"

using namespace Nova;

/**
 * Layout of the capability space.
 */
enum Cap_space_layout
  {
    PT_RECALL    = 9,
    PT_VMX       = 0x120,
    PT_SVM       = 0x220,
    PT_SVM_END   = PT_SVM + 0xff,
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

unsigned _keyboard_modifier = KBFLAG_RWIN;
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

#define VM_FUNC(NR, NAME, INPUT, CODE)					\
  static void  NAME(Utcb *utcb) __attribute__((regparm(0)))		\
  {  CODE; }
  
#define EXCP_FUNC(NAME, CODE)						\
  static void  NAME(Utcb *utcb) __attribute__((regparm(0))) __attribute__((noreturn)) \
  {  CODE;  }
  
  #include "vmx_funcs.h"

  static void force_invalid_gueststate_amd(Utcb *utcb)
  {
    utcb->ctrl[1] = 0;
    utcb->head.mtr = MTD_CTRL;
    Cpu::atomic_or<volatile unsigned>(&_mb->vcpustate(0)->hazard, VirtualCpuState::HAZARD_CTRL);
  };

  static void force_invalid_gueststate_intel(Utcb *utcb)
  {
    utcb->efl &= ~2;
    utcb->head.mtr = MTD_EFL;
  };



  void create_devices(Hip *hip, char *args)
  {
    _timeouts.init();

    _mb = new Motherboard(new Clock(hip->tsc_freq_khz*1000));
    _mb->bus_irqlines.add(this, &Vancouver::receive_static<MessageIrq>);
    _mb->bus_hostop.add(this, &Vancouver::receive_static<MessageHostOp>);
    _mb->bus_console.add(this, &Vancouver::receive_static<MessageConsole>);
    _mb->bus_disk.add(this, &Vancouver::receive_static<MessageDisk>);
    _mb->bus_timer.add(this, &Vancouver::receive_static<MessageTimer>);
    _mb->bus_time.add(this, &Vancouver::receive_static<MessageTime>);
    _mb->bus_network.add(this, &Vancouver::receive_static<MessageNetwork>);
    _mb->bus_legacy.add(this, &Vancouver::receive_static<MessageLegacy>);

    // create default devices
    char default_devices [] = "mem:0,0xa0000 mem:0x100000 init triplefault msr cpuid irq novahalifax nixon ioio";
    _mb->parse_args(default_devices);

    // create devices from cmdline
    _mb->parse_args(args);    
    _mb->bus_hwioin.debug_dump();
  }


  unsigned create_irq_thread(unsigned hostirq, void __attribute__((regparm(0))) (*func)(Utcb *utcb))
  {
    Logging::printf("%s %x\n", __PRETTY_FUNCTION__, hostirq);
  
    if (hostirq != ~0u) check(Sigma0Base::request_irq(hostirq + _hip->pre));

    debug_ec_name("v ", hostirq);
    unsigned cap_ec = create_ec_helper(reinterpret_cast<unsigned>(this));
    unsigned cap_pt = _cap_free++;
    create_pt(cap_pt, cap_ec, empty_message(), func);

    // XXX How many time should an IRQ thread get?
    check(create_sc(_cap_free++, cap_ec, qpd(2, 10000)));

    Utcb *utcb = myutcb();
    utcb->msg[0] = hostirq + _hip->pre; // the caps for irq threads start here
    utcb->msg[1] = hostirq;
    check(call(SEND, cap_pt, untyped_words(2)));
    return cap_ec;
  }


  unsigned init_caps(Hip *hip)
  {
    _lock = Semaphore(&_lockcount, _cap_free++);
    check(create_sm(_lock.sm(), 0));

    // create exception EC
    debug_ec_name("excp", 0);
    unsigned cap_ex = create_ec_helper(reinterpret_cast<unsigned>(this));

    // create portals for exceptions
    for (unsigned i=0; i < 32; i++)
      if (i!=14 && i!=9) check(create_pt(i, cap_ex, MTD_ALL, got_exception));
    
    // create worker
    debug_ec_name("work", 0);   
    unsigned cap_worker = create_ec_helper(reinterpret_cast<unsigned>(this));


    // initialize feature specific variables
    unsigned features =  (Cpu::has_svm() << 1) | Cpu::has_vmx();

    // create portals for VCPU faults

#undef VM_FUNC
#define VM_FUNC(NR, NAME, INPUT, CODE) {NR, NAME, INPUT},
    struct vm_caps {
      unsigned nr;
      void __attribute__((regparm(0))) (*func)(Utcb *);
      unsigned mtd;
    } vm_caps[] = {
#undef EXCP_FUNC
#define EXCP_FUNC(NR, CODE)
#include "vmx_funcs.h"
    };
    for (unsigned i=0; i < sizeof(vm_caps)/sizeof(vm_caps[0]); i++)
      {
	if (!(vm_caps[i].nr >> 8) || (vm_caps[i].nr >> 8) == features)
	  {
	    Logging::printf("create pt %x features %x\n", vm_caps[i].nr, features);
	    check(create_pt(vm_caps[i].nr & 0xff, cap_worker, vm_caps[i].mtd, vm_caps[i].func));
	  }
      }
    return 0;
  }


  static void instruction_emulation(Utcb *utcb)
  {
    if (_debug)
      Logging::printf("execute %s at %x:%x pid %d cr3 %x inj_info %x hazard %x\n", __func__, utcb->cs.sel, utcb->eip, utcb->head.pid, 
		      utcb->cr3, utcb->inj_info, _mb->vcpustate(0)->hazard);
    utcb->head.pid = 33;
    do {
      COUNTER_SET("iv pid",  utcb->head.pid);
      COUNTER_SET("iv eip",  utcb->eip);
      //Logging::printf("execute: eip %x:%x esp %x:%x+%x eax %x ebx %x ecx %x edx %x edi %x esi %x\n", utcb->cs.sel, utcb->eip, utcb->ss.sel, utcb->esp, utcb->ss.base, utcb->eax, utcb->ebx, utcb->ecx, utcb->edx, utcb->edi, utcb->esi);
      
      if (!execute_all(static_cast<CpuState*>(utcb), _mb->vcpustate(0)))
	Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, utcb->cs.sel, utcb->eip, utcb->head.pid);
      do_recall(utcb);
    }
    while (utcb->head.pid);
	
    //Logging::printf("executed %s at %x:%x pid %d cr3 %x intr %x\n", __func__, utcb->cs.sel, utcb->eip, utcb->head.pid, utcb->cr3, utcb->inj_info);
    if (~_mb->vcpustate(0)->hazard & VirtualCpuState::HAZARD_CRWRITE) 
      utcb->head.mtr =  mtd_untyped(utcb->head.mtr) & ~MTD_CR;
    else
      Cpu::atomic_and<volatile unsigned>(&_mb->vcpustate(0)->hazard, ~VirtualCpuState::HAZARD_CRWRITE);
  }	


  static bool execute_all(CpuState *cpu, VirtualCpuState *vcpu)
  {
    SemaphoreGuard l(_lock);
    MessageExecutor msg(cpu, vcpu);
    return _mb->bus_executor.send(msg, true, cpu->head.pid);
  }

  static void skip_instruction(Utcb *utcb)
  {
    assert(mtd_untyped(utcb->head.mtr) & MTD_EIP);
    assert(mtd_untyped(utcb->head.mtr) & MTD_STA);
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
	if ((utcb->qual[0] >> 16) == 0x80) COUNTER_INC("out(0x80)");
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


public:

  bool  receive(MessageIrq &msg)
  { 
    if (msg.line == MessageIrq::LINT0)
      {
	for (unsigned i=0; i < Motherboard::MAX_CPUS; i++)
	  if (msg.type == MessageIrq::ASSERT_IRQ)
	    {
	      if (~_mb->vcpustate(i)->hazard & VirtualCpuState::HAZARD_IRQ)
		{
		  Cpu::atomic_or<volatile unsigned>(&_mb->vcpustate(i)->hazard, VirtualCpuState::HAZARD_IRQ);
		  //Logging::printf("do wakeup hz %x\n", _mb->vcpustate(i)->hazard);
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
	for (unsigned i=0; i < Motherboard::MAX_CPUS; i++)
	  {
	    _mb->vcpustate(i)->hazard |= VirtualCpuState::HAZARD_INIT;
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
      case MessageHostOp::OP_ALLOC_IOMEM_REGION:
	msg.ptr = reinterpret_cast<char *>(Sigma0Base::request_io(msg.value & ~0x1f, 1 << (msg.value & 0x1f), false, _iomem_start));
	res = msg.value;
	break;
      case MessageHostOp::OP_ATTACH_HOSTIRQ:
	res = create_irq_thread(msg.value & 0xff, do_gsi);
	break;
      case MessageHostOp::OP_GUEST_MEM:
	if (msg.value >= _physsize)
	  msg.value = 0;
	else
	  {
	    extern char __guestmem;
	    msg.len    = _physsize - msg.value;
	    msg.ptr    = &__guestmem + msg.value;
	  }
	break;
      case MessageHostOp::OP_GET_MODULE:
	res          = Sigma0Base::hostop(msg);
	break;
      case MessageHostOp::OP_GET_UID:
	res = Sigma0Base::hostop(msg);
	break;
      case MessageHostOp::OP_UNMASK_IRQ:
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
  bool  receive(MessageConsole &msg) {  return Sigma0Base::console(msg); }
  bool  receive(MessageNetwork &msg)
  {
    //Logging::printf("forward %p\n", msg.buffer);
    if (_forward_pkt == msg.buffer) return false;
    Sigma0Base::network(msg);
    return true;
  }
  

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
    if (reprogram) {
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
      case MessageTimer::TIMER_CANCEL_TIMEOUT:
	res = _timeouts.cancel(msg.nr);
	break;
      case MessageTimer::TIMER_REQUEST_TIMEOUT:
	res = _timeouts.request(msg.nr, msg.abstime);
	break;
      default:
	return false;
      }
    static unsigned c;
    static timevalue old;
    unsigned diff = msg.abstime - old;
    //if (!(c++ & 0x1f))  Logging::printf("%s: res %x abstime %lld diff %d\n", __PRETTY_FUNCTION__, msg.res, msg.abstime, diff / 2666);
    old = msg.abstime;
    if (res == 0 )
      {
	// update timeout in sigma0
	MessageTimer msg2(0, _timeouts.timeout());
	Sigma0Base::timer(msg2);
      }
    return true;
  }

  bool  receive(MessageTime &msg) {  return Sigma0Base::time(msg);  }

public:
  NOVA_NORETURN void run(Hip *hip)
  {    
    console_init("VMM");    
    assert(hip);
    unsigned res;
    if ((res = init(hip))) Logging::panic("init failed with %x", res);

    char *args = reinterpret_cast<char *>(hip_module(hip, 0)->aux);
    Logging::printf("Vancouver: hip %p utcb %p args '%s'\n", hip, _boot_utcb, args);

    extern char __guestmem;
    _physmem = reinterpret_cast<unsigned long>(&__guestmem);
    _physsize = 0;
    
    // get physsize
    for (int i=0; i < (hip->length - hip->mem_offset) / hip->mem_desc_size; i++)
      {
	HipMem *hmem = reinterpret_cast<HipMem *>(reinterpret_cast<char *>(hip) + hip->mem_offset) + i;
	if (hmem->type == 1 && hmem->address <= _physmem)
	  {
	    _physsize = hmem->size - (_physmem - hmem->address);
	    _iomem_start = hmem->address + hmem->size;
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
    debug_ec_name("vcpu", 0);
    _mb->vcpustate(0)->block_sem = new Semaphore(&_mb->vcpustate(0)->block_count, _cap_free++);
    if (create_sm(_mb->vcpustate(0)->block_sem->sm(), 0))
      Logging::panic("could not create blocking semaphore\n");
    _mb->vcpustate(0)->cap_vcpu = _cap_free++;
    if (create_ec(_mb->vcpustate(0)->cap_vcpu, 0, 0) || create_sc(_cap_free++, _mb->vcpustate(0)->cap_vcpu, qpd(1, 100000)))
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

ASMFUNCS(Vancouver);
