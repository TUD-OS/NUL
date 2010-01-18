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
	  utcb->head.pid = 2;
	  if (!execute_all(static_cast<CpuState*>(utcb), _mb->vcpustate(0)))
	    Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, utcb->cs.sel, utcb->eip, utcb->head.pid);
	  do_recall(utcb);
	}
	)
VM_FUNC(PT_VMX +  3,  vmx_init, MTD_ALL,
	Logging::printf("%s() mtr %x rip %x ilen %x cr0 %x efl %x\n", __func__, utcb->head.mtr.value(), utcb->eip, utcb->inst_len, utcb->cr0, utcb->efl);
	utcb->head.pid = 3;
	if (!execute_all(static_cast<CpuState*>(utcb), _mb->vcpustate(0)))
	  Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, utcb->cs.sel, utcb->eip, utcb->head.pid);
	Logging::printf("%s() mtr %x rip %x ilen %x cr0 %x efl %x hz %x\n", __func__, utcb->head.mtr.value(), utcb->eip, utcb->inst_len, utcb->cr0, utcb->efl, _mb->vcpustate(0)->hazard);
	//do_recall(utcb);
	)
VM_FUNC(PT_VMX +  7,  vmx_irqwin, MTD_IRQ,
	COUNTER_INC("irqwin");
	do_recall(utcb);
	)
VM_FUNC(PT_VMX + 10,  vmx_cpuid, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_STATE,
	if (!handle_special_cpuid(utcb))
	  {
	    utcb->head.pid = 10;
	    COUNTER_INC("cpuid");
	    if (!execute_all(static_cast<CpuState*>(utcb), _mb->vcpustate(0)))
	      Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, utcb->cs.sel, utcb->eip, utcb->head.pid);
	    // XXX call skip instruction for MTD_STATE update
	  }
	)
VM_FUNC(PT_VMX + 12,  vmx_hlt, MTD_RIP_LEN | MTD_IRQ,
	COUNTER_INC("hlt");
	skip_instruction(utcb);

	// wait for irq
	Cpu::atomic_or<volatile unsigned>(&_mb->vcpustate(0)->hazard, VirtualCpuState::HAZARD_INHLT);
	if (~_mb->vcpustate(0)->hazard & VirtualCpuState::HAZARD_IRQ)  _mb->vcpustate(0)->block_sem->down();
	Cpu::atomic_and<volatile unsigned>(&_mb->vcpustate(0)->hazard, ~VirtualCpuState::HAZARD_INHLT);
	do_recall(utcb);
	)

VM_FUNC(PT_VMX + 30,  vmx_ioio, MTD_RIP_LEN | MTD_QUAL | MTD_GPR_ACDB | MTD_RFLAGS | MTD_STATE,
	if (_debug) Logging::printf("guest ioio at %x port %llx len %x\n", utcb->eip, utcb->qual[0], utcb->inst_len);
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
	utcb->head.pid = 31;
	COUNTER_INC("rdmsr");
	if (!execute_all(static_cast<CpuState*>(utcb), _mb->vcpustate(0)))
	  Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, utcb->cs.sel, utcb->eip, utcb->head.pid);
	)
VM_FUNC(PT_VMX + 32,  vmx_wrmsr, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_TSC | MTD_SYSENTER,
	utcb->head.pid = 32;
	COUNTER_INC("wrmsr");
	if (!execute_all(static_cast<CpuState*>(utcb), _mb->vcpustate(0)))
	  Logging::panic("nobody to execute %s at %x:%x pid %d\n", __func__, utcb->cs.sel, utcb->eip, utcb->head.pid);
	)

VM_FUNC(PT_VMX + 33,  vmx_invalid, MTD_ALL,
	{
	  utcb->efl |= 2;
	  instruction_emulation(utcb);
	  if (_mb->vcpustate(0)->hazard & VirtualCpuState::HAZARD_CTRL)
	    {
	      Cpu::atomic_and<volatile unsigned>(&_mb->vcpustate(0)->hazard, ~VirtualCpuState::HAZARD_CTRL);
	      utcb->head.mtr =  Mtd(utcb->head.mtr.untyped() | MTD_CTRL, 0);
	      utcb->ctrl[0] = 1 << 3; // tscoffs
	      utcb->ctrl[1] = 0;
	    }
	  do_recall(utcb);
	})
