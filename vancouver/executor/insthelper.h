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
   * Add base and check segment type and limit.
   */
  static int handle_segment(MessageExecutor &msg, InstructionCacheEntry *entry, CpuState::Descriptor *desc, unsigned &virt, unsigned length, bool write)
  {
    // align address
    //if (virt < 0xb8000 || virt > 0xc0000)
    //  Logging::printf("%s() #%x %x+%d desc %x limit %x base %x esp %x\n", __func__, desc - &msg.cpu->es, virt, length, desc->ar, desc->limit, desc->base, msg.cpu->esp);
    if (!entry)
      {
	assert(desc == &msg.cpu->ss);
	unsigned stack_size = ((msg.cpu->ss.ar >> 10) & 1) + 1;
	//Logging::printf("stack_size %x eip %x esp %x\n", stack_size, msg.cpu->eip, msg.cpu->esp);
	if (stack_size == 1) virt &= 0xffff;
      }
    else
      if (entry->address_size == 1) virt &= 0xffff; 
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
	Logging::printf("%s() #%x %x+%d desc %x limit %x base %x esp %x\n", __func__, desc - &msg.cpu->es, virt, length, desc->ar, desc->limit, desc->base, msg.cpu->esp);
	if (desc == &msg.cpu->ss) {  SS0; } else { GP0; }
      }
    
    // add segment base
    virt += desc->base;
    //Logging::printf("handle_segment %x+%d base %x\n", virt, length, desc->base);
    return msg.vcpu->fault;
  }

  template<unsigned operand_size>
  static int logical_mem(MessageExecutor &msg, InstructionCacheEntry *entry, CpuState::Descriptor *desc, unsigned virt, bool write, void *&res)
  {
    handle_segment(msg, entry, desc, virt, 1 << operand_size, write) 
      || msg.vcpu->instcache->prepare_virtual(msg, virt, 1 << operand_size, user_access(msg, write ? TYPE_W : TYPE_R) , res);
    return msg.vcpu->fault;
  }

  template<unsigned operand_size>
  static void move(void *tmp_dst, void *tmp_src)
  {
    if (operand_size == 0) asm volatile ("movsb" : "+S"(tmp_src), "+D"(tmp_dst) : : "memory");
    if (operand_size == 1) asm volatile ("movsw" : "+S"(tmp_src), "+D"(tmp_dst) : : "memory");
    if (operand_size == 2) asm volatile ("movsl" : "+S"(tmp_src), "+D"(tmp_dst) : : "memory");
  }

  /**
   * Transfer bytes from src to dst.
   */
  static void move(void *tmp_dst, void *tmp_src, unsigned order)
  {    
    switch (order)
      {
      case 0:  move<0>(tmp_dst, tmp_src); break;
      case 1:  move<1>(tmp_dst, tmp_src); break;
      case 2:  move<2>(tmp_dst, tmp_src); break;
      default: assert(0);
      }
  }

  /**
   * Perform an absolute JMP.
   */
  template<unsigned operand_size>
  static int helper_JMP_absolute(MessageExecutor &msg, unsigned nrip, InstructionCacheEntry *entry)
  {
    if (operand_size == 1)  nrip &= 0xffff;
    unsigned limit = READ(cs).limit;
    if (~limit && limit < nrip)  GP0;
    msg.cpu->eip = nrip;
    return 0;
  }

  /**
   * Do an unconditional JMP.
   */
  template<unsigned operand_size>
  static int __attribute__((regparm(3))) __attribute__((noinline)) helper_JMP(MessageExecutor &msg, void *tmp_src, InstructionCacheEntry *entry)
  {
    unsigned nrip = 0;
    if (entry->flags & IC_MODRM)
      move<operand_size>(&nrip, tmp_src);
    else
      nrip = msg.cpu->eip + *reinterpret_cast<int *>(tmp_src);
    return helper_JMP_absolute<operand_size>(msg, nrip, entry);
  }

