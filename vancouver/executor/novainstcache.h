/**
 * InstructionCache for NovaHalifax.
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
#pragma once

/**
 * Reverse MTR mapping.
 */
enum
  {
    RMTR_eip = MTD_RIP_LEN,
    RMTR_efl = MTD_RFLAGS,
    RMTR_cr0 = MTD_CR,
    RMTR_cr2 = MTD_CR,
    RMTR_cr3 = MTD_CR,
    RMTR_cr4 = MTD_CR,
    RMTR_cs  = MTD_CS_SS,
    RMTR_ss  = MTD_CS_SS,
  };

/**
 * Faults.
 */
enum {
  FAULT_NOERROR,
  FAULT_RETRY,
  FAULT_RECALL,
  FAULT_UNIMPLEMENTED,
  FAULT_CPUID,
  FAULT_RDTSC,
  FAULT_RDMSR,
  FAULT_WRMSR,
  FAULT_HLT,
  FAULT_INVD,
  FAULT_WBINVD,
};


#define READ(NAME) ({ msg.vcpu->mtr_read |= RMTR_##NAME; msg.cpu->NAME; })
#define WRITE(NAME) ({							\
      msg.vcpu->mtr_write |= RMTR_##NAME;				\
      msg.cpu->NAME;							\
})
#define VCPU(NAME) (msg.vcpu->NAME)
#define FAULT(VALUE) { msg.vcpu->debug_fault_line = __LINE__;  msg.vcpu->fault = VALUE; }
#define UNIMPLEMENTED { return (FAULT(FAULT_UNIMPLEMENTED)); }
#define RETRY         { return (FAULT(FAULT_RETRY)); }
#define EXCEPTION0(NR) { msg.vcpu->error_code = 0; FAULT(0x80000300 | NR); }
#define EXCEPTION(NR, ERROR) { msg.vcpu->error_code = ERROR; FAULT(0x80000b00 | NR); }
#define DE0   { EXCEPTION0(0x0); }
#define UD0   { EXCEPTION0(0x6); return msg.vcpu->fault; }
#define NP(X) { EXCEPTION(0xb, X); return msg.vcpu->fault; }
#define SS(X) { EXCEPTION(0xc, X); return msg.vcpu->fault; }
#define SS0   { EXCEPTION(0xc, 0); return msg.vcpu->fault; }
#define GP(X) { EXCEPTION(0xd, X); return msg.vcpu->fault; }
#define GP0   { EXCEPTION(0xd, 0); return msg.vcpu->fault; }
#define PF(ADDR, ERR) { msg.cpu->cr2 = ADDR; EXCEPTION(0xe, ERR); return msg.vcpu->fault; }

#include "memtlb.h"


enum {
  MRM_EAX    = 1 << 8,
  MRM_REG    = 1 << 9,
  MRM_SIB    = 1 << 10,
  MRM_DISPSHIFT = 12,
  MRM_DISP08 = 1 << MRM_DISPSHIFT,
  MRM_DISP16 = 2 << MRM_DISPSHIFT,
  MRM_DISP32 = 3 << MRM_DISPSHIFT,
  MRM_NOBASE  = 1 << 14,
  MRM_NOINDEX = 1 << 15,
};

/**
 * Lookup table for modrm decoding.
 */
