#!/usr/bin/env python2
"""Check different patterns to get the encoding of an instruction and
generate optimized handler functions.

We use an compile+disassemble approach to extract the encoding from
the assembler.
"""

import sys, os, tempfile, re
def compile_and_disassemble(str, file, fdict):
    if str not in fdict:
	tmp = tempfile.NamedTemporaryFile(bufsize=0)
	f = os.popen("as --32 -o %s -- 2> /dev/null"%(tmp.name), "w")
	f.write(str+"\n")
	f.close()
	if os.path.exists(tmp.name):
	    f = os.popen("objdump -w -d -z -M no-aliases,att-mnemonic %s"%tmp.name)
	    l = f.readlines()
	    line = (filter(lambda x: len(x) > 2 and x[:2]=="0:", map(lambda x: x.strip(), l)) + [""])[0]
	else:
	    open(tmp.name, "w")
	    line = ""
	fdict[str] = line
	file.write("%s#%s\n"%(str, line))
    else:
	line = fdict[str]
    if line=="":  return None
    l = map(lambda x: x.strip(), line.split("\t"))
    # print >>sys.stderr, str, repr(line), l
    l[1] = l[1].split()
    return l


def no_modrm(res, prefix):
    return prefix not in map(lambda x: x[3], filter(lambda y: "MODRM" in y[1], res))
def no_grp(res, prefix):
    return prefix not in map(lambda x: x[3][:-1], filter(lambda y: "GRP" in y[1], res))