static int helper_HLT (MessageExecutor &msg)    {  if (msg.cpu->cpl()) { GP0; } return msg.vcpu->fault = FAULT_HLT; }
static int helper_WBINVD (MessageExecutor &msg) {  if (msg.cpu->cpl()) { GP0; } return msg.vcpu->fault = FAULT_WBINVD; }
static int helper_INVD (MessageExecutor &msg)   {  if (msg.cpu->cpl()) { GP0; } return msg.vcpu->fault = FAULT_INVD; }
static int helper_INT3(MessageExecutor &msg) {  msg.vcpu->oeip = msg.cpu->eip; return msg.vcpu->fault = 0x80000603; }
static int helper_UD2A(MessageExecutor &msg) {  return msg.vcpu->fault = 0x80000606; }
static int helper_INTO(MessageExecutor &msg) {  msg.vcpu->oeip = msg.cpu->eip; if (msg.cpu->efl & EFL_OF) msg.vcpu->fault = 0x80000604; return msg.vcpu->fault; }
static int helper_CLTS(MessageExecutor &msg) {  if (msg.cpu->cpl()) GP0; msg.cpu->cr0 &= ~(1<<3); return msg.vcpu->fault; }


  /**
   * Push a stackframe on the stack.
   */
  template<unsigned operand_size>
  static int __attribute__((regparm(3))) __attribute__((noinline))  helper_PUSH(MessageExecutor &msg, void *tmp_src, InstructionCacheEntry *entry)
  {
    void *res;
    unsigned length = 1 << operand_size;
    msg.cpu->esp -= length;
    unsigned virt = msg.cpu->esp;
    if (!(handle_segment(msg, entry, &msg.cpu->ss, virt, length, true) 
	  || msg.vcpu->instcache->prepare_virtual(msg, virt, length, user_access(msg, TYPE_W), res)))
	move<operand_size>(res, tmp_src);
    return msg.vcpu->fault;
  }


  /**
   * Pop a stackframe from the stack.
   */
  template<unsigned operand_size>
  static int __attribute__((regparm(3)))  __attribute__((noinline)) helper_POP(MessageExecutor &msg, InstructionCacheEntry *entry, void *tmp_dst)
  {
    void *res;
    unsigned virt = msg.cpu->esp;
    unsigned length = 1 << operand_size;
    if (!(handle_segment(msg, entry, &msg.cpu->ss, virt, length, false) || msg.vcpu->instcache->prepare_virtual(msg, virt, length, user_access(msg, TYPE_R), res)))
      {
	msg.cpu->esp += length;
	move<operand_size>(tmp_dst, res);
      }
    return msg.vcpu->fault;
  }


  /**
   * Return from a function.
   */
  template<unsigned operand_size>
  static void __attribute__((regparm(3)))  helper_RET(MessageExecutor &msg, void *tmp_src, InstructionCacheEntry *entry)
  {
    unsigned tmp_eip;
    helper_POP<operand_size>(msg, entry, &tmp_eip) || helper_JMP_absolute<operand_size>(msg, tmp_eip, entry);
    if (tmp_src)  msg.cpu->esp += *reinterpret_cast<unsigned short *>(tmp_src);
  }


  /**
   * FarReturn from a function.
   */
  template<unsigned operand_size>
  static void __attribute__((regparm(3)))  helper_LRET(MessageExecutor &msg, void *tmp_src, InstructionCacheEntry *entry)
  {
    unsigned tmp_eip = 0, tmp_cs = 0;
    helper_POP<operand_size>(msg, entry, &tmp_eip) || helper_POP<operand_size>(msg, entry, &tmp_cs) || helper_far_jmp(msg, tmp_cs, tmp_eip, msg.cpu->efl);
    if (tmp_src)  msg.cpu->esp += *reinterpret_cast<unsigned short *>(tmp_src);
  }


  /**
   * Leave a function.
   */
  template<unsigned operand_size>
  static void __attribute__((regparm(3)))  helper_LEAVE(MessageExecutor &msg, InstructionCacheEntry *entry)
  {
    unsigned stack_size = ((msg.cpu->ss.ar >> 10) & 1) + 1;
    move(&msg.cpu->esp, &msg.cpu->ebp, stack_size);
    helper_POP<operand_size>(msg, entry, &msg.cpu->ebp);
  }

  /**
   * Implement LOOPNE, LOOPE, LOOP, and JECXZ
   */