static const unsigned short modrminfo[64] =
  {
    0x36               , 0x37             ,  0x56             ,  0x57             ,  0x06             ,  0x07             ,         MRM_DISP16,  0x03             ,
    0x36 | MRM_DISP08  , 0x37 | MRM_DISP08,  0x56 | MRM_DISP08,  0x57 | MRM_DISP08,  0x06 | MRM_DISP08,  0x07 | MRM_DISP08,  0x50 | MRM_DISP08,  0x03 | MRM_DISP08,
    0x36 | MRM_DISP16  , 0x37 | MRM_DISP16,  0x56 | MRM_DISP16,  0x57 | MRM_DISP16,  0x06 | MRM_DISP16,  0x07 | MRM_DISP16,  0x50 | MRM_DISP16,  0x03 | MRM_DISP16,
    MRM_EAX| MRM_REG   , 0x01 | MRM_REG   ,  0x02 | MRM_REG   ,  0x03 | MRM_REG   ,  0x04 | MRM_REG   ,  0x05 | MRM_REG   ,  0x06 | MRM_REG   ,  0x07 | MRM_REG   ,
    MRM_EAX            , 0x01             ,  0x02             ,  0x03             ,  MRM_SIB            ,       MRM_DISP32,  0x06             ,  0x07             ,
    MRM_EAX| MRM_DISP08, 0x01 | MRM_DISP08,  0x02 | MRM_DISP08,  0x03 | MRM_DISP08,  MRM_SIB| MRM_DISP08,0x05 | MRM_DISP08,  0x06 | MRM_DISP08,  0x07 | MRM_DISP08,
    MRM_EAX| MRM_DISP32, 0x01 | MRM_DISP32,  0x02 | MRM_DISP32,  0x03 | MRM_DISP32,  MRM_SIB| MRM_DISP32,0x05 | MRM_DISP32,  0x06 | MRM_DISP32,  0x07 | MRM_DISP32,
    MRM_EAX| MRM_REG   , 0x01 | MRM_REG   ,  0x02 | MRM_REG   ,  0x03 | MRM_REG   ,  0x04 | MRM_REG   ,  0x05 | MRM_REG   ,  0x06 | MRM_REG   ,  0x07 | MRM_REG   ,
  };


/**
 * The data that is cached between different runs.
 */
struct InstructionCacheEntry
{
  enum {
    MAX_INSTLEN = 15,
  };
  // the index into data where the main operand byte lives also used to find the MODRM byte
  unsigned char offset_opcode;
  unsigned char data[MAX_INSTLEN];
  unsigned flags;
  unsigned inst_len;
  unsigned operand_size;
  unsigned address_size;
  unsigned modrminfo;
  unsigned cs_ar;
  unsigned prefixes;
  void __attribute__((regparm(3))) (*execute)(MessageExecutor &msg, void *tmp_src, void *tmp_dst);
  void     *src;
  void     *dst;
  unsigned immediate;
};



/**
 * An instruction cache that keeps decoded instructions.
 */
class InstructionCache : public MemTlb
{
  enum EFLAGS {
    EFL_ZF  = 1 <<  6,
    EFL_TF  = 1 <<  8,
    EFL_IF  = 1 <<  9,
    EFL_OF  = 1 << 11,
    EFL_IOPL= 3 << 12,
    EFL_NT  = 1 << 14,
    EFL_RF  = 1 << 16,
    EFL_VM  = 1 << 17,
    EFL_AC  = 1 << 18,
    EFL_VIF = 1 << 19,
    EFL_VIP = 1 << 20,
  };


  enum {
    IC_ASM       = 1 <<  0,
    IC_SAVEFLAGS = 1 <<  1,
    IC_LOADFLAGS = 1 <<  2,
    IC_MODRM     = 1 <<  3,
    IC_DIRECTION = 1 <<  4,
    IC_READONLY  = 1 <<  5,
    IC_BYTE      = 1 <<  6,
    IC_LOCK      = 1 <<  7,
    IC_BITS      = 1 <<  8,
    IC_RMW       = 1 <<  9,
    IC_MOFS      = 1 << 10,
  };


  enum {
    SIZE = 64,
    ASSOZ = 4,
  };

  unsigned _pos;
  unsigned _tags[SIZE*ASSOZ];
  InstructionCacheEntry _values[SIZE*ASSOZ];
  unsigned slot(unsigned tag) { return ((tag ^ (tag/SIZE)) % SIZE) * ASSOZ; }



  int event_injection(MessageExecutor &msg)
  {
    if ((msg.cpu->inj_info & 0x80000000) && !idt_traversal(msg, msg.cpu->inj_info, msg.cpu->inj_error))
      {
	msg.cpu->inj_info &= ~0x80000000;
	RETRY;
      }
    return msg.vcpu->fault;
  };