def get_encoding(opcode, file, fdict):
    templates = [("",                 0, lambda x: (" ".join(x[2].split()) == opcode[0] or "COMPLETE" in opcode[1]) and []),
		 ("b",                0, lambda x: ["BYTE"]),
		 ("l",                0, lambda x: []),
		 ("  $0x12345, (%ecx), %edx",   5, lambda x: ["MODRMMEM", "MODRM", "IMMO", "DIRECTION"]),
		 ("  $0x12,    (%edx), %ecx",   2, lambda x: ["MODRMMEM", "MODRM", "IMM1", "DIRECTION"]),
		 ("  $0x12,    %ecx, (%edx)",   2, lambda x: ["MODRMMEM", "MODRM", "IMM1"]),
		 ("  %ecx, (%edx)",   1, lambda x: ["MODRMMEM", "MODRM"]),
		 ("  (%edx), %ecx",   1, lambda x: ["MODRMMEM", "MODRM", "DIRECTION"]),
		 ("  %edx, %ecx",     1, lambda x: x[1][-1] == "d1" or x[1][-1] == "ca" and ["MODRMREG", "MODRM"]),
		 ("  %cx, %bx",       1, lambda x: x[1][0]!= '66' and ["MODRMREG", "MODRM", "WORD"]),
		 ("  %dl, (%edx)",    1, lambda x: ["MODRMMEM", "MODRM", "BYTE"]),
		 ("  (%edx), %dl",    1, lambda x: ["MODRMMEM", "MODRM", "DIRECTION", "BYTE"]),
		 ("  %dl, %dh",       1, lambda x: x[1][-1] == "ca" and ["MODRMREG", "MODRM", "BYTE"]),
		 ("  $0x2, %dl",      1, lambda x: len(x[1]) > 2 and ["IMM1", "MODRM", "GRP", "BYTE"] or len(x[1]) == 2 and ["IMM1", "IMPL", "BYTE"]),
		 ("  $0x12345678",    4, lambda x: ["IMMO"]),
		 ("  $0x1234",        2, lambda x: x[1][:-4] not in map(lambda y: y[3], res) and ["IMM2"]),
		 ("  $0x12",          1, lambda x: x[1][:-2] not in map(lambda y: y[3], res) and ["IMM1"]),
		 ("  %al, (%dx)",     0, lambda x: ["PORT", "BYTE", "EDX"]),
		 ("  %eax, (%dx)",    0, lambda x: ["PORT", "EDX"]),
		 ("  (%dx), %al",     0, lambda x: ["PORT", "BYTE", "EDX"]),
		 ("  (%dx), %eax",    0, lambda x: ["PORT", "EDX"]),
		 ("  %eax, $0x42",    1, lambda x: len(x[1]) == 2 and ["IMM1", "PORT"]),
		 ("  $0x42, %eax",    1, lambda x: len(x[1]) == 2 and ["IMM1", "PORT"]),
		 ("  $0x2, %al",      1, lambda x: (not (len(x[1]) > 2 and x[1][:-2] in map(lambda y: y[3][:-1], res))
						    and not (len(x[1]) == 2 and "IMPL" in res[-1][1]) and ["IMM1",  "EAX", "BYTE"])),
		 ("  %al, $0x2",      1, lambda x: not (len(x[1]) > 2 and x[1][:-2] in map(lambda y: y[3][:-1], res)) and ["IMM1",  "EAX", "BYTE", "DIRECTION"]),
		 ("  $0x12345, %edx", 4, lambda x: (len(x[1]) > 5 and x[1][:-5] not in map(lambda y: y[3], res) and ["IMMO", "MODRM", "GRP"]
						    or len(x[1]) == 5 and ["IMMO", "IMPL"])),
		 ("  $0x2, %edx",     1, lambda x: (not (len(x[1]) > 4 and x[1][:-4] in map(lambda x: x[3], res))
						    and x[1][:-2] not in map(lambda y: y[3], res) and ["IMM1", "MODRM", "GRP"])),
		 ("  $0x12345, %eax", 4, lambda x: (x[1][:-4] not in map(lambda y: y[3][:-1], res)
						    and x[1][:-5] not in map(lambda y: y[3], res) and not (len(x[1]) == 5 and "IMPL" in res[-1][1]) and ["IMMO", "EAX"])),
		 ("l %cl, (%eax)",    0, lambda x: ["OP1", "MODRM", "GRP", "ECX"]),
		 ("b %cl, (%eax)",    0, lambda x: x[1][:-1] not in map(lambda y: y[3], res) and ["OP1", "MODRM", "GRP", "ECX", "BYTE"]),
		 ("b (%eax)",         0, lambda x: ["OP1", "MODRM", "GRP", "BYTE"]),
		 ("w (%eax)",         0, lambda x: x[1][0]!= '66' and ["OP1", "MODRM", "GRP", "WORD"]),
		 ("l (%eax)",         0, lambda x: ["OP1", "MODRM", "GRP"]),
		 ("  (%eax)",         0, lambda x: x[1] not in map(lambda x: x[3], res) and ["OP1", "MODRM", "GRP"]),
		 ("  %edx",           0, lambda x: ["IMPLTEST", "TESTONLY"]),
		 ("  %ecx",           0, lambda x: (x[1][:-1] in map(lambda x: x[3][:-1], filter(lambda y: "GRP" not in y[1], res)) and
						    x[1][:-1] not in map(lambda x: x[3][:-1], filter(lambda y: "GRP" in y[1], res)) and ["IMPL"])),
		 ("  %edx, %eax",     0, lambda x: (not (len(x[1]) > 1 and x[1][:-1] in map(lambda x: x[3], res))
						    and not (len(x[1]) > 1 and x[1][:-1] in map(lambda x: x[3][:-1], res)) and ["EAX", "IMPL", "DIRECTION"])),
		 ("  $0x1234,$0x12",  3, lambda x: ["IMM3"]),
		 ("l $0x12344,(%edx)",4, lambda x: x[1][:-5] not in map(lambda x: x[3][:-1], filter(lambda y: "GRP" in y[1], res)) and ["OP1", "MODRM", "GRP", "IMMO"]),
		 ("b $0x45, (%edx)",  1, lambda x: x[1][:-2] not in map(lambda x: x[3][:-1], filter(lambda y: "GRP" in y[1], res)) and ["OP1", "MODRM", "GRP", "IMM1", "BYTE"]),
		 (" %eax, 0x1234",    4, lambda x: no_modrm(res, x[1][:-5]) and no_grp(res, x[1][:-5]) and ["MOFS"]),
		 (" 0x1234, %eax",    4, lambda x: no_modrm(res, x[1][:-5]) and no_grp(res, x[1][:-5]) and ["MOFS", "DIRECTION"]),
		 (" 0x1234, %al",     4, lambda x: no_modrm(res, x[1][:-5]) and no_grp(res, x[1][:-5]) and ["MOFS", "DIRECTION", "BYTE"]),
		 (" %al,  0x1234",    4, lambda x: no_modrm(res, x[1][:-5]) and no_grp(res, x[1][:-5]) and ["MOFS", "BYTE"]),
		 ]
    # XXX add segment jmps
    if "JMP" in opcode[1]:
	templates = [
	    (" *0x12345678", 4, lambda x: ["MODRM", "GRP"]),
	    (" 0x12345678", 4, lambda x: x[1][:-4] not in map(lambda x: x[3], res) and ["IMMO"]),
	    (" 1f; 1:", 1, lambda x: x[1][:-4] not in map(lambda x: x[3], res) and ["IMM1"]),
	    (" $0x1234,$0x12345678", 6, lambda x: ["LONGJMP"]),
	    ]
    res = []
    for postfix, length, func in templates:
	l = compile_and_disassemble(opcode[0]+postfix, file, fdict)
	flags = l and func(l)
	if type(flags) == type([]):
	    code = l[1][:len(l[1])-length - ("DROP1" in opcode[1] and 1 or 0)]
	    # a LOCK prefix is valid?
	    flags += "RMW" in opcode[1] and "DIRECTION" not in flags and "MODRM" in flags and ["LOCK"] or []
	    res += [(opcode[0],
	    	    filter(lambda x: x, opcode[1] + flags),
	    	    opcode[2], code)]
	    print >>sys.stderr, res[-1], l
    if not res:
	print >>sys.stderr, "error", opcode, res
    return filter(lambda x: "TESTONLY" not in x[1], res)