#define helper_LOOPS(NAME, X)						\
  template<unsigned operand_size>					\
    static void __attribute__((regparm(3))) __attribute__((noinline))	\
    helper_##NAME(MessageExecutor &msg, void *tmp_src, InstructionCacheEntry *entry) \
    {									\
    unsigned long ecx = 0;						\
    move<operand_size>(&ecx, &msg.cpu->ecx);				\
    if (X != 3) --ecx;							\
    if ((ecx && (X==0 || (X==1 && msg.cpu->efl & 0x40) || (X==2 && ~msg.cpu->efl & 0x40))) || (!ecx && X == 3)) \
      if (helper_JMP<operand_size>(msg, tmp_src, entry))		\
	return;								\
    move<operand_size>(&msg.cpu->ecx, &ecx);				\
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
  static void __attribute__((regparm(3))) __attribute__((noinline)) helper_CALL(MessageExecutor &msg, void *tmp_src, InstructionCacheEntry *entry)
  {
    helper_PUSH<operand_size>(msg, &msg.cpu->eip, entry) || helper_JMP<operand_size>(msg, tmp_src, entry);
  }



  /**
   * LGDT and LIDT helper.
   */
#define helper_LDT(NAME, VAR)						\
  template<unsigned operand_size>					\
    static void __attribute__((regparm(3)))				\
    helper_##NAME(MessageExecutor &msg, InstructionCacheEntry *entry) \
    {									\
    void *addr;								\
    if (!modrm2mem(msg, entry, addr, 6, user_access(msg, TYPE_R)))	\
      {									\
      unsigned base;							\
      move<1>(&msg.cpu->VAR.limit, addr);				\
      move<2>(&base, reinterpret_cast<char *>(addr)+2);			\
      if (operand_size == 1) base &= 0x00ffffff;			\
      msg.cpu->VAR.base = base;						\
      }									\
    }
helper_LDT(LIDT, id)
helper_LDT(LGDT, gd)
#undef helper_LDT

  /**
   * SGDT and SIDT helper.
   */
#define helper_SDT(NAME, VAR)						\
  template<unsigned operand_size>					\
  static void __attribute__((regparm(3)))				\
  helper_##NAME(MessageExecutor &msg, InstructionCacheEntry *entry)	\
  {									\
    void *addr;								\
    if (!modrm2mem(msg, entry, addr, 6, user_access(msg, TYPE_W)))	\
      {									\
	unsigned base = msg.cpu->VAR.base;				\
	if (operand_size == 1) base &= 0x00ffffff;			\
	move<1>(addr, &msg.cpu->VAR.limit);				\
	move<2>(reinterpret_cast<char *>(addr)+2, &base);		\
      }									\
  }
  helper_SDT(SIDT,id)
  helper_SDT(SGDT,gd)
#undef helper_SDT


  template<unsigned operand_size>
  static int __attribute__((regparm(3)))  helper_POPF(MessageExecutor &msg, InstructionCacheEntry *entry)
  {
    unsigned long tmp;
    if (msg.cpu->v86() && msg.cpu->iopl() < 3) 
      GP0
    else if (!helper_POP<operand_size>(msg, entry, &tmp))
      {
	// clear VIP+VIF
	if (!msg.cpu->v86()) tmp &= ~0x180000;
	
	// reserved bits and RF+VM are unaffected
	unsigned mask = 0x3f7fd5L & ~0x30000;
	// iopl and IF also if not running with CPL==0
	if (msg.cpu->cpl()) mask &= ~0x3200;
	tmp = READ(efl) & ~mask | tmp & mask;
	WRITE(efl);
	move<operand_size>(&msg.cpu->efl, &tmp);
      }
    return msg.vcpu->fault;
  }


  template<unsigned operand_size>
  static int __attribute__((regparm(3)))  helper_PUSHF(MessageExecutor &msg, InstructionCacheEntry *entry)
  {
    if (msg.cpu->v86() && msg.cpu->iopl() < 3)
      GP0
    else
      {
	unsigned long tmp = READ(efl) & 0xfcffff;
	return helper_PUSH<operand_size>(msg, &tmp, entry);
      }
  }

  template<unsigned operand_size>
  static void __attribute__((regparm(3)))  helper_IN(MessageExecutor &msg, unsigned port, void *dst)
  {

    // XXX check IOPBM
    MessageIOIn msg1(static_cast<MessageIOIn::Type>(operand_size), port);
    bool res =  msg.vcpu->instcache->_mb.bus_ioin.send(msg1);
    move<operand_size>(dst, &msg1.value);

    //Logging::printf("in<%d>(%x) = %x\n", operand_size, port, msg1.value);

    static unsigned char debugioin[8192];
    if (!res && ~debugioin[port >> 3] & (1 << (port & 7)))
      {
	debugioin[port >> 3] |= 1 << (port & 7);
	Logging::panic("could not read from ioport %x eip %x cs %x-%x\n", port, msg.vcpu->oeip, msg.cpu->cs.base, msg.cpu->cs.ar);
      }
  }

  template<unsigned operand_size>
  static void __attribute__((regparm(3)))  helper_OUT(MessageExecutor &msg, unsigned port, void *src)
  {

    // XXX check IOPBM
    MessageIOOut msg1(static_cast<MessageIOOut::Type>(operand_size), port, 0);
    move<operand_size>(&msg1.value, src);
    bool res =  msg.vcpu->instcache->_mb.bus_ioout.send(msg1);

    static unsigned char debugioout[8192];
    if (!res && ~debugioout[port >> 3] & (1 << (port & 7)))
      {
	debugioout[port >> 3] |= 1 << (port & 7);
	Logging::printf("could not write to ioport %x at %x\n", port, msg.cpu->eip);
      }
  }


  enum STRING_HELPER_FEATURES
  {
    SH_LOAD_ESI = 1 << 0,
    SH_SAVE_EDI = 1 << 1,
    SH_LOAD_EDI = 1 << 2,
    SH_SAVE_EAX = 1 << 3,
    SH_DOOP_CMP = 1 << 4,
    SH_DOOP_IN  = 1 << 5,
    SH_DOOP_OUT = 1 << 6,
  };

#define NCHECK(X)  { if (X) break; }
#define FEATURE(X,Y) { if (feature & (X)) Y; }
  template<unsigned feature, unsigned operand_size>
  static int __attribute__((regparm(3)))  string_helper(MessageExecutor &msg, InstructionCacheEntry *entry)
  {
    //Logging::printf("%s-%x efl %x edi %x ecx %x eip %x prefix %x\n", __func__, feature, msg.cpu->efl, msg.cpu->edi, msg.cpu->ecx, msg.cpu->eip, entry->prefixes);
    while (entry->address_size == 1 && msg.cpu->cx || entry->address_size == 2 && msg.cpu->ecx || !(entry->prefixes & 0xff))
      {
	unsigned res;
	void *src = &msg.cpu->eax;
	void *dst = &msg.cpu->eax;
	FEATURE(SH_LOAD_ESI, NCHECK(logical_mem<operand_size>(msg, entry, (&msg.cpu->es) + ((entry->prefixes >> 8) & 0xf), msg.cpu->esi, false, src)));
	FEATURE(SH_LOAD_EDI, NCHECK(logical_mem<operand_size>(msg, entry, &msg.cpu->es, msg.cpu->edi, false, dst)));
	FEATURE(SH_DOOP_IN,  helper_IN<operand_size>(msg,  msg.cpu->edx & 0xffff, dst));
	FEATURE(SH_DOOP_OUT, helper_OUT<operand_size>(msg, msg.cpu->edx & 0xffff, src));
	FEATURE(SH_DOOP_CMP, {
	    InstructionCacheEntry entry2;
	    entry2.execute = operand_size == 0 ? exec_38_cmp_0 : (operand_size == 1 ? exec_39_cmp_1 : exec_39_cmp_2);
	    entry2.flags = IC_SAVEFLAGS;
	    call_asm(msg, src, dst, &entry2);
	  }
	  );
	FEATURE(SH_SAVE_EDI, NCHECK(logical_mem<operand_size>(msg, entry, &msg.cpu->es, msg.cpu->edi, true, dst)));
	FEATURE(SH_SAVE_EDI | SH_SAVE_EAX, move<operand_size>(dst, src));
	int size = 1 << operand_size;
	if (msg.cpu->efl & 0x400)  size = -size;
	FEATURE(SH_LOAD_ESI,               if (entry->address_size == 1)  msg.cpu->si += size; else msg.cpu->esi += size;);
	FEATURE(SH_LOAD_EDI | SH_SAVE_EDI, if (entry->address_size == 1)  msg.cpu->di += size; else msg.cpu->edi += size;);
	if (!(entry->prefixes & 0xff)) break;
	if (entry->address_size == 1)  msg.cpu->cx--; else msg.cpu->ecx--;
	FEATURE(SH_DOOP_CMP,  if (((entry->prefixes & 0xff) == 0xf3)  && (~msg.cpu->efl & 0x40))  break);
	FEATURE(SH_DOOP_CMP,  if (((entry->prefixes & 0xff) == 0xf2)  && ( msg.cpu->efl & 0x40))  break);
      }
    //Logging::printf("%s efl %x edi %x ecx %x eip %x\n", __func__, msg.cpu->efl, msg.cpu->edi, msg.cpu->ecx, msg.cpu->eip);
    return msg.vcpu->fault;
  }


/**
 * Move from control register.
 */
static int helper_MOV__CR0__EDX(MessageExecutor &msg, InstructionCacheEntry * entry)
{
  void *tmp_src;
  void *tmp_dst = get_gpr(msg,  entry->data[entry->offset_opcode] & 0x7, 0);
  switch ((entry->data[entry->offset_opcode] >> 3) & 0x7) 
    {
    case 0: tmp_src = &msg.cpu->cr0; break;
    case 2: tmp_src = &msg.cpu->cr2; break;
    case 3: tmp_src = &msg.cpu->cr3; break;
    case 4: tmp_src = &msg.cpu->cr4; break;
    default: UD0; 
    }
  move<2>(tmp_dst, tmp_src);
  return msg.vcpu->fault;
}


/**
 * Move to control register.
 */
static int helper_MOV__EDX__CR0(MessageExecutor &msg, InstructionCacheEntry * entry)
{
  void *tmp_src = get_gpr(msg, entry->data[entry->offset_opcode] & 0x7, 0);
  void *tmp_dst;
  unsigned tmp = *reinterpret_cast<unsigned *>(tmp_src);
  switch ((entry->data[entry->offset_opcode] >> 3) & 0x7) 
    {
    case 0: if (tmp & 0x1ffaffc0U) GP0;  tmp_dst = &msg.cpu->cr0; break;
    case 2: tmp_dst = &msg.cpu->cr2; break;
    case 3: tmp_dst = &msg.cpu->cr3; break;
    case 4: if (tmp & 0xffff9800U) GP0;  tmp_dst = &msg.cpu->cr4; break;
    default: UD0; 
    }
  move<2>(tmp_dst, tmp_src);
  msg.vcpu->hazard |= VirtualCpuState::HAZARD_CRWRITE;

  // update TLB
  return msg.vcpu->instcache->init(msg);
}


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


  int get_base(MessageExecutor &msg, unsigned short selector, unsigned long &base, bool ext)
  {
    unsigned long l;
    if (selector & 0x4)
      {
	l =  msg.cpu->ld.limit;
	base  = msg.cpu->ld.base;
      }
    else
      {
	l =  msg.cpu->gd.limit;
	base = msg.cpu->gd.base;
      }
    //Logging::printf("get_base(%x) %lx %lx\n", selector, base, l);
    if (selector > (l + 7))  GP(selector | ext);
    base += selector & ~0x7;
    return msg.vcpu->fault;
  }
  

  int load_gdt_descriptor(MessageExecutor &msg, unsigned short selector, bool ext)
  {
    if (!(selector & ~0x3)) GP(ext);
    memset(values, 0, sizeof(values));
    unsigned long base;
    void *res;
    if (!get_base(msg, selector, base, ext) && !msg.vcpu->instcache->prepare_virtual(msg, base, 8, MemTlb::TYPE_R, res))
      memcpy(values, res, 8);
    return msg.vcpu->fault;
  };
 

  int load_idt_descriptor(MessageExecutor &msg, unsigned event)
  {
    unsigned ofs = (event & 0xff) << 3;
    bool ext = (event & 0x700) <= 0x300;
    unsigned error = ofs | (ext ? 2 : 3);

    if (msg.cpu->id.limit < (ofs | 7)) GP(error);
    void *res;
    if (!msg.vcpu->instcache->prepare_virtual(msg, msg.cpu->id.base + ofs, 8, MemTlb::TYPE_R, res))
      {
	memcpy(values, res, 8);
	// is it a trap, intr or task-gate?
	if (!(0xce00 & (1<<(ar0 & 0x1f)))) {
	  Logging::panic("%s event %x %x base %x limit %x cr0 %x\n", __func__, event, ar0, msg.cpu->id.base, msg.cpu->id.limit, msg.cpu->cr0);	
	  GP(error);
	}
	// and present?
	if (~ar0 & 0x80) NP(error);
      }
    return msg.vcpu->fault;
  }


  int set_flag(MessageExecutor &msg, unsigned short selector, unsigned char flag, bool ext)
  {
    Descriptor desc2 = *this;
    ar0 |= flag;
    
    unsigned long base;
    void *res;
    if (!get_base(msg, selector, base, ext) && !msg.vcpu->instcache->prepare_virtual(msg, base, 8, MemTlb::TYPE_RMW, res))
      // do a fire and forget cmpxchg here
      asm volatile ("lock; cmpxchg8b (%4)" : "+a"(desc2.values[0]), "+d"(desc2.values[1]) :  "b"(values[0]), "c"(values[1]), "r"(res) : "memory", "cc");
    return msg.vcpu->fault;
  }

  void to_cpustate(CpuState::Descriptor *desc, unsigned short selector)
  {
    desc->sel = selector;
    desc->limit = limit();
    desc->base  = base0  | (base1 << 16) | (base2 << 24);
    desc->ar    = ar0 | ((ar1 & 0xf0) << 4);
  }
  
};


