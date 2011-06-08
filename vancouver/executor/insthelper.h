/** @file
 * Instruction helper.
 *
 * Copyright (C) 2009-2010, Bernhard Kauer <bk@vmmon.org>
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
 * Check whether CPL0 is required.
 */
int cpl0_test() {
  if (_cpu->cpl()) GP0;
  return _fault;
}


  /**
   * Add base and check segment type and limit.
   */
  int handle_segment(CpuState::Descriptor *desc, unsigned &virt, unsigned length, bool write, bool stackop)
  {
    // align address
    if (!_entry || stackop)
      {
	assert(desc == &_cpu->ss);
	unsigned stack_size = ((_cpu->ss.ar >> 10) & 1) + 1;
	if (stack_size == 1) virt &= 0xffff;
      }
    else
      if (_entry->address_size == 1) virt &= 0xffff;
    if (!(~desc->limit) && !desc->base && desc->ar == 0xc93) return 0;

    // limit check
    bool fault = virt > desc->limit ||  (virt + length - 1) > desc->limit;

    // expand down?
    if ((desc->ar & 0xc) == 4) fault = !fault;

    // overflow on non-big expand down segment?
    if ((desc->ar & 0x40c) == 4)  fault |= (0xffff - length) < virt;

    // rights check: present, readonly and exec-only segment
    fault |= ~desc->ar & 0x80 || !write && ((desc->ar & 0xa) == 0x8) || write && (desc->ar & 0xa) != 0x2;
    if (fault)
      {
	Logging::printf("%s() #%x %x+%d desc %x limit %x base %x esp %x\n", __func__, desc - &_cpu->es, virt, length, desc->ar, desc->limit, desc->base, _cpu->esp);
	if (desc == &_cpu->ss) {  SS0; } else { GP0; }
      }

    // add segment base
    virt += desc->base;
    return _fault;
  }

  template<unsigned operand_size>
  int logical_mem(CpuState::Descriptor *desc, unsigned virt, bool write, void *&res, bool stackop=false)
  {
    handle_segment(desc, virt, 1 << operand_size, write, stackop)
      || prepare_virtual(virt, 1 << operand_size, user_access(write ? TYPE_W : TYPE_R) , res);
    return _fault;
  }

static void move(void * tmp_dst, void *tmp_src, unsigned order) {  Cpu::move(tmp_dst, tmp_src, order); }
template<unsigned operand_size> static void move(void *tmp_dst, void *tmp_src) { Cpu::move<operand_size>(tmp_dst, tmp_src); }

/**
 * Move
 */
#define MOVE2(operand_size, DST, SRC) {					\
    if (operand_size == 0)  DST = (DST & ~0xff)   | (SRC & 0xff);	\
    if (operand_size == 1)  DST = (DST & ~0xffff) | (SRC & 0xffff);	\
    if (operand_size == 2)  DST = SRC;					\
  }

  /**
   * Perform an absolute JMP.
   */
  template<unsigned operand_size>
  int helper_JMP_absolute(unsigned nrip)
  {
    if (operand_size == 1)  nrip &= 0xffff;
    unsigned limit = READ(cs).limit;
    if (~limit && limit < nrip)  GP0;
    _cpu->eip = nrip;
    return 0;
  }

  template<unsigned operand_size>
  static int __attribute__((regparm(3))) helper_JMP_static(InstructionCache *cache, void *tmp_src)
  { return cache->helper_JMP<operand_size>(tmp_src); }

  /**
   * Do an unconditional JMP.
   */
  template<unsigned operand_size>
  int __attribute__((regparm(3))) helper_JMP(void *tmp_src)
  {
    unsigned nrip = 0;
    if (_entry->flags & IC_MODRM)
      move<operand_size>(&nrip, tmp_src);
    else
      nrip = _cpu->eip + *reinterpret_cast<int *>(tmp_src);
    return helper_JMP_absolute<operand_size>(nrip);
  }