def filter_chars(snippet, op_size, flags):
    for m,n in [("[os]", "%d"%op_size), ("[data16]", op_size == 1 and "data16" or ""),
		("[bwl]", "bwl"[op_size]), ("[qrr]", "qrr"[op_size]), ("[lock]", "LOCK" in flags and "lock" or ""),
		("[EAX]", ("%%al","%%ax", "%%eax")[op_size]),
		("[EDX]", ("%%dl","%%dx", "%%edx")[op_size]),
		("[IMM]", filter(lambda x: x in ["IMM1", "IMM2", "IMMO", "CONST1"], flags) and "1" or "0"),
		("[OP1]", "OP1" in flags and "1" or "0"),
		("[IMMU]", "IMM1" in flags and "unsigned char" or "unsigned")]:
	snippet = map(lambda x: x.replace(m, n), snippet)
    return snippet


def generate_functions(name, flags, snippet, enc, functions, l2):
    if not snippet:
	l2.append("UNIMPLEMENTED(this)")
	return
    if "ASM" in flags and not "asm volatile" in ";".join(snippet):
        # Be sure to duplicate %s
        snippet = ['asm volatile("'+ ";".join([re.sub(r'([^%])%([^%0-9])', r'\1%%\2', s) for s in snippet])+'" : "+d"(tmp_src), "+c"(tmp_dst) : : "eax")']
    if "FPU" in flags:
        if "FPUNORESTORE" not in flags:  snippet = ['fxrstor (%%eax)'] + snippet
        snippet = ['if (cache->_cpu->cr0 & 0xc) EXCEPTION(cache, 0x7, 0)',
                   'asm volatile("' + ';'.join(snippet)+'; fxsave (%%eax);" : "+d"(tmp_src), "+c"(tmp_dst) : "a"(cache->_fpustate))']
    if "CPL0" in flags:
        snippet = ["if (cache->cpl0_test()) return"] + snippet
    # parameter handling
    imm = ";entry->%s = &entry->immediate"%("DIRECTION" in flags and "dst" or "src")
    additions = [("MEMONLY", "if (~entry->modrminfo & MRM_REG) {"),
		 ("REGONLY", "if (entry->modrminfo & MRM_REG) {"),
		 ("OS2",     "entry->operand_size = 2"),
		 ("OS1",     "entry->operand_size = 1"),
		 ("CONST1",  "entry->immediate = 1" + imm),
		 ("IMM1",    "fetch_code(entry, 1); entry->immediate = *reinterpret_cast<char  *>(entry->data+entry->inst_len - 1)" + imm),
		 ("IMM2",    "fetch_code(entry, 2); entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2)" + imm),
		 ("IMM3",    "fetch_code(entry, 3); entry->immediate = *reinterpret_cast<unsigned *>(entry->data+entry->inst_len - 3)" + imm),
		 ("IMMO",    imm),
		 ("MOFS",    "fetch_code(entry, 1 << entry->address_size); entry->src = &_cpu->eax"),
		 ("LONGJMP", "fetch_code(entry, 2 + (1 << entry->operand_size))"),
		 ("IMPL",    "entry->dst = get_reg<%d>(entry->data[entry->offset_opcode-1] & 0x7)"%("BYTE" in flags)),
		 ("ECX",     "entry->src = &_cpu->ecx"),
		 ("EDX",     "entry->%s = &_cpu->edx"%("DIRECTION" in flags and "dst" or "src")),
		 ("EAX",     "entry->%s = &_cpu->eax"%("DIRECTION" in flags and "src" or "dst")),
		 ]
    l2.extend(filter(lambda x: x, map(lambda x: x[0] in flags and x[1] or "", additions)))

    # flag handling
    f2 = ["LOADFLAGS", "SAVEFLAGS", "MODRM", "BYTE", "DIRECTION", "READONLY", "ASM", "RMW", "LOCK", "MOFS", "BITS"]
    if "SKIPMODRM" in flags:                    f2.remove("MODRM")
    if "IMM1" in flags and "BITS" in flags:     f2.remove("BITS")
    if name in ["cltd"]:                        f2.remove("DIRECTION")
    f = map(lambda x: "IC_%s"%x, filter(lambda x: x in f2, flags))
    if f: l2.append("entry->flags = %s"%"|".join(f))

    # operand size loop
    s = ""
    for op_size in range(3):
	no_os = ("BYTE" in flags or "NO_OS" in flags) and "HAS_OS" not in flags
	if no_os or op_size > 0:
	    s += op_size == 1 and "if (entry->operand_size == 1) {" or op_size == 2 and " else {" or "{"
	    if "IMMO"   in flags:
		s += "fetch_code(entry, %d);"%(1 << op_size)
		s += (op_size == 1 and "entry->immediate = *reinterpret_cast<short *>(entry->data+entry->inst_len - 2);"  or
		      op_size == 2 and "entry->immediate = *reinterpret_cast<int   *>(entry->data+entry->inst_len - 4);")
	    funcname = "exec_%s_%s_%d"%("".join(enc), reduce(lambda x,y: x.replace(y, "_"), "% ,.()", name), op_size)
	    s+= "entry->execute = %s; }"%funcname
	    functions[funcname] = filter_chars(snippet, op_size, flags)
	    if no_os: break
    l2.append(s)
    if "MEMONLY" in flags or "REGONLY" in flags:
	l2.append("} else  { ")
	l2.append('Logging::printf("%s not implemented at %%x - %%x instr %%02x%%02x%%02x\\n", _cpu->eip, entry->modrminfo, entry->data[0], entry->data[1], entry->data[2]); '%(name.replace("%", "%%")))
	l2.append("UNIMPLEMENTED(this); }")