  /**
   * Fetch code.
   */
  int fetch_code(MessageExecutor &msg, InstructionCacheEntry *entry, unsigned len)
  {
    unsigned virt = READ(eip) + entry->inst_len;
    unsigned limit = READ(cs).limit;
    if ((~limit && limit < (virt + len - 1)) || ((entry->inst_len + len) > InstructionCacheEntry::MAX_INSTLEN)) GP0;
    virt += READ(cs).base;

    read_code(msg, virt, len, entry->data + entry->inst_len);
    entry->inst_len += len;
    return msg.vcpu->fault;
  }


  /**
   * Find a cache entry for the given state and checks whether it is
   * still valid.
   */
  bool find_entry(MessageExecutor &msg, unsigned &index) __attribute__((noinline))
  {
    unsigned cs_ar = READ(cs).ar;
    unsigned linear = msg.cpu->eip + READ(cs).base;
    for (unsigned i = slot(linear); i < slot(linear) + ASSOZ; i++)
      if (linear == _tags[i] &&  _values[i].inst_len)
	{
	  InstructionCacheEntry tmp;
	  tmp.inst_len = 0;
	  // revalidate entries
	  if (fetch_code(msg, &tmp, _values[i].inst_len)) return false;

	  // either code modified or two entries with different bases?
	  if (memcmp(tmp.data, _values[i].data, _values[i].inst_len) || cs_ar != _values[i].cs_ar)  continue;
	  index = i;
	  //COUNTER_INC("I$ ok");
	  return true;
	}
    // allocate new invalid entry
    index = slot(linear) + (_pos++ % ASSOZ);
    memset(_values + index, 0, sizeof(*_values));
    _values[index].cs_ar =  cs_ar;
    _values[index].prefixes = 0x8300; // default is to use the DS segment
    _tags[index] = linear;
    return false;
  }


  /**
   * Fetch the modrm byte including sib byte and displacement.
   */
  int get_modrm(MessageExecutor &msg, InstructionCacheEntry *entry)
  {
    fetch_code(msg, entry, 1);
    unsigned char  modrm = entry->data[entry->inst_len - 1];
    unsigned short info = modrminfo[(entry->address_size == 2) << 5 | (modrm >> 3) & 0x18 | modrm & 0x7];

    // sib byte
    if (info & MRM_SIB)
      {
	fetch_code(msg, entry, 1);
	if ((modrm & 0xc7) == 0x4 && (entry->data[entry->inst_len - 1] & 0x7) == 5) info |= MRM_DISP32 | MRM_NOBASE;
	info = info & ~0xff | entry->data[entry->inst_len - 1];
	if (((info >> 3) & 0xf) == 4) info |= MRM_NOINDEX;
      }
    unsigned disp = ((info >> MRM_DISPSHIFT) & 0x3);
    if (disp)  fetch_code(msg, entry, 1 << (disp-1));
    entry->modrminfo = info;

    // in 16bit addressing mode bp-references are using the stack segment as default
    if ((entry->address_size == 1) && ((entry->prefixes & 0xff00) == 0x8300) && ((info & 0xf0) == 0x50))
      entry->prefixes = (entry->prefixes & ~0xff00) | 0x200;

    return msg.vcpu->fault;
  }


#include "insthelper.h"
#include "instructions.h"
#include "executor/instructions.inc"

public:
  /**
   * Decode the instruction.
   */
  int get_instruction(MessageExecutor &msg, InstructionCacheEntry *&entry)
  {
    //COUNTER_INC("INSTR");
    unsigned index = 0;
    if (!find_entry(msg, index) && !msg.vcpu->fault)
      {
	entry = _values + index;
	entry->address_size = entry->operand_size = ((entry->cs_ar >> 10) & 1) + 1;
	for (int op_mode = 0; !entry->execute && !msg.vcpu->fault; )
	  {
	    /**
	     * Handle a new byte of the instruction.
	     *
	     * The op_mode, keeps track which parts of the opcode bytes have
	     * already been seen.  Negative if the whole instruction is fetched.
	     */
	    fetch_code(msg, entry, 1) || handle_code_byte(msg, entry, entry->data[entry->inst_len-1], op_mode);
	  }
	if (msg.vcpu->fault)
	  {
	    // invalidate entry
	    entry->inst_len = 0;
	    Logging::printf("decode fault %x\n", msg.vcpu->fault);
	    return msg.vcpu->fault;
	  }

	assert(_values[index].execute);
	//COUNTER_INC("decoded");
      }
    entry = _values + index;
    msg.cpu->eip += entry->inst_len;
    if (debug)
      {
	Logging::printf("eip %x:%x esp %x eax %x ebp %x prefix %x\n", msg.cpu->cs.sel, msg.vcpu->oeip, msg.vcpu->oesp, msg.cpu->eax, msg.cpu->ebp, entry->prefixes);
	if (msg.vcpu->oeip == 0xf69)
	  Logging::panic("done bp %x\n", msg.cpu->ebp);
	Logging::printf(".byte ");
	for (unsigned i = 0; i < entry->inst_len; i++)
	    Logging::printf("0x%02x%c", entry->data[i], (i == entry->inst_len - 1) ? '\n' : ',');
      }
    return msg.vcpu->fault;
  }
    /**
   * Get a GPR.
   */
  static void *get_gpr(MessageExecutor &msg, unsigned reg, bool bytereg)
  {
    void *res = msg.cpu->gpr + reg;
    if (bytereg && reg >= 4 && reg < 8)
      res = reinterpret_cast<char *>(msg.cpu->gpr+(reg & 0x3)) + ((reg & 0x4) >> 2);
    return res;
  }