int helper_HLT ()    { return send_message(CpuMessage::TYPE_HLT); }
int helper_WBINVD () { return send_message(CpuMessage::TYPE_WBINVD); }
int helper_INVD ()   { return send_message(CpuMessage::TYPE_INVD); }
int helper_CLTS()    { _cpu->cr0 &= ~(1<<3); return _fault; }
int helper_INT3() {  _oeip = _cpu->eip; _mtr_out |= MTD_RIP_LEN; return _fault = 0x80000603; }
int helper_UD2A() {  return _fault = 0x80000606; }
int helper_INTO() {  _oeip = _cpu->eip; _mtr_out |= MTD_RIP_LEN; if (_cpu->efl & EFL_OF) _fault = 0x80000604; return _fault; }


  /**
   * Push a stackframe on the stack.
   */
  template<unsigned operand_size>
  int __attribute__((regparm(3))) __attribute__((noinline))  helper_PUSH(void *tmp_src)
  {
    void *res;
    unsigned length = 1 << operand_size;
    unsigned virt = _cpu->esp - length;
    if (!logical_mem<operand_size>(&_cpu->ss, virt, true, res, true))
      move<operand_size>(res, tmp_src);
    _cpu->esp -= length;
    return _fault;
  }


  /**
   * Pop a stackframe from the stack.
   */
  template<unsigned operand_size>
  int __attribute__((regparm(3)))  __attribute__((noinline)) helper_POP(void *tmp_dst)
  {
    void *res;
    unsigned virt = _cpu->esp;
    if (!logical_mem<operand_size>(&_cpu->ss, virt, false, res, true))
      move<operand_size>(tmp_dst, res);
    _cpu->esp += 1 << operand_size;
    return _fault;
  }


  /**
   * Return from a function.
   */
  template<unsigned operand_size>
  void __attribute__((regparm(3)))  helper_RET(void *tmp_src)
  {
    unsigned tmp_eip;
    helper_POP<operand_size>(&tmp_eip) || helper_JMP_absolute<operand_size>(tmp_eip);
    if (tmp_src)  _cpu->esp += *reinterpret_cast<unsigned short *>(tmp_src);
  }


  /**
   * FarReturn from a function.
   */
  template<unsigned operand_size>
  void __attribute__((regparm(3)))  helper_LRET(void *tmp_src)
  {
    unsigned tmp_eip = 0, tmp_cs = 0;
    helper_POP<operand_size>(&tmp_eip) || helper_POP<operand_size>(&tmp_cs) || helper_far_jmp(tmp_cs, tmp_eip, _cpu->efl);
    if (tmp_src)  _cpu->esp += *reinterpret_cast<unsigned short *>(tmp_src);
  }


  /**
   * Leave a function.
   */
  template<unsigned operand_size>
  void __attribute__((regparm(3)))  helper_LEAVE()
  {
    unsigned stack_size = ((_cpu->ss.ar >> 10) & 1) + 1;
    move(&_cpu->esp, &_cpu->ebp, stack_size);
    helper_POP<operand_size>(&_cpu->ebp);
  }

  /**
   * Implement LOOPNE, LOOPE, LOOP, and JECXZ
   */
#define helper_LOOPS(NAME, X)						\
  template<unsigned operand_size>					\
  void __attribute__((regparm(3))) __attribute__((noinline))		\
  helper_##NAME(void *tmp_src)						\
  {									\
    unsigned ecx = 0;							\
    MOVE2(operand_size, ecx, _cpu->ecx);				\
    if (X != 3) --ecx;							\
    if ((ecx && (X==0 || (X==1 && _cpu->efl & 0x40) || (X==2 && ~_cpu->efl & 0x40))) || (!ecx && X == 3)) \
      if (helper_JMP<operand_size>(tmp_src))				\
	return;								\
    MOVE2(operand_size, _cpu->ecx, ecx);				\
  }
helper_LOOPS(LOOP, 0)
helper_LOOPS(LOOPE, 1)
helper_LOOPS(LOOPNE, 2)
helper_LOOPS(JECXZ, 3)
#undef helper_LOOPS

  /**
   * Call a function.
   */
  template<unsigned operand_size>
  void __attribute__((regparm(3))) __attribute__((noinline)) helper_CALL(void *tmp_src)
  {
    helper_PUSH<operand_size>(&_cpu->eip) || helper_JMP<operand_size>(tmp_src);
  }



  /**
   * LGDT and LIDT helper.
   */
#define helper_LDT(NAME, VAR, MTD)						\
  template<unsigned operand_size>					\
  void __attribute__((regparm(3)))					\
  helper_##NAME()							\
    {									\
    void *addr;								\
    if (!modrm2mem(addr, 6, user_access(TYPE_R)))		\
      {									\
	unsigned base;							\
	move<1>(&_cpu->VAR.limit, addr);				\
	move<2>(&base, reinterpret_cast<char *>(addr)+2);		\
	if (operand_size == 1) base &= 0x00ffffff;			\
	_cpu->VAR.base = base;						\
	_mtr_out |= MTD;						\
      }									\
    }
helper_LDT(LIDT, id, MTD_IDTR)
helper_LDT(LGDT, gd, MTD_GDTR)
#undef helper_LDT

  /**
   * SGDT and SIDT helper.
   */
#define helper_SDT(NAME, VAR, MTD)					\
  template<unsigned operand_size>					\
  void __attribute__((regparm(3)))					\
  helper_##NAME()							\
  {									\
    _mtr_in |= MTD;							\
    void *addr;								\
    if (!modrm2mem(addr, 6, user_access(TYPE_W)))			\
      {									\
	unsigned base = _cpu->VAR.base;					\
	if (operand_size == 1) base &= 0x00ffffff;			\
	move<1>(addr, &_cpu->VAR.limit);				\
	move<2>(reinterpret_cast<char *>(addr)+2, &base);		\
      }									\
  }
