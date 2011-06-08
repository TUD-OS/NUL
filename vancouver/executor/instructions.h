/** @file
 * Instruction helper.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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
int helper_SYSENTER()
{
  if (!_cpu->pm() || !(_cpu->sysenter_cs & 0xfffc)) GP0;
  _cpu->efl &= ~(EFL_VM | EFL_IF | EFL_RF);
  _cpu->cs.set(_cpu->sysenter_cs + 0, 0, 0xffffffff, 0xc9b);
  _cpu->ss.set(_cpu->sysenter_cs + 8, 0, 0xffffffff, 0xc93);
  _cpu->esp = _cpu->sysenter_esp;
  _cpu->eip = _cpu->sysenter_eip;
  return _fault;
}

/**
 * sysexit.
 * state: stable
 * doc: intel
 */
int helper_SYSEXIT()
{
  if (!_cpu->pm() || !(_cpu->sysenter_cs & 0xfffc) || _cpu->cpl()) GP0;
  _cpu->cs.set((_cpu->sysenter_cs + 16) | 3, 0, 0xffffffff, 0xcfb);
  _cpu->ss.set((_cpu->sysenter_cs + 24) | 3, 0, 0xffffffff, 0xcf3);
  _cpu->esp = _cpu->ecx;
  _cpu->eip = _cpu->edx;
  return _fault;
}

/**
 * cli.
 * state: stable
 * doc: intel, amd
 */
int helper_CLI () {
  if (_cpu->cpl() <= _cpu->iopl())
    _cpu->efl &= ~EFL_IF;
  else if (_cpu->v86() && (_cpu->cr4 & 1) || (!_cpu->v86() && _cpu->pm() && _cpu->cpl() == 3 && (_cpu->cr4 & 2)))
    _cpu->efl &= ~EFL_VIF;
  else
    GP0;
  return _fault;
}

/**
 * sti.
 * state: stable
 * doc: intel, amd
 */
int helper_STI() {
  if (_cpu->cpl() <= _cpu->iopl()) {
    // add irq shadow
    if (~_cpu->efl & EFL_IF) _cpu->intr_state |= 1;
    _cpu->efl |= EFL_IF;
  }
  else if (_cpu->v86() && (_cpu->cr4 & 1) || (!_cpu->v86() && _cpu->pm() && _cpu->cpl() == 3 && (_cpu->cr4 & 2)))
    {
      if (_cpu->efl & EFL_VIP) GP0;
      _cpu->efl |= EFL_VIF;
    }
  else
    GP0;
  return _fault;
}

/**
 * lea.
 * state: stable
 * doc: intel
 */
template<unsigned operand_size>
void __attribute__((regparm(3)))  helper_LEA()
{
  unsigned *tmp_dst = get_reg32((_entry->data[_entry->offset_opcode] >> 3) & 0x7);
  unsigned  virt = modrm2virt();
  move<operand_size>(tmp_dst, &virt);
}


/**
 * lds, les, lfs, lgs, lss.
 * state: testing
 * doc: intel
 */
template<unsigned operand_size>
int helper_loadsegment(CpuState::Descriptor *desc)
{
  void *addr;
  unsigned short sel;
  if (modrm2mem(addr, 2 + (1 << operand_size), TYPE_R)) return _fault;
  move<1>(&sel, reinterpret_cast<char *>(addr) + (1 << operand_size));

  if (!set_segment(desc, sel))
    move<operand_size>(get_reg32((_entry->data[_entry->offset_opcode] >> 3) & 0x7), addr);
  return _fault;
}