  static unsigned modrm2virt(MessageExecutor &msg, InstructionCacheEntry *entry)
  {
    unsigned short info = entry->modrminfo;
    unsigned virt = 0;
    unsigned char *disp_offset = entry->data + entry->offset_opcode + 1;
    if (info & MRM_SIB)
      {
	// add base + scaled index
	if (~info & MRM_NOBASE)   virt += msg.cpu->gpr[info & 0x7];
	if (~info & MRM_NOINDEX)  virt += msg.cpu->gpr[(info >> 3) & 0x7] << ((info >> 6) & 0x3);
	disp_offset++;
      }
    else
      {
	if (info & 0xf || info & MRM_EAX) virt += msg.cpu->gpr[info & 0x7];
	if (info & 0xf0) virt += msg.cpu->gpr[(info >> 4) & 0x7];
      }

    unsigned disp = ((info >> MRM_DISPSHIFT) & 0x3);
    switch (disp)
      {
      case 1:  virt += *reinterpret_cast<char  *>(disp_offset); break;
      case 2:  virt += *reinterpret_cast<short *>(disp_offset); break;
      case 3:  virt += *reinterpret_cast<int   *>(disp_offset); break;
      default:
	break;
      }
    if (entry->flags & IC_BITS)
      {
	unsigned bitofs;
	move<2>(&bitofs, get_gpr(msg, (entry->data[entry->offset_opcode] >> 3) & 0x7, 0));
	virt += (bitofs >> 3) & ~((1 << entry->operand_size) - 1);
      }
    return virt;
  }


  static int virt_to_ptr(MessageExecutor &msg, InstructionCacheEntry *entry, void *&res, unsigned length, Type type, unsigned virt)
  {
    InstructionCache::handle_segment(msg, entry, (&msg.cpu->es) + ((entry->prefixes >> 8) & 0x0f), virt, length, type & TYPE_W) 
      || msg.vcpu->instcache->prepare_virtual(msg, virt, length, type, res);
    return msg.vcpu->fault;
  }


  /**
   * Convert modrm to a pointer in cache or RAM.
   */
  static int modrm2mem(MessageExecutor &msg, InstructionCacheEntry *entry, void *&res, unsigned length, Type type)
  {
    unsigned short info = entry->modrminfo;
    if (info & MRM_REG)
      res = get_gpr(msg, info & 0x7, length == 1);
    else
      virt_to_ptr(msg, entry, res, length, type, modrm2virt(msg, entry));
    return msg.vcpu->fault;
  }


