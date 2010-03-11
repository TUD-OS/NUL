/*
 * VMX+SVM handler functions.
 *
 * Copyright (C) 2008-2010, Bernhard Kauer <bk@vmmon.org>
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

// the VMX portals follow
VM_FUNC(PT_VMX + 2,  vmx_triple, MTD_ALL,
	{
	  utcb->head._pid = 2;
	  if (!execute_all(static_cast<CpuState*>(utcb), _mb->vcpustate(0)))
	    Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, utcb->cs.sel, utcb->eip, pid);
	  do_recall(pid, utcb);
	}
	)
VM_FUNC(PT_VMX +  3,  vmx_init, MTD_ALL,
	Logging::printf("%s() mtr %x rip %x ilen %x cr0 %x efl %x\n", __func__, utcb->head.mtr.value(), utcb->eip, utcb->inst_len, utcb->cr0, utcb->efl);
	utcb->head._pid = 3;
	if (!execute_all(static_cast<CpuState*>(utcb), _mb->vcpustate(0)))
	  Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, utcb->cs.sel, utcb->eip, pid);
	Logging::printf("%s() mtr %x rip %x ilen %x cr0 %x efl %x hz %x\n", __func__, utcb->head.mtr.value(), utcb->eip, utcb->inst_len, utcb->cr0, utcb->efl, _mb->vcpustate(0)->hazard);
	//do_recall(pid, utcb);
	)
VM_FUNC(PT_VMX +  7,  vmx_irqwin, MTD_IRQ,
	COUNTER_INC("irqwin");
	do_recall(pid, utcb);
	)
VM_FUNC(PT_VMX + 10,  vmx_cpuid, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_IRQ,
	COUNTER_INC("cpuid");
	VCpu * vcpu= reinterpret_cast<VCpu*>(utcb->head.tls);
	CpuMessage msg(CpuMessage::TYPE_CPUID, utcb);
	if (!vcpu->executor.send(msg, true, msg.type))
	  Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, utcb->cs.sel, utcb->eip, pid);
	skip_instruction(utcb);
	do_recall(pid, utcb);
	)
VM_FUNC(PT_VMX + 12,  vmx_hlt, MTD_RIP_LEN | MTD_IRQ,
	COUNTER_INC("hlt");
	skip_instruction(utcb);

	// wait for irq
	Cpu::atomic_or<volatile unsigned>(&_mb->vcpustate(0)->hazard, VirtualCpuState::HAZARD_INHLT);
	if (~_mb->vcpustate(0)->hazard & VirtualCpuState::HAZARD_IRQ)  _mb->vcpustate(0)->block_sem->down();
	Cpu::atomic_and<volatile unsigned>(&_mb->vcpustate(0)->hazard, ~VirtualCpuState::HAZARD_INHLT);
	do_recall(pid, utcb);
	)

VM_FUNC(PT_VMX + 30,  vmx_ioio, MTD_RIP_LEN | MTD_QUAL | MTD_GPR_ACDB | MTD_RFLAGS | MTD_STATE,
	//if (_debug) Logging::printf("guest ioio at %x port %llx len %x\n", utcb->eip, utcb->qual[0], utcb->inst_len);
	if (utcb->qual[0] & 0x10)
	  {
	    COUNTER_INC("IOS");
	    force_invalid_gueststate_intel(utcb);
	  }
	else
	  {
	    unsigned order = utcb->qual[0] & 7;
	    if (order > 2)  order = 2;
	    ioio_helper(utcb, utcb->qual[0] & 8, order);
	  }
	)

VM_FUNC(PT_VMX + 31,  vmx_rdmsr, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_TSC | MTD_SYSENTER,
	utcb->head._pid = 31;
	COUNTER_INC("rdmsr");
	if (!execute_all(static_cast<CpuState*>(utcb), _mb->vcpustate(0)))
	  Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, utcb->cs.sel, utcb->eip, pid);
	)
VM_FUNC(PT_VMX + 32,  vmx_wrmsr, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_TSC | MTD_SYSENTER,
	utcb->head._pid = 32;
	COUNTER_INC("wrmsr");
	if (!execute_all(static_cast<CpuState*>(utcb), _mb->vcpustate(0)))
	  Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, utcb->cs.sel, utcb->eip, pid);
	)

VM_FUNC(PT_VMX + 33,  vmx_invalid, MTD_ALL,
	{
	  utcb->efl |= 2;
	  instruction_emulation(pid, utcb, true);
	  if (_mb->vcpustate(0)->hazard & VirtualCpuState::HAZARD_CTRL)
	    {
	      Cpu::atomic_and<volatile unsigned>(&_mb->vcpustate(0)->hazard, ~VirtualCpuState::HAZARD_CTRL);
	      utcb->head.mtr =  Mtd(utcb->head.mtr.untyped() | MTD_CTRL, 0);
	      utcb->ctrl[0] = 1 << 3; // tscoffs
	      utcb->ctrl[1] = 0;
	    }
	  do_recall(pid, utcb);
	})
VM_FUNC(PT_VMX + 48,  vmx_mmio, MTD_ALL,
	COUNTER_INC("MMIO");
	/**
	 * Idea: optimize the default case - mmio to general purpose register
	 * Need state: GPR_ACDB, GPR_BSD, RIP_LEN, RFLAGS, CS, DS, SS, ES, RSP, CR, EFER
	 */
	// make sure we do not inject the #PF!
	utcb->inj_info = ~0x80000000;
	if (!map_memory_helper(utcb))
	  {
	    // this is an access to MMIO
	    instruction_emulation(pid, utcb, false);
	    do_recall(pid, utcb);
	  }
	)