def generate_code(encodings):
    code = {}
    functions={}
    code_prefixes=[]
    for i in range(len(encodings)):
	name, flags, snippet, enc = encodings[i]
	print >>sys.stderr, "%10s %s %s"%(name, enc, flags)
	for l in range(len(enc)):
	    if l == len(enc)-1 or l == len(enc)-2 and "GRP" in flags:
		n = str(enc[:l])
		if n not in code_prefixes:
		    code_prefixes.append(n)
		    code.setdefault(code_prefixes.index(n), {})
		p = code[code_prefixes.index(n)]
		l2 = []
		if "PREFIX" in flags:  l2.extend(snippet)
		if "MODRM"  in flags:
		    l2.append("get_modrm()")
		    if "GRP" not in flags:
			l2.append("entry->%s = get_reg<%d>((entry->data[entry->offset_opcode] >> 3) & 0x7)"%("src", "BYTE" in flags))
		if l == len(enc)-2:
		    if enc[l] not in p:
			l2.append("switch (entry->data[entry->offset_opcode] & 0x38) {")
			l2.append('default:')
			l2.append('Logging::printf("unimpl GRP case %02x%02x%02x at %d\\n", entry->data[0], entry->data[1], entry->data[2], __LINE__)')
			l2.append("UNIMPLEMENTED(this)")
			l2.append('}')
			p[enc[l]] = l2
		    l2 = []
		    l2.append("case 0x%s & 0x38: {"%enc[l+1])
		    l2.append("/* instruction '%s' %s %s */"%(name, enc, flags))
		    generate_functions(name, flags, snippet, enc, functions, l2)
		    l2.append("break; }")
		    p[enc[l]] = p[enc[l]][:2] + l2 + p[enc[l]][2:]
		elif enc[l] not in p:
		    l2 = ["/* instruction2 '%s' %s %s */"%(name, enc, flags)] + l2
		    if "PREFIX" not in flags:
			generate_functions(name, flags, snippet, enc, functions, l2)
		    key = enc[l]
		    if "IMPL" in flags:  key = "%x ... %#x"%(int(key, 16) & ~0x7, int(key, 16) | 7)
		    p[key] = l2
		break
	    else:
		n = str(enc[:l])
		if n not in code_prefixes:
		    code_prefixes.append(n)
		    code[code_prefixes.index(n)] = {}
		code[code_prefixes.index(n)].setdefault(enc[l], ["op_mode = %d"%len(code_prefixes)])
    return code, functions