VM_FUNC(PT_VMX + 48,  vmx_mmio, MTD_ALL,
	/**
	 * Idea: optimize the default case - mmio to general purpose register
	 * Need state: GPR_ACDB, GPR_BSD, RIP_LEN, RFLAGS, CS, DS, SS, ES, RSP, CR, EFER
	 */
	// make sure we do not inject the #PF!
	utcb->inj_info = ~0x80000000;
	if (!map_memory_helper(utcb))
	  // this is an access to MMIO
	  vmx_invalid(utcb);
	)
VM_FUNC(PT_VMX + 0xfe,  vmx_startup, 0,  vmx_triple(utcb); )
VM_FUNC(PT_VMX + 0xff,  do_recall, MTD_IRQ | MTD_RIP_LEN | MTD_RSP | MTD_CR,
	if (_mb->vcpustate(0)->hazard & VirtualCpuState::HAZARD_INIT)
	  vmx_init(utcb);
	else
	  {
	    SemaphoreGuard l(_lock);
	    COUNTER_INC("recall");
	    COUNTER_SET("rEIP", utcb->eip);
	    COUNTER_SET("rESP", utcb->esp);
	    COUNTER_SET("rCR3", utcb->cr3);
	    COUNTER_SET("HZ", _mb->vcpustate(0)->hazard);
	    unsigned lastpid = utcb->head.pid;
	    utcb->head.pid = 1;
	    MessageExecutor msg(static_cast<CpuState*>(utcb), _mb->vcpustate(0));
	    _mb->bus_executor.send(msg, true, utcb->head.pid);
	    utcb->head.pid = lastpid;
	  }
	)


// and now the SVM portals
VM_FUNC(PT_SVM + 0x64,  svm_vintr,   MTD_IRQ, vmx_irqwin(utcb); )
VM_FUNC(PT_SVM + 0x72,  svm_cpuid,   MTD_ALL, if (!handle_special_cpuid(utcb)) svm_invalid(utcb); )
VM_FUNC(PT_SVM + 0x78,  svm_hlt,     MTD_RIP_LEN | MTD_IRQ,  utcb->inst_len = 1; vmx_hlt(utcb); )
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
VM_FUNC(PT_SVM + 0x7c,  svm_msr,     MTD_ALL, svm_invalid(utcb); )
VM_FUNC(PT_SVM + 0x7f,  svm_shutdwn, MTD_ALL, vmx_triple(utcb); )
VM_FUNC(PT_SVM + 0xfc,  svm_npt,     MTD_ALL,
	// make sure we do not inject the #PF!
	utcb->inj_info = ~0x80000000;
	if (!map_memory_helper(utcb))
	  svm_invalid(utcb);
	)
VM_FUNC(PT_SVM + 0xfd, svm_invalid, MTD_ALL,
	COUNTER_INC("invalid");
	if (_mb->vcpustate(0)->hazard & ~1) Logging::printf("invalid %x\n", _mb->vcpustate(0)->hazard);
	instruction_emulation(utcb);
	if (_mb->vcpustate(0)->hazard & VirtualCpuState::HAZARD_CTRL)
	  {
	    COUNTER_INC("ctrl");
	    Cpu::atomic_and<volatile unsigned>(&_mb->vcpustate(0)->hazard, ~VirtualCpuState::HAZARD_CTRL);
	    utcb->head.mtr =  Mtd(utcb->head.mtr.untyped() | MTD_CTRL, 0);
	    utcb->ctrl[0] = 1 << 18; // cpuid
	    utcb->ctrl[1] = 1 << 0;  // vmrun
	  }
	do_recall(utcb);
	)
VM_FUNC(PT_SVM + 0xfe,  svm_startup,MTD_ALL,  svm_shutdwn(utcb);)
VM_FUNC(PT_SVM + 0xff,  svm_recall, MTD_IRQ,  do_recall(utcb); )