helper_SDT(SIDT,id, MTD_IDTR)
helper_SDT(SGDT,gd, MTD_GDTR)
#undef helper_SDT


  template<unsigned operand_size>
  int __attribute__((regparm(3)))  helper_POPF()
  {
    unsigned long tmp = READ(efl);
    if (_cpu->v86() && _cpu->iopl() < 3)
      GP0
    else if (!helper_POP<operand_size>(&tmp))
      {
	// clear VIP+VIF
	if (!_cpu->v86()) tmp &= ~0x180000;

	// reserved bits and RF+VM are unaffected
	unsigned mask = 0x3f7fd5L & ~0x30000;
	// iopl and IF also if not running with CPL==0
	if (_cpu->cpl()) mask &= ~0x3200;
	tmp = READ(efl) & ~mask | tmp & mask;
	WRITE(efl);
	MOVE2(operand_size, _cpu->efl, tmp);
      }
    return _fault;
  }


  template<unsigned operand_size>
  int __attribute__((regparm(3)))  helper_PUSHF()
  {
    if (_cpu->v86() && _cpu->iopl() < 3)
      GP0
    else
      {
	unsigned long tmp = READ(efl) & 0xfcffff;
	return helper_PUSH<operand_size>(&tmp);
      }
  }

  template<unsigned operand_size>
  void __attribute__((regparm(3)))  helper_IN(unsigned port, void *dst)
  {
    // XXX check IOPBM
    CpuMessage msg(true, _cpu, operand_size, port, dst, _mtr_in);
    _vcpu->executor.send(msg, true);
  }

  template<unsigned operand_size>
  void __attribute__((regparm(3)))  helper_OUT(unsigned port, void *dst)
  {

    // XXX check IOPBM
    CpuMessage msg(false, _cpu, operand_size, port, dst, _mtr_in);
    _vcpu->executor.send(msg, true);
  }

/**
 * Calc the flags for an operation.
 */
int calc_flags(unsigned operand_size, void *src, void *dst) {
  InstructionCacheEntry entry2;
  entry2.execute = operand_size == 0 ? exec_38_cmp_0 : (operand_size == 1 ? exec_39_cmp_1 : exec_39_cmp_2);
  entry2.flags = IC_SAVEFLAGS;
  InstructionCacheEntry *old = _entry;
  _entry = &entry2;
  call_asm(src, dst);
  _entry = old;
  return _fault;
}


  enum STRING_HELPER_FEATURES
  {
    SH_LOAD_ESI = 1 << 0,
    SH_SAVE_EDI = 1 << 1,
    SH_LOAD_EDI = 1 << 2,
    SH_SAVE_EAX = 1 << 3,
    SH_DOOP_CMP = 1 << 4,
    SH_DOOP_IN  = 1 << 5,
    SH_DOOP_OUT = 1 << 6
  };

#define NCHECK(X)  { if (X) break; }
#define FEATURE(X,Y) { if (feature & (X)) Y; }
  template<unsigned feature, unsigned operand_size>
  int __attribute__((regparm(3)))  string_helper()
  {
    while (_entry->address_size == 1 && _cpu->cx || _entry->address_size == 2 && _cpu->ecx || !(_entry->prefixes & 0xff))
      {
	void *src = &_cpu->eax;
	void *dst = &_cpu->eax;

	FEATURE(SH_LOAD_ESI, NCHECK(logical_mem<operand_size>((&_cpu->es) + ((_entry->prefixes >> 8) & 0xf), _cpu->esi, false, src)));
	FEATURE(SH_LOAD_EDI, NCHECK(logical_mem<operand_size>(&_cpu->es, _cpu->edi, false, dst)));
	FEATURE(SH_DOOP_IN,  helper_IN<operand_size>(_cpu->dx, dst));
	FEATURE(SH_DOOP_OUT, helper_OUT<operand_size>(_cpu->dx, src));
	FEATURE(SH_DOOP_CMP, calc_flags(operand_size, src, dst); );
	FEATURE(SH_SAVE_EDI, NCHECK(logical_mem<operand_size>(&_cpu->es, _cpu->edi, true, dst)));
	FEATURE(SH_SAVE_EDI | SH_SAVE_EAX, move<operand_size>(dst, src));

	int size = 1 << operand_size;
	if (_cpu->efl & 0x400)  size = -size;
	FEATURE(SH_LOAD_ESI,               if (_entry->address_size == 1)  _cpu->si += size; else _cpu->esi += size;);
	FEATURE(SH_LOAD_EDI | SH_SAVE_EDI, if (_entry->address_size == 1)  _cpu->di += size; else _cpu->edi += size;);
	if (!(_entry->prefixes & 0xff)) break;
	if (_entry->address_size == 1)  _cpu->cx--; else _cpu->ecx--;
	FEATURE(SH_DOOP_CMP,  if (((_entry->prefixes & 0xff) == 0xf3)  && (~_cpu->efl & 0x40))  break);
	FEATURE(SH_DOOP_CMP,  if (((_entry->prefixes & 0xff) == 0xf2)  && ( _cpu->efl & 0x40))  break);

	// XXX check for interrupts
      }
    return _fault;
  }


