/**
 * Instruction helper.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */


/**
 * sysenter.
 * state: stable
 * doc: intel
 */
static int helper_SYSENTER(MessageExecutor &msg)
{
  if (!msg.cpu->pm() || !(msg.cpu->sysenter_cs & 0xfffc)) GP0;
  msg.cpu->efl &= ~(EFL_VM | EFL_IF | EFL_RF);
  msg.cpu->cs.set(msg.cpu->sysenter_cs + 0, 0, 0xffffffff, 0xc9b);
  msg.cpu->ss.set(msg.cpu->sysenter_cs + 8, 0, 0xffffffff, 0xc93);
  msg.cpu->esp = msg.cpu->sysenter_esp;
  msg.cpu->eip = msg.cpu->sysenter_eip;
  return msg.vcpu->fault;
}

/**
 * sysexit.
 * state: stable
 * doc: intel
 */
static int helper_SYSEXIT(MessageExecutor &msg)
{
  if (!msg.cpu->pm() || !(msg.cpu->sysenter_cs & 0xfffc) || msg.cpu->cpl()) GP0;
  msg.cpu->cs.set((msg.cpu->sysenter_cs + 16) | 3, 0, 0xffffffff, 0xcfb);
  msg.cpu->ss.set((msg.cpu->sysenter_cs + 24) | 3, 0, 0xffffffff, 0xcf3);
  msg.cpu->esp = msg.cpu->ecx;
  msg.cpu->eip = msg.cpu->edx;
  return msg.vcpu->fault;
}

/**
 * cli.
 * state: stable
 * doc: intel, amd
 */
static int helper_CLI (MessageExecutor &msg) {
  if (msg.cpu->cpl() <= msg.cpu->iopl())
    msg.cpu->efl &= ~EFL_IF;
  else if (msg.cpu->v86() && (msg.cpu->cr4 & 1) || (!msg.cpu->v86() && msg.cpu->pm() && msg.cpu->cpl() == 3 && (msg.cpu->cr4 & 2)))
    msg.cpu->efl &= ~EFL_VIF;
  else
    GP0;
  return msg.vcpu->fault;
}

/**
 * sti.
 * state: stable
 * doc: intel, amd
 */
static int helper_STI(MessageExecutor &msg) {
  if (msg.cpu->cpl() <= msg.cpu->iopl()) {
    // add irq shadow
    if (~msg.cpu->efl & EFL_IF) msg.cpu->intr_state |= 1;
    msg.cpu->efl |= EFL_IF;
  }
  else if (msg.cpu->v86() && (msg.cpu->cr4 & 1) || (!msg.cpu->v86() && msg.cpu->pm() && msg.cpu->cpl() == 3 && (msg.cpu->cr4 & 2)))
    {
      if (msg.cpu->efl & EFL_VIP) GP0;
      msg.cpu->efl |= EFL_VIF;
    }
  else
    GP0;
  return msg.vcpu->fault;
}

/**
 * lea.
 * state: stable
 * doc: intel
 */
template<unsigned operand_size>
static void __attribute__((regparm(3)))  helper_LEA(MessageExecutor &msg, InstructionCacheEntry *entry)
{
  unsigned *tmp_dst = get_reg32(msg, (entry->data[entry->offset_opcode] >> 3) & 0x7);
  unsigned  virt = modrm2virt(msg, entry);
  move<operand_size>(tmp_dst, &virt);
}


/**
 * lds, les, lfs, lgs, lss.
 * state: testing
 * doc: intel
 */
template<unsigned operand_size>
static int helper_loadsegment(MessageExecutor &msg, CpuState::Descriptor *desc, InstructionCacheEntry *entry)
{
  void *addr;
  unsigned short sel;
  if (modrm2mem(msg, entry, addr, 2 + (1 << operand_size), TYPE_R)) return msg.vcpu->fault;
  move<1>(&sel, reinterpret_cast<char *>(addr) + (1 << operand_size));

  if (!set_segment(msg, desc, sel))
    move<operand_size>(get_reg32(msg, (entry->data[entry->offset_opcode] >> 3) & 0x7), addr);
  return msg.vcpu->fault;
}
