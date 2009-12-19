#!/usr/bin/env python
"""Generate helper functions from pseudo-code available in the
documentation.

Input: vol3.txt generated from 'pdftotext vol3.pdf'
Output: headerfile with helpers
"""

import sys, re


def split_into(l, splitreasons):
    start = 0
    res = {}
    name = ""
    for reason in splitreasons:
        if reason not in l[start:]:
            res[name] = ""
            continue
        ofs = l[start:].index(reason)
        res[name] = l[start:start+ofs]
        name = reason
        start += ofs + 1
    res[name] = l[start:]
    return res

def file_to_opcodes(file, initialname):
    NEWPAGE = 'XXXXXX'
    opcodes = {}
    lines = []
    name = initialname
    for l in filter(lambda x: x, map(lambda x: x[:-1].strip(" "), file.readlines())):
        if '\x0c' in l:
            if len(lines) > 3 and (lines[-1] == "Instruction Reference" or lines[-3] == "Instruction Reference"):
                name = lines[-2]
                lines.pop()
                lines.pop()
                lines.pop()
            opcodes.setdefault(name, [])
            opcodes[name] += lines[1:]
            lines = []
        else:
            lines.append(l)
            
    opcodes[name] = lines
    return opcodes

def intel2c(pseudo, rules):
    "convert intel pseudocode to c"
    res = []
    for line in pseudo:
        l = line[:]
        for a,b in rules:
            line = re.sub(a, b, line)
#        res.append("/* "+repr(l)+" */")
        res.append(line)
    return res

def fixlabels(code):
    "fix labels that contain invalid chars"
    labels = re.findall("(?m)GOTO (.*?);", code)
    labels.sort()
    labels.reverse()
    for r in labels:
        code = code.replace(r, r.replace("-", "_"))
    return code

def handle_if(x):
    assert len(x.groups()) == 1
    if x.group(1) == " 1; ": return "IF  1;"
    if x.group(1) == " 0; ": return "IF  0;"
    return "if ("+ x.group(1)

typos = [('FI;\\)', "FI;"),
         ]
general_rules = typos + [
    ("\\(\\*.*\\*\\)",""), 
    (" ([0-9A-F]+)H([ ;\\)])", lambda x: " 0x"+x.group(1) + x.group(2)),
    ("^IF (.*?);?$", handle_if), ("THEN", ") {",), ("ELSE", "} else {",), 
    ("FI", "}",), 
    ("=", "=="), ("(?i) and ", " & "), ("(?i) or ", " | "),
    ("END", "return"),
    ("#SS(\\(0\\))?", "SS0"),
    ("#GP\\(0\\)", "GP0"),
    ("^IF  0;", "msg.cpu->efl &= ~EFL_IF;"),
    ("^IF  1;", "msg.cpu->efl |= EFL_IF;"),
    ("^VIF  0;","msg.cpu->efl &= ~EFL_VIF;"), 
    ("^VIF  1;","msg.cpu->efl |= ~EFL_VIF;"),
    ("IOPL  CPL", "IOPL >= CPL"),
    ("IOPL", "msg.cpu->iopl()"), 
    ("CPL", "msg.cpu->cpl()"),
    ("VIP", "msg.cpu->efl & EFL_VIP"), 
    ("VME ", "!!(msg.cpu->cr4 & CR4_VME)"),
    ("PVI ", "!!(msg.cpu->cr4 & CR4_PVI)"),
    ("PE ", "!!(msg.cpu->cr0 & CR0_PE)"),
    ("VM ", "!!(msg.cpu->cr0 & CR0_VM)"),
    ("IA32_EFER.LMA", "!!(msg.vcpu->efer & EFER_LMA)"),
    ]

rules = {
    }

opcodes = {}
for filename in sys.argv[1:]:
    opcodes.update(file_to_opcodes(open(filename), filename))
print opcodes.keys(), len(opcodes)
import pprint
pprint.pprint(opcodes["IRETx"])
#for key in rules:
#    print "void helper_%s(MessageExecutor &msg, InstructionCacheEntry *entry) {"%key.split("/")[0]
#    print fixlabels("\n".join(intel2c(opcodes[key]["Operation"], rules[key])))
#    print "}"