/**
 * Move from control register.
 */
int helper_MOV__CR0__EDX()
{
  unsigned *tmp_src;
  unsigned *tmp_dst = get_reg32( _entry->data[_entry->offset_opcode] & 0x7);
  switch ((_entry->data[_entry->offset_opcode] >> 3) & 0x7)
    {
    case 0: tmp_src = &_cpu->cr0; break;
    case 2: tmp_src = &_cpu->cr2; break;
    case 3: tmp_src = &_cpu->cr3; break;
    case 4: tmp_src = &_cpu->cr4; break;
    default: UD0;
    }
  *tmp_dst = *tmp_src;
  return _fault;
}


/**
 * Move to control register.
 */
int helper_MOV__EDX__CR0()
{
  unsigned *tmp_src = get_reg32(_entry->data[_entry->offset_opcode] & 0x7);
  unsigned *tmp_dst;
  unsigned tmp = *tmp_src;
  // XXX missing invalid transition checks
  // XXX cr0 has no reserved bit checking
  switch ((_entry->data[_entry->offset_opcode] >> 3) & 0x7)
    {
    case 0: if (tmp & 0x1ffaffc0U) GP0;  tmp_dst = &_cpu->cr0; tmp |= 0x10; break;
    case 2: tmp_dst = &_cpu->cr2; break;
    case 3: tmp_dst = &_cpu->cr3; break;
    case 4: if (tmp & 0xffff9800U) GP0;  tmp_dst = &_cpu->cr4; break;
    default: UD0;
    }
  *tmp_dst = tmp;
  _mtr_out |= MTD_CR;

  // XXX flush only if paging-bits change
  // update TLB
  return init();
}

int helper_LMSW(unsigned short value) { _cpu->cr0 = _cpu->cr0 & ~0xeu | value & 0xfu; _mtr_out |= MTD_CR; return _fault; }



struct Descriptor
{
  union
  {
    unsigned values[2];
    struct {
      unsigned short limit0;
      unsigned short base0;
      unsigned char  base1;
      unsigned char    ar0;
      unsigned char    ar1;
      unsigned char   base2;
    };
  };

  unsigned dpl() { return (ar0 >> 5) & 3; }
  unsigned offset() { return (values[1] & 0xffff0000) | (values[0] & ~0xffff0000); }
  unsigned limit()
  {
    unsigned res = limit0 | ((ar1 & 0xf) << 16);
    if (ar1 & 0x80) res = (res << 12) | 0xfff;
    return res;
  }

  void to_cpustate(CpuState::Descriptor *desc, unsigned short selector)
  {
    desc->sel = selector;
    desc->limit = limit();
    desc->base  = base0  | (base1 << 16) | (base2 << 24);
    desc->ar    = ar0 | ((ar1 & 0xf0) << 4);
  }

};


int desc_get_base(unsigned short selector, unsigned long &base, bool ext) {
  unsigned long l;
  if (selector & 0x4)
    {
      l =  _cpu->ld.limit;
      base  = _cpu->ld.base;
    }
  else
    {
      l =  _cpu->gd.limit;
      base = _cpu->gd.base;
    }
  if (selector > (l + 7))  GP(selector | ext);
  base += selector & ~0x7;
  return _fault;
}


int load_gdt_descriptor(Descriptor &desc, unsigned short selector, bool ext) {
  if (!(selector & ~0x3)) GP(ext);
  memset(desc.values, 0, sizeof(desc.values));
  unsigned long base;
  void *res;
  if (!desc_get_base(selector, base, ext) && !prepare_virtual(base, 8, MemTlb::TYPE_R, res))
    memcpy(desc.values, res, 8);
  return _fault;
};


int load_idt_descriptor(Descriptor &desc, unsigned event)
{
  unsigned ofs = (event & 0xff) << 3;
  bool ext = (event & 0x700) <= 0x300;
  unsigned error = ofs | (ext ? 2 : 3);

  if (_cpu->id.limit < (ofs | 7)) GP(error);
  void *res;
  if (!prepare_virtual(_cpu->id.base + ofs, 8, MemTlb::TYPE_R, res)) {
    memcpy(desc.values, res, 8);
    // is it a trap, intr or task-gate?
    if (!(0xce00 & (1<<(desc.ar0 & 0x1f)))) {
      Logging::panic("%s event %x %x base %x limit %x cr0 %x\n", __func__, event, desc.ar0, _cpu->id.base, _cpu->id.limit, _cpu->cr0);
      GP(error);
    }
    // and present?
    if (~desc.ar0 & 0x80) NP(error);
  }
  return _fault;
}