def print_code(code, functions):
    names = functions.keys()
    names.sort()
    for funcname in names:
	print """static void __attribute__((regparm(3))) %s(InstructionCache *cache, void *tmp_src, void *tmp_dst) {
		 %s; }"""%(funcname, ";".join(functions[funcname]))

    print "int handle_code_byte(InstructionCacheEntry *entry, unsigned char code, int &op_mode) {"
    print "entry->offset_opcode = entry->inst_len;"
    print "switch (op_mode) {"
    p = code.keys()
    p.sort()
    for i in p:
	print """case 0x%s:
	{
	  switch(code) {"""%i
	q = code[i].keys()
	q.sort()
	for j in q:
	    print "case 0x%s:"%j
	    print "{"
	    for l in code[i][j]:
		print l,";"
	    print "break; }"
	print """default:
	      fetch_code(entry, 4);
	      Logging::printf("unimplemented case %x at line %d code %02x%02x%02x\\n", code, __LINE__, entry->data[0], entry->data[1], entry->data[2]);
	      UNIMPLEMENTED(this);
	  }
	}
	break;"""
    print "default:  assert(0); }"
    print "return _fault; }"


FILE="foo.keep"
try:
    fdict = dict(map(lambda x: x[:-1].split("#"), open(FILE, "r").readlines()))
except:
    fdict = {}
file = open(FILE, "a")

# List of supported opcodes (name, flags, snippet),
opcodes = []
segment_list = ["es", "cs", "ss", "ds", "fs", "gs"]
opcodes += [(x, ["PREFIX"], ["entry->prefixes = (entry->prefixes & ~0xff00) | (%d << 8)"%segment_list.index(x)]) for x in segment_list]
opcodes += [(x, ["PREFIX"], ["entry->prefixes = (entry->prefixes & ~(%#x)) | (code << %d)"%(0xff<<(8*(y-1)), (8*(y-1)))])
	    for x,y in [("lock", 1) ,("repz", 1) , ("repnz", 1), ("data16", 3), ("addr16", 4)]]
opcodes[-2] = (opcodes[-2][0], opcodes[-2][1], opcodes[-2][2]+["entry->operand_size = (((entry->cs_ar >> 10) & 1) + 1) ^ 3"])
opcodes[-1] = (opcodes[-1][0], opcodes[-1][1], opcodes[-1][2]+["entry->address_size = (((entry->cs_ar >> 10) & 1) + 1) ^ 3"])
opcodes += [
    ("clc",   ["NO_OS"], ["cache->_cpu->efl &= ~1"]),
    ("cmc",   ["NO_OS"], ["cache->_cpu->efl ^=  1"]),
    ("stc",   ["NO_OS"], ["cache->_cpu->efl |=  1"]),
    ("cld",   ["NO_OS"], ["cache->_cpu->efl &= ~0x400"]),
    ("std",   ["NO_OS"], ["cache->_cpu->efl |=  0x400"]),
    ("bswap", ["NO_OS", "ASM"], ["mov (%ecx), %eax", "bswap %eax", "mov %eax, (%ecx)"]),
    ("xchg",  ["ASM", "RMW"],  ["mov (%edx), [EAX]", "[lock] xchg[bwl] [EAX], (%ecx)", "mov [EAX], (%edx)"]),
    ("cwtl",  ["ASM", "EAX"],  ["mov (%ecx), [EAX]", "[data16] cwde", "mov [EAX], (%ecx)"]),
    ("cltd",  ["ASM", "EAX", "EDX", "DIRECTION"],   ["mov (%edx), %eax", "[data16] cltd", "mov %edx, (%ecx)"]),
    ("str",   ["NO_OS", "OS1"], ["move<1>(tmp_dst, &cache->_cpu->tr.sel)"]),
    ("sldt",  ["NO_OS", "OS1"], ["move<1>(tmp_dst, &cache->_cpu->ld.sel)"]),
    ("smsw",  ["NO_OS", "OS1"], ["move<1>(tmp_dst, &cache->_cpu->cr0)"]),
    ("nopl (%eax)",  ["NO_OS", "SKIPMODRM", "MODRM", "DROP1"], [" "]),
    ("lahf",  ["NO_OS"], ["cache->_cpu->ah = (cache->_cpu->efl & 0xd5) | 2"]),
    ("sahf",  ["NO_OS"], ["cache->_cpu->efl = (cache->_cpu->efl & ~0xd5) | (cache->_cpu->ah  & 0xd5)"]),
    ]