VM_FUNC(PT_VMX + 0xfe,  vmx_startup, 0,  vmx_triple(pid, utcb); )
VM_FUNC(PT_VMX + 0xff,  do_recall, MTD_IRQ,
	if (_mb->vcpustate(0)->hazard & VirtualCpuState::HAZARD_INIT)
	  vmx_init(pid, utcb);
	else
	  {
	    SemaphoreGuard l(_lock);
	    COUNTER_INC("recall");
#if 0
	    COUNTER_SET("rEIP", utcb->eip);
	    COUNTER_SET("rESP", utcb->esp);
	    COUNTER_SET("rCR3", utcb->cr3);
	    COUNTER_SET("HZ", _mb->vcpustate(0)->hazard);
#endif
	    unsigned lastpid = utcb->head._pid;
	    utcb->head._pid = 1;
	    MessageExecutor msg(static_cast<CpuState*>(utcb), _mb->vcpustate(0));
	    _mb->bus_executor.send(msg, true, utcb->head._pid);
	    utcb->head._pid = lastpid;
	  }
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
	      ioio_helper(utcb, utcb->qual[0] & 1, order);
	    }
	}
	)
VM_FUNC(PT_SVM + 0x7c,  svm_msr,     MTD_ALL, svm_invalid(pid, utcb); )
VM_FUNC(PT_SVM + 0x7f,  svm_shutdwn, MTD_ALL, vmx_triple(pid, utcb); )
VM_FUNC(PT_SVM + 0xfc,  svm_npt,     MTD_ALL,
	// make sure we do not inject the #PF!
	utcb->inj_info = ~0x80000000;
	if (!map_memory_helper(utcb))
	  svm_invalid(pid, utcb);
	)
VM_FUNC(PT_SVM + 0xfd, svm_invalid, MTD_ALL,
	COUNTER_INC("invalid");
	if (_mb->vcpustate(0)->hazard & ~1) Logging::printf("invalid %x\n", _mb->vcpustate(0)->hazard);
	instruction_emulation(pid, utcb, true);
	if (_mb->vcpustate(0)->hazard & VirtualCpuState::HAZARD_CTRL)
	  {
	    COUNTER_INC("ctrl");
	    Cpu::atomic_and<volatile unsigned>(&_mb->vcpustate(0)->hazard, ~VirtualCpuState::HAZARD_CTRL);
	    utcb->head.mtr =  Mtd(utcb->head.mtr.untyped() | MTD_CTRL, 0);
	    utcb->ctrl[0] = 1 << 18; // cpuid
	    utcb->ctrl[1] = 1 << 0;  // vmrun
	  }
	do_recall(pid, utcb);
	)
VM_FUNC(PT_SVM + 0xfe,  svm_startup,MTD_ALL,  svm_shutdwn(pid, utcb);)
VM_FUNC(PT_SVM + 0xff,  svm_recall, MTD_IRQ,  do_recall(pid, utcb); )