int desc_set_flag(Descriptor &desc, unsigned short selector, unsigned char flag, bool ext) {

  Descriptor desc2 = desc;
  desc.ar0 |= flag;

  unsigned long base;
  void *res;
  if (!desc_get_base(selector, base, ext) && !prepare_virtual(base, 8, MemTlb::TYPE_RMW, res))
    // do a fire and forget cmpxchg here
    asm volatile ("lock; cmpxchg8b (%4)" : "+a"(desc2.values[0]), "+d"(desc2.values[1]) :  "b"(desc.values[0]), "c"(desc.values[1]), "r"(res) : "memory", "cc");
  return _fault;
}




int helper_LTR(unsigned short selector)
{
  if (!_cpu->pm() || _cpu->v86()) UD0;
  if (_cpu->cpl()) GP0;
  if (selector & 0x4) GP(selector & ~0x7);
  selector &= ~0x7;

  Descriptor desc;
  if (!load_gdt_descriptor(desc, selector, false))
    {
      if ((desc.ar0 & 0x1f) != 0x9 && (desc.ar0 & 0x1f) != 0x1) GP(selector);
      if (~desc.ar0 & 0x80)  NP(selector);
      desc_set_flag(desc, selector, 0x2, false);
      desc.to_cpustate(&_cpu->tr, selector);
    }
  return _fault;
}


int helper_LLDT(unsigned short selector)
{
  if (!_cpu->pm() || _cpu->v86()) UD0;
  if (_cpu->cpl()) GP0;
  if (selector & 0x4) GP(selector & ~0x7);
  selector &= ~0x7;
  if (selector)
    {
      Descriptor desc;
      if (!load_gdt_descriptor(desc, selector, false)) {
	if ((desc.ar0 & 0x1f) != 0x2) GP(selector);
	if (~desc.ar0 & 0x80)  NP(selector);
	desc.to_cpustate(&_cpu->ld, selector);
      }
    }
  else
    _cpu->ld.ar = 0x1000;
  _mtr_out |= MTD_LDTR;
  return _fault;
}


static void set_realmode_segment(CpuState::Descriptor *seg, unsigned short sel, bool v86mode) {

  if (v86mode)
    seg->set(sel, sel << 4, 0xffff, 0xf3);
  else
   // there is no limit and attribute modification in realmode
   seg->set(sel, sel << 4, seg->limit, seg->ar);
}


int set_segment(CpuState::Descriptor *seg, unsigned short sel, bool cplcheck = true)
{
  if (!_cpu->pm() || _cpu->v86())
    set_realmode_segment(seg, sel, _cpu->v86());
  else
    {
      bool is_ss = seg == &_cpu->ss;
      unsigned rpl = sel & 0x3;
      Descriptor desc;
      if (!(sel & ~0x3) && !is_ss)
	{
	  seg->sel = sel;
	  seg->ar = 0x1000;
	  return _fault;
	}

      if (!load_gdt_descriptor(desc, sel, false))
	{
	  if ((is_ss && ((rpl != desc.dpl() || cplcheck && desc.dpl() != _cpu->cpl()) || ((desc.ar0 & 0x1a) != 0x12)))
	      || !is_ss && ((((desc.ar0 ^ 0x12) & 0x1a) > 2) || (((desc.ar0 & 0xc) != 0xc) && (rpl > desc.dpl() || cplcheck && _cpu->cpl() > desc.dpl()))))
	    {
	      Logging::printf("set_segment %x sel %x eip %x efl %x ar %x dpl %x rpl %x cpl %x\n", seg - &_cpu->es, sel, _cpu->eip, _cpu->efl, desc.ar0, desc.dpl(), rpl, _cpu->cpl());
	      GP(sel);
	    }
	  if (~desc.ar0 & 0x80) is_ss ? (SS(sel)) : (NP(sel));
	  desc_set_flag(desc, sel, 0x1, false);
	  desc.to_cpustate(seg, sel);
	}
    }
  return _fault;
}

int helper_far_jmp(unsigned tmp_cs, unsigned tmp_eip, unsigned tmp_flag)
{
  _mtr_out |= MTD_CS_SS | MTD_RFLAGS;
  if (!_cpu->pm() || _cpu->v86())
    // realmode + v86mode
    {
      if (tmp_eip > _cpu->cs.limit) GP0;
      _cpu->cs.ar  = (_cpu->cs.ar & ~0xf) | 0x3;
      set_realmode_segment(&_cpu->cs, tmp_cs, _cpu->v86());
      _cpu->eip    = tmp_eip;
      _cpu->efl    = (_cpu->efl & (EFL_VIP | EFL_VIF | EFL_VM | EFL_IOPL)) | (tmp_flag  & ~(EFL_VIP | EFL_VIF | EFL_VM | EFL_IOPL));
    }
  else
    {
      Descriptor desc;
      if (!load_gdt_descriptor(desc, tmp_cs, false))
	{
	  if (tmp_eip > desc.limit())  GP0;
	  desc_set_flag(desc, tmp_cs, 0x1, false);
	  // XXX ring transistions and task switch and call gates
	  desc.to_cpustate(&_cpu->cs, tmp_cs);
	  _cpu->eip = tmp_eip;
	  _cpu->efl = tmp_flag | 2;
	}
    }
  return _fault;
}