opcodes += [(x, ["ASM", "EAX", "NO_OS", x in ["aaa", "aas"] and "LOADFLAGS", "SAVEFLAGS"],
	     ["mov (%ecx), %eax", x, "mov %eax, (%ecx)"])
	    for x in ["aaa", "aas", "daa", "das"]]
opcodes += [(x, ["ASM", x in ["cmp", "test"] and "READONLY",
	   		x in ["adc", "sbb"] and "LOADFLAGS",
			x not in ["mov"] and "SAVEFLAGS",
			x not in ["mov", "cmp",  "test"] and "RMW",
			],
	     ["mov[bwl] (%edx), [EAX]", "[lock] %s[bwl] [EAX],(%%ecx)"%x])
	    for x in ["mov", "add", "adc", "sub", "sbb", "and", "or", "xor", "cmp", "test"]]
opcodes += [(x, ["ASM", x not in ["not"] and "SAVEFLAGS", x in ["dec", "inc"] and "LOADFLAGS", "RMW"],
	     ["[lock] " + x + "[bwl] (%ecx)"])
	    for x in ["inc", "dec", "neg", "not"]]
opcodes += [(x, ["ASM", "CONST1", x in ["rcr", "rcl"] and "LOADFLAGS", "SAVEFLAGS", "RMW"],
	     ["xchg %edx, %ecx","movb (%ecx),%cl", "%s[bwl] %%cl, (%%edx)"%x]) for x in ["rol", "ror", "rcl", "rcr", "shl", "shr", "sar"]]
opcodes += [(x, ["ASM", "SAVEFLAGS", "DIRECTION"], ["%s[bwl] (%%edx), [EAX]"%x, "mov [EAX], (%ecx)"]) for x in ["bsf", "bsr"]]
ccflags = map(lambda x: compile_and_disassemble(".byte %#x, 0x00"%x, file, fdict)[2].split()[0][1:], range(0x70, 0x80))
for i in range(len(ccflags)):
    ccflag = ccflags[i]
    opcodes += [("set" +ccflag, ["BYTE",   "ASM", "LOADFLAGS"], ["set%s (%%ecx)"%ccflag])]
    opcodes += [("cmov"+ccflag, ["NO_OS",  "ASM", "LOADFLAGS",  "OS2"], ["j%s 1f"%(ccflags[i ^ 1]), "mov (%edx), %eax", "mov %eax, (%ecx)", "1:"])]
    opcodes += [("j"+ccflag,    ["JMP",   "ASM", "LOADFLAGS", "DIRECTION"],
		 ['REGPARM(3) int (*foo)(InstructionCache *, void *) = helper_JMP_static<[os]>',
		  'asm volatile ("j%s 1f;call %%c0; 1:" : : "i"(foo))'%(ccflags[i ^ 1])])]
opcodes += [(x, [x[-1] == "b" and "BYTE", "HAS_OS"], [
            "unsigned dummy",
	    "tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7)",
	    """asm volatile("movl (%%2), %%0; [data16] %s (%%1), %%0; mov %%0, (%%2)" : "=a"(dummy), "+d"(tmp_src), "+c"(tmp_dst))"""%x])
	    for x in ["movzxb", "movzxw", "movsxb", "movsxw"]]

def add_helper(l, flags, params):
    for x in l:
	name = reduce(lambda x,y: x.replace(y, "_"), "% ,", x.upper())
	if "NO_OS" not in flags: name += "<[os]>"
	opcodes.append((x, flags, ["cache->helper_%s(%s)"%(name, params or "")]))
add_helper(["push", "lret", "ret"],                              ["DIRECTION"], "tmp_src")
add_helper(["int", "aad", "aam"],                                ["NO_OS"], "*reinterpret_cast<unsigned char *>(tmp_src)")
add_helper(["ljmp", "lcall", "call", "jmp",  "jecxz", "loop", "loope", "loopne"],
	   ["JMP", "DIRECTION"], "tmp_src")
add_helper(["in", "out"],                                        [],       "*reinterpret_cast<[IMMU] *>(tmp_src), &cache->_cpu->eax")
add_helper(["popf", "pushf", "leave", "iret"],                   [], "")
add_helper(["pop"],                                              [], "tmp_dst")
add_helper(["lea", "lgdt", "lidt"],                              ["MEMONLY", "DIRECTION", "SKIPMODRM"], "")
add_helper(["sgdt", "sidt"],                                     ["MEMONLY", "SKIPMODRM"], "")