static int helper_LTR(MessageExecutor &msg, unsigned short selector)
{
  if (!msg.cpu->pm() || msg.cpu->v86()) UD0;
  if (msg.cpu->cpl()) GP0;
  if (selector & 0x4) GP(selector & ~0x7);
  selector &= ~0x7;

  Descriptor desc;
  if (!desc.load_gdt_descriptor(msg, selector, false)) 
    {
      if ((desc.ar0 & 0x1f) != 0x9 && (desc.ar0 & 0x1f) != 0x1) GP(selector);
      if (~desc.ar0 & 0x80)  NP(selector);
      desc.set_flag(msg, selector, 0x2, false);
      desc.to_cpustate(&msg.cpu->tr, selector);
    }
  //Logging::printf("LTR %x base %x limit %x\n", msg.cpu->tr.sel, msg.cpu->tr.base, msg.cpu->tr.limit);
  return msg.vcpu->fault;
}


static int helper_LLDT(MessageExecutor &msg, unsigned short selector)
{
  if (!msg.cpu->pm() || msg.cpu->v86()) UD0;
  if (msg.cpu->cpl()) GP0;
  if (selector & 0x4) GP(selector & ~0x7);
  selector &= ~0x7;
  if (selector)
    {
      Descriptor desc;
      if (!desc.load_gdt_descriptor(msg, selector, false)) 
	{
	  if ((desc.ar0 & 0x1f) != 0x2) GP(selector);
	  if (~desc.ar0 & 0x80)  NP(selector);
	  desc.to_cpustate(&msg.cpu->ld, selector);
	}
    }
  else
    msg.cpu->ld.ar = 0x1000;
  return msg.vcpu->fault;
}


