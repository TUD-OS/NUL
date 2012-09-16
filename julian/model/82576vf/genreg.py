#!/usr/bin/env python2
# -*- Mode: Python -*-

## Assumptions
#
# Devices have sparse register windows. Registers are 32-bit or 64-bit
# wide and are naturally aligned. 64-bit registers can be safely read
# and written as two 32-bit chunks.
#
# Most registers have a static behavior (read-clear, etc), but some
# have mode switches.
#
# Most registers only store data until it is needed, only some trigger
# further activity.
#
## Goals
#
# GCC should be able work its optimization magic (*cough*)
# unobstructed by too much generality in the C++ code.
#
# The resulting code should be human-readable.

import sys
import imp
from time import gmtime, strftime
from getpass import getuser

# Check if we can use binary literals (0b1010)
if sys.version_info < (2, 6):
    print("Your python is OLD! Please upgrade to 2.6.")

def important_cmp(r1, r2):
    return r2['important'] - r1['important']

def print_out(s):
    print(s)

def make_file_out(f):
    def file_out(s):
        f.write(s + '\n')
    return file_out

def unsigned(n):
    return n & 0xFFFFFFFF

# TODO Generate a binary decision tree instead of large switches. We
# could introduce a "probability" key in the register description to
# properly weigh the tree's branches. This probability key can be
# computed from actual register access traces. Sounds like
# profile-guided optimization for hardware models. Good idea?
def dispatch_gen(var, regs, filt, mangle, out, default = ""):
    out("\tswitch (%s/4) {" % (var))
    for r in sorted(regs, important_cmp):
        if filt(r):
            out("\tcase 0x%x: %s break;" % (r['offset']/4, mangle(r) ))
    out ("\tdefault: /* Logging::printf(\"--> %%s UNKNOWN %%x\\n\", __PRETTY_FUNCTION__, %s);*/ %s break;" % (var, default))
    out ("\t}")

def read_dispatch_gen(name, regs, out):
    def mangle(r):
        return "val = %s_read();" % r['name']
    def filt(r):
        return 'write-only' not in r
    out ("\nuint32 %s(uint32 offset)\n{\n\tuint32 val;" % name)
    dispatch_gen("offset", regs, filt, mangle, out, "val = 0; /* UNDEFINED! */")
    out ("\treturn val;\n}")

def write_dispatch_gen(name, regs, out):
    def mangle(r):
        return "%s_write(value);" % r['name']
    def filt(r):
        return 'read-only' not in r
    out ("\nvoid %s(uint32 offset, uint32 value)\n{" % name)
    dispatch_gen("offset", regs, filt, mangle, out, "/* UNDEFINED! */")
    out ("}")

def declaration_gen(name, rset, out):
    """Generate declarations for all registers that need them."""
    inits = []
    for r in rset:
        if 'write-only' in r or ('read-compute' in r and 'read-only' in r) or 'constant' in r:
            pass
        else:
            inits.append("\t%s = 0x%xU;" % (r['name'], unsigned(r['initial'])))
            out("uint32 %s;" % r['name'])
    out("\nvoid %s_init()\n{" % name)
    for line in inits:
        out(line)
    out("}")


def writer_gen(r, out):
    if 'read-only' in r:
        return
    out("\nvoid %s_write(uint32 val)\n{" % r['name'])
    out("\tuint32 nv;")
    if 'set' in r:
        target = r['set']['name']
    else:
        target = r['name']

    if 'callback' in r:
        out("\tuint32 old = %s;" % target);

    if 'w1c' in r:
        out("\tnv = %s & ~val;\t// W1C"  % target)
    elif 'w1s' in r:
        out("\tnv = %s | val;\t// W1S"  % target)
    else:
        out("\tnv = val;")

    if 'mutable' in r:
        out("\tnv = (%s & ~0x%xU) | (nv & 0x%xU);" % (target, unsigned(r['mutable']), unsigned(r['mutable'])))
    out("\t%s = nv;" % target)
    #out('\tLogging::printf("WRITE %10s %%x %%x\\n", val, nv);' % r['name'])
    if 'callback' in r:
        out("\t%s(old, val);" % r['callback'])
    out("}")

def reader_gen(r, out):
    if 'write-only' in r:
        return
    out("\nuint32 %s_read()\n{" % r['name'])
    if 'read-compute' in r:
        out("\tuint32 val = %s();" % r['read-compute'])
    elif 'constant' in r:
        out("\tuint32 val = 0x%xU;" % unsigned(r['initial']))
    else:
        out("\tuint32 val = %s;" % r['name'])
    if 'rc' in r:
        out("\t%s &= ~0x%x;\t// RC" % (r['name'], unsigned(r['rc'])))
    #out('\tLogging::printf("READ  %10s %%x\\n", val);' % r['name'])
    out("\treturn val;")
    out("}")


def sanity_check(rset):
    """Check the register set description for basic consistency."""
    for r in rset:
        assert 'name' in r
        assert not ('read-compute' in r and 'rc' in r)

def class_gen(name, rset, out):
    sanity_check(rset)
    # Resolve set references and provide some convenience
    out("/// -*- Mode: C++ -*-")
    out("/// This file has been automatically generated.")
    out("/// Generated on %s by %s" % (strftime("%a, %d %b %Y %H:%M:%S", gmtime()), getuser()))
    for r in rset:
        assert r['offset'] % 4 == 0
        if 'important' not in r:
            r['important'] = -100
        if 'constant' in r:
            assert 'initial' in r
            r['read-only'] = True
        if 'set' in r:
            for ri in rset:
                if ri['name'] == r['set']:
                    r['set'] = ri
                    break
    out("\n/// Declarations")
    declaration_gen(name, rset, out)
    out("\n/// Dispatch")
    write_dispatch_gen(name + "_write", rset, out);
    read_dispatch_gen(name + "_read", rset, out);
    out("\n/// Readers")
    for r in rset:
        reader_gen(r, out)
    out("\n/// Writers")
    for r in rset:
        writer_gen(r, out)
    out("\n/// Done")

# Main

if __name__ == "__main__":
    m = imp.load_source("foomod", sys.argv[1], open(sys.argv[1], 'r'))
    outfile = open(sys.argv[2], 'w')
    class_gen(m.name, m.rset, make_file_out(outfile))
        
# EOF