add_helper(["mov %cr0,%edx", "mov %edx,%cr0"],                   ["MODRM", "DROP1", "REGONLY", "NO_OS", "CPL0"], "")
add_helper(["ltr", "lldt"],                                      ["NO_OS", "OS1", "DIRECTION"], "*reinterpret_cast<unsigned short *>(tmp_src)")
add_helper(["lmsw"],                                             ["NO_OS", "OS1", "DIRECTION", "CPL0"], "*reinterpret_cast<unsigned short *>(tmp_src)")
add_helper(["enter"],                                            [], "reinterpret_cast<unsigned *>(tmp_src)")
add_helper(["hlt", "clts", "wbinvd",  "invd"], ["NO_OS", "CPL0"], "")
add_helper(["sti", "cli", "int3", "into", "fwait", "ud2a", "sysenter", "sysexit", "xlat"], ["NO_OS"], "")

add_helper(["invlpg"], ["NO_OS", "MEMONLY", "SKIPMODRM", "CPL0"], "")
add_helper(["mov %db0,%edx", "mov %edx,%db0"], ["MODRM", "DROP1", "REGONLY", "NO_OS", "CPL0"], "")
add_helper(["fxsave", "frstor"], ["SKIPMODRM", "NO_OS"], "");

stringops = {"cmps": "SH_LOAD_ESI | SH_LOAD_EDI | SH_DOOP_CMP",
	     "ins" : "SH_SAVE_EDI | SH_DOOP_IN",
	     "movs": "SH_LOAD_ESI | SH_SAVE_EDI",
	     "outs": "SH_LOAD_ESI | SH_DOOP_OUT",
	     "scas": "SH_LOAD_EDI | SH_DOOP_CMP",
	     "stos": "SH_SAVE_EDI",
	     "lods": "SH_LOAD_ESI | SH_SAVE_EAX"}
opcodes += [(x, [], ["cache->string_helper<%s, [os]>()"%stringops[x]]) for x in stringops.keys()]
opcodes += [(x, [], ["cache->send_message(CpuMessage::TYPE_%s)"%(x.upper())]) for x in ["cpuid", "rdtsc", "rdmsr", "wrmsr"]]
opcodes += [(x, ["DIRECTION"], [
	    # 'Logging::printf("%s(%%x, %%x)\\n", cache->_cpu->eax, *reinterpret_cast<unsigned *>(tmp_dst))'%x,
	    "unsigned edx = cache->_cpu->edx, eax = cache->_cpu->eax;",
	    "asm volatile (\"1: ;"
	    "%s[bwl] (%%2);"
	    "xorl %%2, %%2;"
	    "2: ; .section .data.fixup2; .long 1b, 2b, 2b-1b; .previous;"
	    "\" : \"+a\"(eax), \"+d\"(edx), \"+c\"(tmp_src))"%x,
	    "if (tmp_src) DE0(cache)",
	    "cache->_cpu->eax = eax",
	    "cache->_cpu->edx = edx",
	    ]) for x in ["div", "idiv", "mul"]]
opcodes += [(x, ["RMW"], ["unsigned count",
			    "if ([IMM]) count = cache->_entry->immediate; else count = cache->_cpu->ecx",
			    "tmp_src = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7)",
			    'asm volatile ("xchg %%eax, %%ecx; mov (%%edx), %%edx; [data16] '+
			    x+' %%cl, %%edx, (%%eax); pushf; pop %%eax" : "+a"(count), "+d"(tmp_src), "+c"(tmp_dst))',
			    "cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (count  & 0x8d5)"])
	    for x in ["shrd", "shld"]]
opcodes += [("imul", ["DIRECTION"], ["unsigned param, result",
				 "tmp_dst = cache->get_reg32((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7)",
				 "if ([IMM]) param = cache->_entry->immediate; else if ([OP1]) param = cache->_cpu->eax; else move<[os]>(&param, tmp_dst);",
				 # 'Logging::printf("IMUL %x * %x\\n", param, *reinterpret_cast<unsigned *>(tmp_src))',
				 'asm volatile ("imul[bwl] (%%ecx); pushf; pop %%ecx" : "+a"(param), "=d"(result), "+c"(tmp_src))',
				 "cache->_cpu->efl = (cache->_cpu->efl & ~0x8d5) | (reinterpret_cast<unsigned>(tmp_src)  & 0x8d5)",
				 "if ([OP1]) move<[os] ? [os] : 1>(&cache->_cpu->eax, &param)",
				 "if ([OP1] && [os]) move<[os]>(&cache->_cpu->edx, &result)",
				 "if (![OP1]) move<[os]>(tmp_dst, &param)",
				 # 'Logging::printf("IMUL %x:%x\\n", result, param)',
				 ])]