static void set_realmode_segment(CpuState::Descriptor *seg, unsigned short sel, bool cpl0)
{
  //Logging::printf("set_realmode_segment %x\n", sel);

  // there is no limit modification in realmode
  seg->set(sel, sel << 4, seg->limit, cpl0 ? 0x93 : 0xf3);
}


static int set_segment(MessageExecutor &msg, CpuState::Descriptor *seg, unsigned short sel, bool cplcheck = true)
{
  //Logging::printf("set_segment %x sel %x eip %x efl %x\n", seg - &msg.cpu->es, sel, msg.cpu->eip, msg.cpu->efl);
  if (!msg.cpu->pm() || msg.cpu->v86()) 
    set_realmode_segment(seg, sel, !msg.cpu->v86());
  else
    {
      bool is_ss = seg == &msg.cpu->ss;
      unsigned rpl = sel & 0x3;
      Descriptor desc;
      if (!(sel & ~0x3) && !is_ss)
	{
	  seg->sel = sel;
	  seg->ar = 0x1000;
	  return msg.vcpu->fault;
	}

      if (!desc.load_gdt_descriptor(msg, sel, false)) 
	{
	  if ((is_ss && ((rpl != desc.dpl() || cplcheck && desc.dpl() != msg.cpu->cpl()) || ((desc.ar0 & 0x1a) != 0x12))) 
	      || !is_ss && ((((desc.ar0 ^ 0x12) & 0x1a) > 2) || (((desc.ar0 & 0xc) != 0xc) && (rpl > desc.dpl() || cplcheck && msg.cpu->cpl() > desc.dpl()))))
	    {
	      Logging::printf("set_segment %x sel %x eip %x efl %x ar %x dpl %x rpl %x cpl %x\n", seg - &msg.cpu->es, sel, msg.cpu->eip, msg.cpu->efl, desc.ar0, desc.dpl(), rpl, msg.cpu->cpl()); 
	      GP(sel);
	    }
	  if (~desc.ar0 & 0x80) is_ss ? (SS(sel)) : (NP(sel));
	  desc.set_flag(msg, sel, 0x1, false);
	  desc.to_cpustate(seg, sel);
	  //Logging::printf("set_segment %x sel %x eip %x efl %x ar %x\n", seg - &msg.cpu->es, sel, msg.cpu->eip, msg.cpu->efl, seg->ar);
	}
    }
  return msg.vcpu->fault;
}

static int helper_far_jmp(MessageExecutor &msg, unsigned tmp_cs, unsigned tmp_eip, unsigned tmp_flag)
{
  //Logging::printf("farjmp %x:%x efl %x\n", tmp_cs, tmp_eip, tmp_flag);
  if (!msg.cpu->pm() || msg.cpu->v86())
    // realmode + v86mode
    {
      if (tmp_eip > 0xffff) GP0;
      set_realmode_segment(&msg.cpu->cs, tmp_cs,  !msg.cpu->v86());
      msg.cpu->cs.limit = 0xffff;
      msg.cpu->eip = tmp_eip;
      msg.cpu->efl = (msg.cpu->efl & (EFL_VIP | EFL_VIF | EFL_VM | EFL_IOPL)) | (tmp_flag  & ~(EFL_VIP | EFL_VIF | EFL_VM | EFL_IOPL));
    }
  else
    {
      Descriptor desc;
      if (!desc.load_gdt_descriptor(msg, tmp_cs, false)) 
	{
	  if (tmp_eip > desc.limit())  GP0;
	  desc.set_flag(msg, tmp_cs, 0x1, false);
	  // XXX ring transistions and task switch and call gates
	  desc.to_cpustate(&msg.cpu->cs, tmp_cs);
	  msg.cpu->eip = tmp_eip;
	  msg.cpu->efl = tmp_flag | 2;
	}
    }
  return msg.vcpu->fault;
}