template<unsigned operand_size, bool lcall>
int helper_lcall(void *tmp_src)
{
  void *addr;
  unsigned short sel;
  unsigned ofs = 0;
  if (~_entry->flags & IC_MODRM)
      addr = _entry->data + _entry->offset_opcode;
  else
    if (modrm2mem(addr, 2 + (1 << operand_size), user_access(TYPE_R))) return _fault;

  move<operand_size>(&ofs, addr);
  move<1>(&sel, reinterpret_cast<char *>(addr) + (1 << operand_size));

  unsigned cs_sel = _cpu->cs.sel;
  if (lcall && (helper_PUSH<operand_size>(&cs_sel) || helper_PUSH<operand_size>(&_cpu->eip))) return _fault;
  return helper_far_jmp(sel, ofs, _cpu->efl);
}

template<unsigned operand_size>
int helper_LJMP(void *tmp_src)
{  return helper_lcall<operand_size, false>(tmp_src); }


template<unsigned operand_size>
int helper_LCALL(void *tmp_src)
{  return helper_lcall<operand_size, true>(tmp_src); }


template<unsigned operand_size>
int helper_IRET()
{
  _mtr_out |= MTD_CS_SS | MTD_DS_ES | MTD_FS_GS | MTD_RFLAGS;
  if (_cpu->v86())
    {
      if (_cpu->iopl() != 3) GP0;
    }
  else
    if (_cpu->pm() && _cpu->efl & (1<<14)) UNIMPLEMENTED(this); // task return


  // protected mode
  unsigned tmp_eip = 0, tmp_cs = 0, tmp_flag = _cpu->efl;
  if (helper_POP<operand_size>(&tmp_eip) || helper_POP<operand_size>(&tmp_cs) || helper_POP<operand_size>(&tmp_flag))
    return _fault;

  // RETURN-TO-VIRTUAL-8086-MODE?
  if ((tmp_flag & EFL_VM) && !_cpu->cpl())
    {
      assert(operand_size == 2);
      unsigned tmp_ss = 0, tmp_esp = 0;
      if (helper_POP<operand_size>(&tmp_esp) || helper_POP<operand_size>(&tmp_ss))
	return _fault;
      unsigned sels[4];
      for (unsigned i=0; i < 4; i++)
	if (helper_POP<operand_size>(sels+i)) return _fault;
      _cpu->eip = tmp_eip & 0xffff;  // upper bits ignored while entering a VM
      _cpu->esp = tmp_esp;
      _cpu->efl = tmp_flag | 2;
      set_realmode_segment(&_cpu->cs, tmp_cs,  true);
      set_realmode_segment(&_cpu->es, sels[0], true);
      set_realmode_segment(&_cpu->ds, sels[1], true);
      set_realmode_segment(&_cpu->fs, sels[2], true);
      set_realmode_segment(&_cpu->gs, sels[3], true);
      set_realmode_segment(&_cpu->ss, tmp_ss,  true);
      _cpu->intr_state &= ~8;  // clear NMI BLOCKING
      return _fault;
    }


  if (_cpu->pm() && !(_cpu->v86()))
    {
      //return to PMODE
      Descriptor desc;
      if (!load_gdt_descriptor(desc, tmp_cs, false))
	{
	  if (tmp_eip > desc.limit())  GP0;
	  if ((tmp_cs & 3) != _cpu->cpl())
	    {
	      unsigned tmp_ss = 0, tmp_esp = 0;
	      if (helper_POP<operand_size>(&tmp_esp) || helper_POP<operand_size>(&tmp_ss))
		return _fault;
	      _cpu->esp = tmp_esp;
	      if (set_segment(&_cpu->ss, tmp_ss, false)) return _fault;
	    }

	  // ring transistions??
	  desc_set_flag(desc, tmp_cs, 0x1, false);
	  desc.to_cpustate(&_cpu->cs, tmp_cs);
	  _cpu->eip = tmp_eip;
	  _cpu->efl = tmp_flag | 2;
	}
      _cpu->intr_state &= ~8;  // clear NMI BLOCKING
      return _fault;
    };
  // RETURN-FROM-VIRTUAL-8086-MODE? || REALMODE
  if (!helper_far_jmp(tmp_cs, tmp_eip, tmp_flag))
    _cpu->intr_state &= ~8; // clear NMI BLOCKING
  return _fault;
}