opcodes += [("mov %es,%edx", ["MODRM", "DROP1"], [
	    "CpuState::Descriptor *dsc = &cache->_cpu->es + ((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7)",
	    "move<[os]>(tmp_dst, &(dsc->sel))"]),
	    ("mov %edx,%es", ["MODRM", "DROP1", "DIRECTION"], [
	    "if (((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7) == 2) cache->_cpu->intr_state |= 2",
	    "cache->set_segment(&cache->_cpu->es + ((cache->_entry->data[cache->_entry->offset_opcode] >> 3) & 0x7), *reinterpret_cast<unsigned short *>(tmp_src))"]),
	    ("pusha", [], ["for (unsigned i=0; i<8; i++) {",
                           "if (i == 4) { if (cache->helper_PUSH<[os]>(&cache->_oesp)) return; }"
                           "else if (cache->helper_PUSH<[os]>(cache->get_reg32(i))) return; }"]),
	    ("popa", [], ["unsigned values[8]",
                          "for (unsigned i=8; i; i--)  if (cache->helper_POP<[os]>(values+i-1)) return;",
                          "for (unsigned i=0; i < 8; i++) if (i!=4) move<[os]>(cache->get_reg32(i), values+i)"]),
	    ]

for x in segment_list:
    opcodes += [("push %"+x, [], ["cache->helper_PUSH<[os]>(&cache->_cpu->%s.sel)"%x]),
		("pop %"+x, [], ["unsigned sel", "cache->helper_POP<[os]>(&sel) || cache->set_segment(&cache->_cpu->%s, sel)"%x, x == "ss" and "cache->_cpu->intr_state |= 2" or ""]),
		("l"+x, ["SKIPMODRM", "MODRM", "MEMONLY"], ["cache->helper_loadsegment<[os]>(&cache->_cpu->%s)"%x])]
opcodes += [(x, ["FPU", "FPUNORESTORE", "NO_OS"], [x]) for x in ["fninit"]]
opcodes += [(x, ["FPU", "NO_OS"], [x+" (%%ecx)"]) for x in ["fnstsw", "fnstcw", "ficom", "ficomp"]]
opcodes += [(x, ["FPU", "NO_OS", "EAX"], ["fnstsw (%%ecx)"]) for x in ["fnstsw %ax"]]
opcodes += [(".byte 0xdb, 0xe4 ", ["NO_OS", "COMPLETE"], ["/* fnsetpm, on 287 only, noop afterwards */"])]
opcodes += [(x, [x not in ["bt"] and "RMW" or "READONLY", "SAVEFLAGS", "BITS", "ASM"], ["mov (%edx), %eax",
										       "and  $(8<<[os])-1, %eax",
											"[lock] "+x+" [EAX],(%ecx)"]) for x in ["bt", "btc", "bts", "btr"]]
opcodes += [("cmpxchg", ["RMW"], ['char res; asm volatile("mov (%2), %2; [lock] cmpxchg [EDX], (%3); setz %1" : "+a"(cache->_cpu->eax), "=d"(res) : "d"(tmp_src), "c"(tmp_dst))',
				  "if (res) cache->_cpu->efl |= EFL_ZF; else cache->_cpu->efl &= EFL_ZF"])]
opcodes += [("xadd", ["RMW", "ASM", "SAVEFLAGS"], ['mov (%edx), [EAX]', '[lock] xadd [EAX], (%ecx)', 'mov [EAX], (%edx)'])]

# unimplemented instructions
opcodes += [(x, [], []) for x in ["vmcall", "vmlaunch", "vmresume", "vmxoff", "vmptrld", "vmptrst", "vmread", "vmwrite"]] # , "vmxon", "vmclear"
opcodes += [(x, [], []) for x in ["sysenter", "sysexit", "monitor", "mwait"]]
opcodes += [(x, [], []) for x in ["rdpmc"]]
opcodes += [(x, ["RMW"], []) for x in ["cmpxchg8b"]]
opcodes += [(x, [], []) for x in [
	"arpl", "bound", "enter",
	"lar",  "lsl",
	"rsm", "verr", "verw",
	"getsec"]]

# "salc" - 0xd6
encodings = sum(map(lambda x: get_encoding(x, file, fdict), opcodes), [])
encodings.sort(lambda x,y: cmp(x[3], y[3]))
code, functions = generate_code(encodings)
print("// -*- Mode: C++ -*-")
print("// Automagically generated. Do not touch.")
print_code(code, functions)