template<unsigned operand_size, bool lcall>
static int helper_lcall(MessageExecutor &msg, void *tmp_src, InstructionCacheEntry *entry)
{
  void *addr;
  unsigned short sel;
  unsigned ofs = 0;
  if (~entry->flags & IC_MODRM)
      addr = entry->data + entry->offset_opcode;
  else
    if (modrm2mem(msg, entry, addr, 2 + (1 << operand_size), user_access(msg, TYPE_R))) return msg.vcpu->fault;

  move<operand_size>(&ofs, addr);
  move<1>(&sel, reinterpret_cast<char *>(addr) + (1 << operand_size));

  //Logging::printf("%s eip %x-> %x:%x addr %p flags %x instr %x\n", lcall ? "lcall" :"ljmp", msg.cpu->eip, sel, ofs, addr, entry->flags,
  //*reinterpret_cast<unsigned *>(entry->data));
  unsigned cs_sel = msg.cpu->cs.sel;
  if (lcall && (helper_PUSH<operand_size>(msg, &cs_sel, entry) || helper_PUSH<operand_size>(msg, &msg.cpu->eip, entry))) return msg.vcpu->fault;
  return helper_far_jmp(msg, sel, ofs, msg.cpu->efl);
}

template<unsigned operand_size>
static int helper_LJMP(MessageExecutor &msg, void *tmp_src, InstructionCacheEntry *entry)
{  return helper_lcall<operand_size, false>(msg, tmp_src, entry); }


template<unsigned operand_size>
static int helper_LCALL(MessageExecutor &msg, void *tmp_src, InstructionCacheEntry *entry)
{  return helper_lcall<operand_size, true>(msg, tmp_src, entry); }


template<unsigned operand_size>
static int helper_IRET(MessageExecutor &msg, InstructionCacheEntry *entry)
{ 
  if (msg.cpu->v86())
    {
      if (msg.cpu->iopl() != 3) GP0;
    }
  else 
    if (msg.cpu->pm() && msg.cpu->efl & (1<<14)) UNIMPLEMENTED; // task return

  // protected mode
  unsigned tmp_eip = 0, tmp_cs = 0, tmp_flag = 0;
  if (helper_POP<operand_size>(msg, entry, &tmp_eip) || helper_POP<operand_size>(msg, entry, &tmp_cs) || helper_POP<operand_size>(msg, entry, &tmp_flag))
    return msg.vcpu->fault;
  
  // RETURN-TO-VIRTUAL-8086-MODE?
  if ((tmp_flag & EFL_VM) && !msg.cpu->cpl())
    {
      assert(operand_size == 2);
      unsigned tmp_ss = 0, tmp_esp = 0;
      if (helper_POP<operand_size>(msg, entry, &tmp_esp) || helper_POP<operand_size>(msg, entry, &tmp_ss)) 
	return msg.vcpu->fault;
      //Logging::printf("iret %x %x %x @%x,%x esp %x\n", tmp_eip, tmp_cs, tmp_flag, msg.vcpu->oesp, msg.vcpu->oeip, tmp_esp);
      unsigned sels[4];
      for (unsigned i=0; i < 4; i++)
	if (helper_POP<operand_size>(msg, entry, sels+i)) return msg.vcpu->fault;
      msg.cpu->eip = tmp_eip & 0xffff;
      msg.cpu->esp = tmp_esp;
      msg.cpu->efl = tmp_flag | 2;
      set_realmode_segment(&msg.cpu->cs, tmp_cs,  false);
      set_realmode_segment(&msg.cpu->es, sels[0], false);
      set_realmode_segment(&msg.cpu->ds, sels[1], false);
      set_realmode_segment(&msg.cpu->fs, sels[2], false);
      set_realmode_segment(&msg.cpu->gs, sels[3], false);
      set_realmode_segment(&msg.cpu->ss, tmp_ss,  false);
      return msg.vcpu->fault;
    }
  

  if (msg.cpu->pm() && !(msg.cpu->v86()))
    {
      //return to PMODE
      Descriptor desc;
      if (!desc.load_gdt_descriptor(msg, tmp_cs, false)) 
	{
	  if (tmp_eip > desc.limit())  GP0;
	  if ((tmp_cs & 3) != msg.cpu->cpl())
	    {
	      unsigned tmp_ss = 0, tmp_esp = 0;
	      if (helper_POP<operand_size>(msg, entry, &tmp_esp) || helper_POP<operand_size>(msg, entry, &tmp_ss)) 
		return msg.vcpu->fault;
	      msg.cpu->esp = tmp_esp;
	      //Logging::printf("iret %x %x %x @%x esp %x:%x\n", tmp_eip, tmp_cs, tmp_flag, msg.vcpu->oesp, tmp_ss, tmp_esp);
	      if (set_segment(msg, &msg.cpu->ss, tmp_ss, false)) return msg.vcpu->fault;
	    }

	  // ring transistions??
	  desc.set_flag(msg, tmp_cs, 0x1, false);
	  desc.to_cpustate(&msg.cpu->cs, tmp_cs);
	  msg.cpu->eip = tmp_eip;
	  msg.cpu->efl = tmp_flag | 2;
	}
      return msg.vcpu->fault;
    };
  // RETURN-FROM-VIRTUAL-8086-MODE? || REALMODE
  return helper_far_jmp(msg, tmp_cs, tmp_eip, tmp_flag);
}