  static void __attribute__((regparm(3))) call_asm(MessageExecutor &msg, void *tmp_src, void *tmp_dst, InstructionCacheEntry *entry)
  {
    unsigned tmp_flag;
    unsigned dummy1, dummy2, dummy3;
    switch (entry->flags & (IC_LOADFLAGS | IC_SAVEFLAGS))
      {
      case IC_SAVEFLAGS:
	asm volatile ("call *%4; pushf; pop %3"
		      : "=a"(dummy1), "=d"(dummy2), "=c"(dummy3), "=g"(tmp_flag)
		      : "m"(entry->execute), "0"(&msg), "1"(tmp_src), "2"(tmp_dst));
	msg.cpu->efl = (msg.cpu->efl & ~0x8d5) | (tmp_flag  & 0x8d5);
	break;
      case IC_LOADFLAGS:
	tmp_flag = msg.cpu->efl & 0x8d5;
	asm volatile ("push %3; popf; call *%4;"
		      : "=a"(dummy1), "=d"(dummy2), "=c"(dummy3), "+g"(tmp_flag)
		      : "m"(entry->execute), "0"(&msg), "1"(tmp_src), "2"(tmp_dst));
	break;
      case IC_LOADFLAGS | IC_SAVEFLAGS:
	tmp_flag = msg.cpu->efl & 0x8d5;
	asm volatile ("push %3; popf; call *%4; pushf; pop %3"
		      : "=a"(dummy1), "=d"(dummy2), "=c"(dummy3), "+g"(tmp_flag)
		      : "m"(entry->execute), "0"(&msg), "1"(tmp_src), "2"(tmp_dst));
	msg.cpu->efl = (msg.cpu->efl & ~0x8d5) | (tmp_flag  & 0x8d5);
	break;
      default:
	asm volatile ("call *%3;"
		      : "=a"(dummy1), "=d"(dummy2), "=c"(dummy3)
		      : "m"(entry->execute), "0"(&msg), "1"(tmp_src), "2"(tmp_dst));
	break;
      }
  }


  /**
   * Execute the instruction.
   */
  static int execute(MessageExecutor &msg, InstructionCacheEntry *entry)
  {

    //COUNTER_INC("executed");
    assert(entry->execute);
    bool is_byte = entry->flags & IC_BYTE;
    void *tmp_src = entry->src;
    void *tmp_dst = entry->dst;

    if (((entry->prefixes & 0xff) == 0xf0) && ((~entry->flags & IC_LOCK) || (entry->modrminfo & MRM_REG)))
      {
	Logging::panic("LOCK prefix %x at eip %x\n", *reinterpret_cast<unsigned *>(entry->data), msg.cpu->eip);
	UD0;
      }

    Type type = TYPE_R;
    if (!(entry->flags & (IC_DIRECTION | IC_READONLY))) type = TYPE_W;
    if (entry->flags & IC_RMW) type = Type(type | TYPE_R);
    if (entry->flags & IC_MODRM)
      {
	if (modrm2mem(msg, entry, tmp_dst, is_byte ? 1 : 1 << entry->operand_size, type)) return msg.vcpu->fault;
      }
    if (entry->flags & IC_MOFS)
      {
	unsigned virt = 0;
	move(&virt, entry->data+entry->offset_opcode, entry->address_size);
	if (virt_to_ptr(msg, entry, tmp_dst, is_byte ? 1 : 1 << entry->operand_size, type, virt)) return msg.vcpu->fault;
      }
    if (entry->flags & IC_DIRECTION)
      {
	void *tmp = tmp_src;
	tmp_src = tmp_dst;
	tmp_dst = tmp;
      }
    if (entry->flags & IC_ASM)
      call_asm(msg, tmp_src, tmp_dst, entry);
    else
      entry->execute(msg, tmp_src, tmp_dst);

    /**
     * Have we accessed more than we are allowed to?
     * Do a recall with more state.
     */
    if (msg.vcpu->mtr_read & ~msg.cpu->head.mtr.value())
      {
	Logging::printf("recall %x out of %x\n", msg.vcpu->mtr_read, msg.cpu->head.mtr.value());
	// signal a recall
	//COUNTER_INC("recall");
	FAULT(FAULT_RECALL);
      };
    return msg.vcpu->fault;
  }