int idt_traversal(unsigned event, unsigned error_code)
{
  assert(event & 0x80000000);
  assert(event != 0x80000b0e || _cpu->pm() && _cpu->pg());
  _mtr_out |= MTD_RFLAGS | MTD_CS_SS;

  // realmode
  if (!_cpu->pm())
    {
      void *res;
      unsigned ofs = (event & 0xff) << 2;
      unsigned idt;
      if (prepare_virtual(_cpu->id.base + ofs, 4, MemTlb::TYPE_R, res))  return _fault;
      move<2>(&idt, res);
      if (helper_PUSH<1>(&_cpu->efl)
	  || helper_PUSH<1>(&_cpu->cs.sel)
	  || helper_PUSH<1>(&_cpu->eip)) return _fault;
      if (_cpu->id.limit < (ofs | 3)) GP0;
      _cpu->efl &= ~(EFL_AC | EFL_TF | EFL_IF | EFL_RF);
      return helper_far_jmp(idt >> 16, idt & 0xffff, _cpu->efl & ~(EFL_IF | EFL_TF | EFL_AC));
    }

  // pmode
  bool has_errorcode = event & 0x800;
  Descriptor idt;
  if (!load_idt_descriptor(idt, event))
    {
      if ((idt.ar0 & 0x1f) == 0x05) UNIMPLEMENTED(this); // task gate
      bool ext = (event & 0x700) <= 0x300;
      Descriptor desc;
      unsigned old_efl = _cpu->efl;
      unsigned newcpl = _cpu->cpl();
      switch (idt.ar0 & 0x1f)
	{
	case 0x0f: // trap gate 32bit
	case 0x0e: // interrupt gate 32bit
	  {
	    if (load_gdt_descriptor(desc, idt.base0, ext))   return _fault;
	    if (((desc.ar0 & 0x18) != 0x18) || (desc.dpl() > _cpu->cpl())) GP(idt.base0 | ext);
	    if (~desc.ar0 & 0x80) NP(idt.base0 | ext);

	    if (desc.ar0 & 0x8)  newcpl = desc.dpl();
	    Logging::printf("IDT ar %x dpl %x cpl %x sel %x vec %x eip %x/%x/%x\n", desc.ar0, desc.dpl(), _cpu->cpl(), idt.base0, event,
			    _cpu->eip, _cpu->cs.sel, old_efl);

	    if (!_cpu->v86())
	      {
		Utcb::Descriptor oldss = _cpu->ss;
		unsigned short new_ss;
		unsigned new_esp;
		if (newcpl != _cpu->cpl())
		  {
		    // XXX hacked TSS handling
		    void *tss;
		    if (prepare_virtual(_cpu->tr.base + 4, 8, MemTlb::TYPE_R, tss)) return _fault;
		    _cpu->efl &= ~(EFL_VM | EFL_TF | EFL_RF | EFL_NT);
		    move<2>(&new_esp, tss);
		    move<1>(&new_ss,  reinterpret_cast<char *>(tss)+4*(newcpl+1));
		    _cpu->esp = new_esp;
		    if (set_segment(&_cpu->ss, new_ss, false)
			|| helper_PUSH<2>(&oldss)
			|| helper_PUSH<2>(&_oesp))
		      {
			_cpu->ss = oldss;
			return _fault;
		      }
		  }
		unsigned cs_sel = _cpu->cs.sel;
		if (helper_PUSH<2>(&old_efl)
		    || helper_PUSH<2>(&cs_sel)
		    || helper_PUSH<2>(&_cpu->eip)
		    || has_errorcode && helper_PUSH<2>(&error_code))
		  {
		    _cpu->ss = oldss;
		    return _fault;
		  }
		_cpu->efl &= ~(EFL_VM | EFL_TF | EFL_RF | EFL_NT);
		if ((idt.ar0 & 0x1f) == 0xe)  _cpu->efl &= ~EFL_IF;
		desc.to_cpustate(&_cpu->cs, idt.base0);
		_cpu->eip = idt.offset();
	      }
	    else
	      {
		if (desc.dpl() != 0) GP0;
		//FROM-V86-MODE

		// XXX hacked TSS handling
		void *tss;
		unsigned old_ss = _cpu->ss.sel;
		unsigned short new_ss;
		unsigned new_esp;
		if (prepare_virtual(_cpu->tr.base + 4, 8, MemTlb::TYPE_R, tss)) return _fault;
		_cpu->efl &= ~(EFL_VM | EFL_TF | EFL_RF | EFL_NT);
		move<2>(&new_esp, tss);
		move<1>(&new_ss,  reinterpret_cast<char *>(tss)+4);

		// XXX 32bit push with 16bit selector
		_cpu->esp = new_esp;
		unsigned cs_sel = _cpu->cs.sel;
		if (set_segment(&_cpu->ss, new_ss, false)
		    || helper_PUSH<2>(&_cpu->gs.sel)
		    || helper_PUSH<2>(&_cpu->fs.sel)
		    || helper_PUSH<2>(&_cpu->ds.sel)
		    || helper_PUSH<2>(&_cpu->es.sel)
		    || helper_PUSH<2>(&old_ss)
		    || helper_PUSH<2>(&_oesp)
		    || helper_PUSH<2>(&old_efl)
		    || helper_PUSH<2>(&cs_sel)
		    || helper_PUSH<2>(&_cpu->eip)
		    || has_errorcode && helper_PUSH<2>(&error_code)
		    || set_segment(&_cpu->ds, 0)
		    || set_segment(&_cpu->es, 0)
		    || set_segment(&_cpu->fs, 0)
		    || set_segment(&_cpu->gs, 0)
		    )
		  {
		    Logging::printf("failed to traverse %x!\n", _fault);
		    // rollback efl+SS change
		    _cpu->efl = old_efl;
		    set_realmode_segment(&_cpu->ss, old_ss, true);
		    break;
		  }
		desc.to_cpustate(&_cpu->cs, idt.base0);
		_cpu->eip = idt.offset();
		break;
	      }
	    break;
	  case 0x07: // trap gate 16bit
	  case 0x06: // interrupt gate 16bit
	  default:
	    UNIMPLEMENTED(this);
	  };
	}
    }
  return _fault;
}