static int idt_traversal(MessageExecutor &msg, unsigned event, unsigned error_code)
{
  //if (event != 0x80000020)
    //Logging::printf("IDT vec %x eip %x idt %x+%x cr0 %x eax %x efl %x oesp %x\n", 
    //event, msg.cpu->eip, msg.cpu->id.base, msg.cpu->id.limit, msg.cpu->cr0, msg.cpu->eax, msg.cpu->efl);
  assert(event & 0x80000000);
  assert(event != 0x80000b0e || msg.cpu->pm() && msg.cpu->pg());

  // realmode
  if (!msg.cpu->pm())
    {
      void *res;
      unsigned ofs = (event & 0xff) << 2;
      unsigned idt;
      if (msg.vcpu->instcache->prepare_virtual(msg, msg.cpu->id.base + ofs, 4, MemTlb::TYPE_R, res))  return msg.vcpu->fault;
      move<2>(&idt, res);
      if (helper_PUSH<1>(msg, &msg.cpu->efl, 0)  
	  || helper_PUSH<1>(msg, &msg.cpu->cs.sel, 0) 
	  || helper_PUSH<1>(msg, &msg.cpu->eip, 0)) return msg.vcpu->fault;
      if (msg.cpu->id.limit < (ofs | 3)) GP0;
      msg.cpu->efl &= ~(EFL_AC | EFL_TF | EFL_IF | EFL_RF);
      return helper_far_jmp(msg, idt >> 16, idt & 0xffff, msg.cpu->efl & ~(EFL_IF | EFL_TF | EFL_AC));
    }

  // pmode
  bool has_errorcode = event & 0x800;
  Descriptor idt;
  if (!idt.load_idt_descriptor(msg, event))
    {
      if ((idt.ar0 & 0x1f) == 0x05) UNIMPLEMENTED; // task gate
      bool ext = (event & 0x700) <= 0x300;
      Descriptor desc;     
      unsigned old_efl = msg.cpu->efl;
      unsigned newcpl = msg.cpu->cpl();
      switch (idt.ar0 & 0x1f)
	{
	case 0x0f: // trap gate 32bit
	case 0x0e: // interrupt gate 32bit
	  {
	    if (desc.load_gdt_descriptor(msg, idt.base0, ext))   return msg.vcpu->fault;
	    if (((desc.ar0 & 0x18) != 0x18) || (desc.dpl() > msg.cpu->cpl())) GP(idt.base0 | ext);
	    if (~desc.ar0 & 0x80) NP(idt.base0 | ext);

	    if (desc.ar0 & 0x8)  newcpl = desc.dpl();
	    //Logging::printf("IDT ar %x dpl %x cpl %x sel %x vec %x eip %x/%x/%x\n", desc.ar0, desc.dpl(), msg.cpu->cpl(), idt.base0, event, 
	    //msg.cpu->eip, msg.cpu->cs.sel, old_efl);
	    
	    if (!msg.cpu->v86())
	      {
		
		Utcb::Descriptor oldss = msg.cpu->ss;
		unsigned short new_ss;
		unsigned new_esp;
		if (newcpl != msg.cpu->cpl())
		  {
		    // XXX hacked TSS handling
		    void *tss;
		    if (msg.vcpu->instcache->prepare_virtual(msg, msg.cpu->tr.base + 4, 8, MemTlb::TYPE_R, tss)) return msg.vcpu->fault;
		    msg.cpu->efl &= ~(EFL_VM | EFL_TF | EFL_RF | EFL_NT);
		    move<2>(&new_esp, tss);
		    move<1>(&new_ss,  reinterpret_cast<char *>(tss)+4*(newcpl+1));
		    msg.cpu->esp = new_esp;
		    if (set_segment(msg, &msg.cpu->ss, new_ss, false) 
			|| helper_PUSH<2>(msg, &oldss, 0)
			|| helper_PUSH<2>(msg, &msg.vcpu->oesp, 0))
		      {
			msg.cpu->ss = oldss;
			return msg.vcpu->fault;
		      }
		  }
		unsigned cs_sel = msg.cpu->cs.sel;
		if (helper_PUSH<2>(msg, &old_efl, 0)
		    || helper_PUSH<2>(msg, &cs_sel, 0)
		    || helper_PUSH<2>(msg, &msg.cpu->eip, 0)
		    || has_errorcode && helper_PUSH<2>(msg, &error_code, 0))
		  {
		    msg.cpu->ss = oldss;
		    return msg.vcpu->fault;
		  }

		  
		msg.cpu->efl &= ~(EFL_VM | EFL_TF | EFL_RF | EFL_NT);
		if ((idt.ar0 & 0x1f) == 0xe)  msg.cpu->efl &= ~EFL_IF;
		//Logging::printf("IDT %x -> %x\n", msg.cpu->eip, idt.offset());
		desc.to_cpustate(&msg.cpu->cs, idt.base0);
		msg.cpu->eip = idt.offset();
	      }
	    else
	      {
		if (desc.dpl() != 0) GP0;
		//FROM-V86-MODE

		// XXX hacked TSS handling
		void *tss;
		unsigned old_ss = msg.cpu->ss.sel;
		unsigned short new_ss;
		unsigned new_esp;
		if (msg.vcpu->instcache->prepare_virtual(msg, msg.cpu->tr.base + 4, 8, MemTlb::TYPE_R, tss)) return msg.vcpu->fault;
		msg.cpu->efl &= ~(EFL_VM | EFL_TF | EFL_RF | EFL_NT);
		move<2>(&new_esp, tss);
		move<1>(&new_ss,  reinterpret_cast<char *>(tss)+4);
		
		// XXX 32bit push with 16bit selector
		msg.cpu->esp = new_esp;
		unsigned cs_sel = msg.cpu->cs.sel;
		if (set_segment(msg, &msg.cpu->ss, new_ss, false)
		    || helper_PUSH<2>(msg, &msg.cpu->gs.sel, 0) 
		    || helper_PUSH<2>(msg, &msg.cpu->fs.sel, 0)
		    || helper_PUSH<2>(msg, &msg.cpu->ds.sel, 0)
		    || helper_PUSH<2>(msg, &msg.cpu->es.sel, 0)
		    || helper_PUSH<2>(msg, &old_ss, 0)
		    || helper_PUSH<2>(msg, &msg.vcpu->oesp, 0)
		    || helper_PUSH<2>(msg, &old_efl, 0)
		    || helper_PUSH<2>(msg, &cs_sel, 0)
		    || helper_PUSH<2>(msg, &msg.cpu->eip, 0)
		    || has_errorcode && helper_PUSH<2>(msg, &error_code, 0)
		    || set_segment(msg, &msg.cpu->ds, 0)
		    || set_segment(msg, &msg.cpu->es, 0) 
		    || set_segment(msg, &msg.cpu->fs, 0) 
		    || set_segment(msg, &msg.cpu->gs, 0)
		    )
		  {
		    Logging::printf("failed to traverse %x!\n", msg.vcpu->fault);
		    // rollback efl+SS change
		    msg.cpu->efl = old_efl;
		    set_realmode_segment(&msg.cpu->ss, old_ss, true);
		    break;
		  }
		desc.to_cpustate(&msg.cpu->cs, idt.base0);
		msg.cpu->eip = idt.offset();
		//Logging::printf("eip %x esp %x old esp %x dsbase %x oldefl %x oldeip %x\n", msg.cpu->eip, msg.cpu->esp, new_esp, msg.cpu->ds.base, old_efl, msg.vcpu->oeip);
		break;
	      }
	    break;
	  case 0x07: // trap gate 16bit
	  case 0x06: // interrupt gate 16bit
	  default:
	    UNIMPLEMENTED;
	  };
	}
    }
  return msg.vcpu->fault;
}