  /**
   * Commits the instruction by setting the appropriate UTCB fields.
   */
  bool commit(MessageExecutor &msg, InstructionCacheEntry *entry)
  {
    // irq blocking propagation
    if (msg.vcpu->fault)  msg.cpu->actv_state = msg.vcpu->oactv_state;
    if (msg.cpu->actv_state != msg.vcpu->oactv_state)
      {
	msg.vcpu->mtr_read  |= MTD_STATE;
	msg.vcpu->mtr_write |= MTD_STATE;
      }

    if (!msg.vcpu->fault || msg.vcpu->fault == FAULT_RETRY)
      {
	// successfull
	invalidate(true);
	msg.cpu->head.pid = 0;
      }
    else
      {
	invalidate(false);
	msg.cpu->eip = msg.vcpu->oeip;
	msg.cpu->esp = msg.vcpu->oesp;
	if (msg.vcpu->fault > 0)
	  {
	    if (entry)  msg.cpu->inst_len = entry->inst_len; else msg.cpu->inst_len = 0;
	    switch (msg.vcpu->fault)
	      {
	      case FAULT_UNIMPLEMENTED:
		Logging::printf("unimplemented at line %d eip %x\n", msg.vcpu->debug_fault_line, msg.cpu->eip);
		// unimplemented
		return false;
	      case FAULT_CPUID:
		// forward to the cpuid portal
		// XXX unify portal-ID and faultNR
		msg.cpu->head.pid = 10;
		break;
		// XXX own exits
	      case FAULT_WBINVD:
	      case FAULT_INVD:
	      case FAULT_HLT:
		msg.cpu->head.pid = 12;
		break;
	      case FAULT_RDTSC:
		// forward to the rdtsc portal
		msg.cpu->head.pid = 16;
		break;
	      case FAULT_RDMSR:
		msg.cpu->head.pid = 31;
		break;
	      case FAULT_WRMSR:
		msg.cpu->head.pid = 32;
		break;
	      default:
		Logging::panic("internal fault %x at eip %x\n", msg.vcpu->fault, msg.cpu->eip);
	      }
	  }
	else
	  {
	    assert(msg.vcpu->fault & 0x80000000);
	    //XXX overwrite mtr_write to set only inj_info

	    Logging::printf("fault: %x old %x error %x at eip %x line %d %x\n", msg.vcpu->fault, msg.cpu->inj_info,
			    msg.vcpu->error_code, msg.cpu->eip, msg.vcpu->debug_fault_line, msg.cpu->cr2);
	    // consolidate two exceptions

	    // triple fault ?
	    unsigned old_info = msg.cpu->inj_info & ~INJ_IRQWIN;
	    if (old_info == 0x80000b08)
	      {
		msg.cpu->inj_info = (msg.cpu->inj_info & INJ_IRQWIN);
		msg.cpu->head.pid = 2;
	      }
	    else
	      {
		if ((old_info & msg.vcpu->fault & 0x80000700) == 0x80000300)
		  if ((0x3c01 & (1 << (old_info & 0xff))) && (0x3c01 & (1 << (msg.vcpu->fault & 0xff)))
		      || (old_info == 0x80000b0e && (0x7c01 & (1 << (msg.vcpu->fault & 0xff)))))
		    {
		      msg.vcpu->fault = 0x80000b08;
		      msg.vcpu->error_code = 0;
		    }
		msg.cpu->inj_info = msg.vcpu->fault | (msg.cpu->inj_info & INJ_IRQWIN);
		msg.cpu->inj_error = msg.vcpu->error_code;
	      }
	  }
      }
    //msg.cpu->head.mtr = Mtd(mtr_write, 0);
    return true;
  }

public:
  bool step(MessageExecutor &msg)
  {
    InstructionCacheEntry *entry = 0;
    msg.vcpu->fault = 0;
    msg.vcpu->oeip = msg.cpu->eip;
    msg.vcpu->oesp = msg.cpu->esp;
    msg.vcpu->oactv_state = msg.cpu->actv_state;
    // remove sti+movss blocking
    msg.cpu->actv_state &= ~3;
    init(msg) || event_injection(msg) || get_instruction(msg, entry) || execute(msg, entry);
    return commit(msg, entry);
  }


 InstructionCache(Motherboard &mb) : MemTlb(mb), _values() { }
};
