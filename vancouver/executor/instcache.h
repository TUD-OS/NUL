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
};

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
  void __attribute__((regparm(3))) (*execute)(InstructionCache *instr, void *tmp_src, void *tmp_dst);
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


  // cpu state
  VCpu   * _vcpu;
  unsigned _oeip;
  unsigned _oesp;
  unsigned _ointr_state;
  unsigned _dr6;
  unsigned _dr[4];
  unsigned _fpustate[512/sizeof(unsigned)] __attribute__((aligned(16)));

  int send_message(CpuMessage::Type type)
  {
    CpuMessage msg(type, _cpu, _mtr_in);
    _vcpu->executor.send(msg, true);
    return _fault;
  }

  int event_injection()
  {
    if ((_cpu->inj_info & 0x80000000) && !idt_traversal(_cpu->inj_info, _cpu->inj_error))
      {
	_cpu->inj_info &= ~0x80000000;
	RETRY;
      }
    return _fault;
  };


  /**
   * Fetch code.
   */
  int fetch_code(InstructionCacheEntry *entry, unsigned len)
  {
    unsigned virt = READ(eip) + entry->inst_len;
    unsigned limit = READ(cs).limit;
    if ((~limit && limit < (virt + len - 1)) || ((entry->inst_len + len) > InstructionCacheEntry::MAX_INSTLEN)) GP0;
    virt += READ(cs).base;

    read_code(virt, len, entry->data + entry->inst_len);
    entry->inst_len += len;
    return _fault;
  }


  /**
   * Find a cache entry for the given state and checks whether it is
   * still valid.
   */
  bool find_entry(unsigned &index)
  {
    unsigned cs_ar = READ(cs).ar;
    unsigned linear = _cpu->eip + READ(cs).base;
    for (unsigned i = slot(linear); i < slot(linear) + ASSOZ; i++)
      if (linear == _tags[i] &&  _values[i].inst_len)
	{
	  InstructionCacheEntry tmp;
	  tmp.inst_len = 0;
	  // revalidate entries
	  if (fetch_code(&tmp, _values[i].inst_len)) return false;

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
  int get_modrm(InstructionCacheEntry *entry)
  {
    fetch_code(entry, 1);
    unsigned char  modrm = entry->data[entry->inst_len - 1];
    unsigned short info = modrminfo[(entry->address_size == 2) << 5 | (modrm >> 3) & 0x18 | modrm & 0x7];

    // sib byte
    if (info & MRM_SIB)
      {
	fetch_code(entry, 1);
	if ((modrm & 0xc7) == 0x4 && (entry->data[entry->inst_len - 1] & 0x7) == 5) info |= MRM_DISP32 | MRM_NOBASE;
	info = info & ~0xff | entry->data[entry->inst_len - 1];
	if (((info >> 3) & 0xf) == 4) info |= MRM_NOINDEX;
      }
    unsigned disp = ((info >> MRM_DISPSHIFT) & 0x3);
    if (disp)  fetch_code(entry, 1 << (disp-1));
    entry->modrminfo = info;

    // in 16bit addressing mode bp-references are using the stack segment as default
    if ((entry->address_size == 1) && ((entry->prefixes & 0xff00) == 0x8300) && ((info & 0xf0) == 0x50))
      entry->prefixes = (entry->prefixes & ~0xff00) | 0x200;

    return _fault;
  }



#include "insthelper.h"
#include "instructions.h"
#include "instructions.inc"

public:
  /**
   * Decode the instruction.
   */
  int get_instruction(InstructionCacheEntry *&entry)
  {
    //COUNTER_INC("INSTR");
    unsigned index = 0;
    if (!find_entry(index) && !_fault)
      {
	entry = _values + index;
	entry->address_size = entry->operand_size = ((entry->cs_ar >> 10) & 1) + 1;
	for (int op_mode = 0; !entry->execute && !_fault; )
	  {
	    /**
	     * Handle a new byte of the instruction.
	     *
	     * The op_mode, keeps track which parts of the opcode bytes have
	     * already been seen.  Negative if the whole instruction is fetched.
	     */
	    fetch_code(entry, 1) || handle_code_byte(entry, entry->data[entry->inst_len-1], op_mode);
	  }
	if (_fault)
	  {
	    // invalidate entry
	    entry->inst_len = 0;
	    Logging::printf("decode fault %x\n", _fault);
	    return _fault;
	  }

	assert(_values[index].execute);
	//COUNTER_INC("decoded");
      }
    entry = _values + index;
    _cpu->eip += entry->inst_len;
    if (debug)
      {
	Logging::printf("eip %x:%x esp %x eax %x ebp %x prefix %x\n", _cpu->cs.sel, _oeip, _oesp, _cpu->eax, _cpu->ebp, entry->prefixes);
	if (_oeip == 0xf69)
	  Logging::panic("done bp %x\n", _cpu->ebp);
	Logging::printf(".byte ");
	for (unsigned i = 0; i < entry->inst_len; i++)
	    Logging::printf("0x%02x%c", entry->data[i], (i == entry->inst_len - 1) ? '\n' : ',');
      }
    return _fault;
  }

  unsigned *get_reg32(unsigned reg)
  {
    return _cpu->gpr + reg;
  }

  /**
   * Get a GPR.
   */
  template<bool bytereg>
  void *get_reg(unsigned reg)
  {
    void *res = _cpu->gpr + reg;
    if (bytereg && reg >= 4 && reg < 8)
      res = reinterpret_cast<char *>(_cpu->gpr+(reg & 0x3)) + ((reg & 0x4) >> 2);
    return res;
  }



  unsigned modrm2virt(InstructionCacheEntry *entry)
  {
    unsigned short info = entry->modrminfo;
    unsigned virt = 0;
    unsigned char *disp_offset = entry->data + entry->offset_opcode + 1;
    if (info & MRM_SIB)
      {
	// add base + scaled index
	if (~info & MRM_NOBASE)   virt += _cpu->gpr[info & 0x7];
	if (~info & MRM_NOINDEX)  virt += _cpu->gpr[(info >> 3) & 0x7] << ((info >> 6) & 0x3);
	disp_offset++;
      }
    else
      {
	if (info & 0xf || info & MRM_EAX) virt += _cpu->gpr[info & 0x7];
	if (info & 0xf0) virt += _cpu->gpr[(info >> 4) & 0x7];
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
	unsigned bitofs = *get_reg32((entry->data[entry->offset_opcode] >> 3) & 0x7);
	virt += (bitofs >> 3) & ~((1 << entry->operand_size) - 1);
      }
    return virt;
  }


  int virt_to_ptr(InstructionCacheEntry *entry, void *&res, unsigned length, Type type, unsigned virt)
  {
    InstructionCache::handle_segment(entry, (&_cpu->es) + ((entry->prefixes >> 8) & 0x0f), virt, length, type & TYPE_W)
      || prepare_virtual(virt, length, type, res);
    return _fault;
  }


  /**
   * Convert modrm to a pointer in cache or RAM.
   */
  int modrm2mem(InstructionCacheEntry *entry, void *&res, unsigned length, Type type)
  {
    unsigned short info = entry->modrminfo;
    if (info & MRM_REG)
	res = length == 1 ? get_reg<1>(info & 0x7) : get_reg<0>(info & 0x7);
    else
      virt_to_ptr(entry, res, length, type, modrm2virt(entry));
    return _fault;
  }


  void call_asm(void *tmp_src, void *tmp_dst, InstructionCacheEntry *entry)
  {
    unsigned tmp_flag;
    unsigned dummy1, dummy2, dummy3;
    switch (entry->flags & (IC_LOADFLAGS | IC_SAVEFLAGS))
      {
      case IC_SAVEFLAGS:
	asm volatile ("call *%4; pushf; pop %3"
		      : "=a"(dummy1), "=d"(dummy2), "=c"(dummy3), "=g"(tmp_flag)
		      : "m"(entry->execute), "0"(this), "1"(tmp_src), "2"(tmp_dst) : "memory");
	_cpu->efl = (_cpu->efl & ~0x8d5) | (tmp_flag  & 0x8d5);
	break;
      case IC_LOADFLAGS:
	tmp_flag = _cpu->efl & 0x8d5;
	asm volatile ("push %3; popf; call *%4;"
		      : "=a"(dummy1), "=d"(dummy2), "=c"(dummy3), "+g"(tmp_flag)
		      : "m"(entry->execute), "0"(this), "1"(tmp_src), "2"(tmp_dst) : "memory");
	break;
      case IC_LOADFLAGS | IC_SAVEFLAGS:
	tmp_flag = _cpu->efl & 0x8d5;
	asm volatile ("push %3; popf; call *%4; pushf; pop %3"
		      : "=a"(dummy1), "=d"(dummy2), "=c"(dummy3), "+g"(tmp_flag)
		      : "m"(entry->execute), "0"(this), "1"(tmp_src), "2"(tmp_dst) : "memory");
	_cpu->efl = (_cpu->efl & ~0x8d5) | (tmp_flag  & 0x8d5);
	break;
      default:
	asm volatile ("call *%3;"
		      : "=a"(dummy1), "=d"(dummy2), "=c"(dummy3)
		      : "m"(entry->execute), "0"(this), "1"(tmp_src), "2"(tmp_dst) : "memory");
	break;
      }
  }


  /**
   * Execute the instruction.
   */
  int execute(InstructionCacheEntry *entry)
  {

    //COUNTER_INC("executed");
    assert(entry->execute);
    bool is_byte = entry->flags & IC_BYTE;
    void *tmp_src = entry->src;
    void *tmp_dst = entry->dst;

    if (((entry->prefixes & 0xff) == 0xf0) && ((~entry->flags & IC_LOCK) || (entry->modrminfo & MRM_REG)))
      {
	Logging::panic("LOCK prefix %02x%02x%02x%02x at eip %x\n", entry->data[0], entry->data[1], entry->data[2], entry->data[3], _cpu->eip);
	UD0;
      }

    Type type = TYPE_R;
    if (!(entry->flags & (IC_DIRECTION | IC_READONLY))) type = TYPE_W;
    if (entry->flags & IC_RMW) type = Type(type | TYPE_R);
    if (entry->flags & IC_MODRM)
      {
	if (modrm2mem(entry, tmp_dst, is_byte ? 1 : 1 << entry->operand_size, type)) return _fault;
      }
    if (entry->flags & IC_MOFS)
      {
	unsigned virt = 0;
	move(&virt, entry->data+entry->offset_opcode, entry->address_size);
	if (virt_to_ptr(entry, tmp_dst, is_byte ? 1 : 1 << entry->operand_size, type, virt)) return _fault;
      }
    if (entry->flags & IC_DIRECTION)
      {
	void *tmp = tmp_src;
	tmp_src = tmp_dst;
	tmp_dst = tmp;
      }
    if (entry->flags & IC_ASM)
      call_asm(tmp_src, tmp_dst, entry);
    else
      entry->execute(this, tmp_src, tmp_dst);

    /**
     * Have we accessed more than we are allowed to?
     * Do a recall with more state.
     */
    if (_mtr_read & ~_mtr_in)
      {
	Logging::printf("recall %x out of %x\n", _mtr_read, _mtr_in);
	// signal a recall
	//COUNTER_INC("recall");
	FAULT(this, FAULT_RECALL);
      };
    return _fault;
  }


  /**
   * Commits the instruction by setting the appropriate UTCB fields.
   */
  bool commit(InstructionCacheEntry *entry)
  {
    // irq blocking propagation
    if (_fault)  _cpu->intr_state = _ointr_state;
    if (_cpu->intr_state != _ointr_state)
      _mtr_out |= MTD_STATE;

    if (!_fault || _fault == FAULT_RETRY)
      {
	// successfull
	//invalidate(true);
      }
    else
      {
	// XXX this looks fishy invalidate(false);
	_cpu->eip = _oeip;
	_cpu->esp = _oesp;
	if (_fault > 0)
	  {
	    if (entry)  _cpu->inst_len = entry->inst_len; else _cpu->inst_len = 0;
	    switch (_fault)
	      {
	      case FAULT_UNIMPLEMENTED:
		Logging::printf("unimplemented at line %d eip %x\n", _debug_fault_line, _cpu->eip);
		// unimplemented
		return false;
	      default:
		Logging::panic("internal fault %x at eip %x\n", _fault, _cpu->eip);
	      }
	  }
	else
	  {
	    assert(_fault & 0x80000000);
	    //XXX overwrite mtr_write to set only inj_info

	    Logging::printf("fault: %x old %x error %x at eip %x line %d %x\n", _fault, _cpu->inj_info,
			    _error_code, _cpu->eip, _debug_fault_line, _cpu->cr2);
	    // consolidate two exceptions

	    // triple fault ?
	    unsigned old_info = _cpu->inj_info & ~INJ_IRQWIN;
	    if (old_info == 0x80000b08)
	      {
		_cpu->inj_info = (_cpu->inj_info & INJ_IRQWIN);
		// triple fault
		CpuMessage msg(CpuMessage::TYPE_TRIPLE, _cpu, _mtr_in);
		_vcpu->executor.send(msg, true);
	      }
	    else
	      {
		if ((old_info & _fault & 0x80000700) == 0x80000300)
		  if ((0x3c01 & (1 << (old_info & 0xff))) && (0x3c01 & (1 << (_fault & 0xff)))
		      || (old_info == 0x80000b0e && (0x7c01 & (1 << (_fault & 0xff)))))
		    {
		      _fault = 0x80000b08;
		      _error_code = 0;
		    }
		_cpu->inj_info = _fault | (_cpu->inj_info & INJ_IRQWIN);
		_cpu->inj_error = _error_code;
	      }
	  }
      }
    //_cpu->head.mtr = Mtd(mtr_write, 0);
    return true;
  }

public:
  bool enter(CpuMessage &msg) {
    _cpu = msg.cpu;
    _mtr_in = msg.mtr_in;
    _mtr_out =  0;
    return init();
  }


  bool step() {

    InstructionCacheEntry *entry = 0;
    _fault = 0;
    _oeip = _cpu->eip;
    _oesp = _cpu->esp;
    _ointr_state = _cpu->intr_state;
    // remove sti+movss blocking
    _cpu->intr_state &= ~3;
    event_injection() || get_instruction(entry) || execute(entry);
    return commit(entry);
  }

  bool leave() {
    invalidate(true);
    return true;
  }

 InstructionCache(Motherboard &mb, VCpu *vcpu) : MemTlb(mb), _values(), _vcpu(vcpu) { }
};