static int helper_INT(MessageExecutor &msg, void *tmp_src) { return idt_traversal(msg, 0x80000600 | *reinterpret_cast<unsigned char *>(tmp_src), 0); }
static int helper_INVLPG(MessageExecutor &msg, InstructionCacheEntry *entry) { return msg.vcpu->fault; }
static int helper_FWAIT(MessageExecutor &msg)                                { return msg.vcpu->fault; }
static int helper_MOV__DB0__EDX(MessageExecutor &msg, InstructionCacheEntry *entry) 
{
  unsigned dbreg = (entry->data[entry->offset_opcode] >> 3) & 0x7;
  if ((dbreg == 4 || dbreg == 5) && ~msg.cpu->cr4 & 0x8)
    dbreg += 2;

  void *tmp_src;
  switch (dbreg)
    {
    case 0 ... 3: tmp_src = &msg.vcpu->dr[dbreg]; break;
    case 6: tmp_src = &msg.vcpu->dr6; break;
    case 7: tmp_src = &msg.cpu->dr7; break;
    default: UD0; 
    }
  move<2>(get_gpr(msg, (entry->data[entry->offset_opcode]) & 0x7, 0), tmp_src);
  return msg.vcpu->fault;
}
static int helper_MOV__EDX__DB0(MessageExecutor &msg, InstructionCacheEntry *entry) 
{
  unsigned dbreg = (entry->data[entry->offset_opcode] >> 3) & 0x7;
  if ((dbreg == 4 || dbreg == 5) && ~msg.cpu->cr4 & 0x8)
    dbreg += 2;
  unsigned value;
  move<2>(&value, get_gpr(msg, (entry->data[entry->offset_opcode]) & 0x7, 0));
  switch (dbreg)
    {
    case 0 ... 3: msg.vcpu->dr[dbreg] = value; break;
    case 6: msg.vcpu->dr6 = (value & ~0x1000) | 0xffff0ff0; break; 
    case 7: msg.cpu->dr7  = (value & ~0xd800) | 0x400; break;
    default: UD0; 
    }
  return msg.vcpu->fault;
}



/**
 * fxsave.
 * Missing: #AC for unaligned access
 */
static int helper_FXSAVE(MessageExecutor &msg, InstructionCacheEntry *entry)
{
  unsigned virt = modrm2virt(msg, entry);
  if (virt & 0xf) GP0; // could be also AC if enabled
  for (unsigned i=0; i < sizeof(msg.vcpu->fpustate)/sizeof(unsigned); i++)
    {
      void *addr;							
      if (!virt_to_ptr(msg, entry, addr, 4, user_access(msg, TYPE_W), virt + i*sizeof(unsigned)))  return msg.vcpu->fault;
      move<2>(addr, reinterpret_cast<unsigned *>(msg.vcpu->fpustate)+i);
    }
  return msg.vcpu->fault;
}


static int helper_FRSTOR(MessageExecutor &msg, InstructionCacheEntry *entry)
{
  unsigned virt = modrm2virt(msg, entry);
  UNIMPLEMENTED;
}