int helper_INT(unsigned char vector) { return idt_traversal(0x80000600 | vector, 0); }
int helper_INVLPG() { return _fault; }
int helper_FWAIT()                              { return _fault; }
int helper_MOV__DB0__EDX()
{
  unsigned dbreg = (_entry->data[_entry->offset_opcode] >> 3) & 0x7;
  if ((dbreg == 4 || dbreg == 5) && ~_cpu->cr4 & 0x8)
    dbreg += 2;

  unsigned *tmp_src;
  switch (dbreg)
    {
    case 0 ... 3: tmp_src = &_dr[dbreg]; break;
    case 6: tmp_src = &_dr6; break;
    case 7: tmp_src = &_cpu->dr7; break;
    default: UD0;
    }
  *get_reg32((_entry->data[_entry->offset_opcode]) & 0x7) = *tmp_src;
  return _fault;
}
int helper_MOV__EDX__DB0()
{
  unsigned dbreg = (_entry->data[_entry->offset_opcode] >> 3) & 0x7;
  if ((dbreg == 4 || dbreg == 5) && ~_cpu->cr4 & 0x8)
    dbreg += 2;
  unsigned value = *get_reg32((_entry->data[_entry->offset_opcode]) & 0x7);
  switch (dbreg)
    {
    case 0 ... 3: _dr[dbreg] = value; break;
    case 6: _dr6 = (value & ~0x1000) | 0xffff0ff0; break;
    case 7: _cpu->dr7  = (value & ~0xd800) | 0x400; break;
    default: UD0;
    }
  return _fault;
}



/**
 * fxsave.
 * Missing: #AC for unaligned access
 */
int helper_FXSAVE()
{
  unsigned virt = modrm2virt();
  if (virt & 0xf) GP0; // could be also AC if enabled
  for (unsigned i=0; i < sizeof(_fpustate)/sizeof(unsigned); i++)
    {
      void *addr;
      if (!virt_to_ptr(addr, 4, user_access(TYPE_W), virt + i*sizeof(unsigned)))  return _fault;
      move<2>(addr, _fpustate+i);
    }
  return _fault;
}


int helper_FRSTOR()
{
  // unsigned virt = modrm2virt();
  UNIMPLEMENTED(this);
}


void helper_AAM(unsigned char imm) {
  if (!imm) DE0(this);
  _cpu->ax = ((_cpu->al / imm) << 8) | (_cpu->al % imm);
  _mtr_out |= MTD_GPR_ACDB | MTD_RFLAGS;
  unsigned zero = 0;
  calc_flags(0, &_cpu->eax, &zero);
}

void helper_AAD(unsigned char imm) {
  _cpu->ax = (_cpu->al + (_cpu->ah * imm)) & 0xff;
  _mtr_out |= MTD_GPR_ACDB | MTD_RFLAGS;
  unsigned zero = 0;
  calc_flags(0, &_cpu->eax, &zero);
}


void helper_XLAT() {
  void *dst = 0;
  if (!logical_mem<0>((&_cpu->es) + ((_entry->prefixes >> 8) & 0xf), _cpu->ebx + _cpu->al, false, dst)) {
    _mtr_out |= MTD_GPR_ACDB;
    move<0>(&_cpu->eax, dst);
  }
}


template<unsigned operand_size>
void helper_ENTER(unsigned *imm) {

  unsigned long ebp = _cpu->ebp;
  if (helper_PUSH<operand_size>(&ebp)) return;

  unsigned long frametemp = _cpu->esp;
  unsigned nesting_level = (*imm >> 16) & 0x1f;
  void *tmp = 0;
  for (unsigned n = 1; n < nesting_level; n++) {
    ebp -= 1 << operand_size;
    if (logical_mem<operand_size>(&_cpu->ss, ebp, false, tmp, true) || helper_PUSH<operand_size>(tmp))  return;
  }
  if (nesting_level && helper_PUSH<operand_size>(&frametemp))  return;
  _cpu->esp -= (*imm & 0xffff);

  // check for errors while accessing the top of the stack
  if (logical_mem<operand_size>(&_cpu->ss, _cpu->esp, true, tmp, true)) return;

  _cpu->ebp = frametemp;
}
