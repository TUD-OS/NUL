# -*- Mode: Python -*-

## Assumptions
#
# Devices have sparse register windows. Registers are 32-bit or 64-bit
# wide and are naturally aligned.
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

rset82576vf = [
    # Normal R/W register
    { 'name' : 'VTCTRL',
      'offset'  : 0x0,
      'initial' : 0},
    # Normal R/O register
    { 'name' : 'STATUS',
      'offset' : 0x8,
      'initial' : 0,
      'read-only' : True },
    # Free Running Timer
    { 'name' : 'VTFRTIMER',
      'offset' : 0x1048,
      'read-only' : True,
      'read-compute' : 'VTFRTIMER_compute' },
    # RC/W1C
    { 'name' : 'VTEICR',
      'offset' : 0x1580,
      'w1c' : True,
      'rc': True },
    { 'name' : 'VTEICS',
      'offset' : 0x1520,
      'set' : 'VTEICR',
      'callback' : 'VTEICR_cb',
      'w1s' : True,             # Write 1s to set. 0s are ignored
      'write-only' : True },
    ]

def offset_cmp(r1, r2):
    return r1['offset'] - r2['offset']

def print_out(s):
    print(s)

# TODO Generate a binary decision tree instead of large switches. We
# could introduce a "probability" key in the register description to
# properly weigh the tree's branches. This probability key can be
# computed from actual register access traces. Sounds like
# profile-guided optimization for hardware models. Good idea?
def dispatch_gen(var, regs, filt, mangle, out, default = ""):
    out("\tswitch (%s) {" % var)
    for r in sorted(regs, offset_cmp):
        if filt(r):
            out("\tcase 0x%x: %s break;" % (r['offset'], mangle(r) ))
    out ("\tdefault: %s break;" % default)
    out ("\t}")

def read_dispatch_gen(name, regs, out):
    def mangle(r):
        return "val = %s_read();" % r['name']
    def filt(r):
        return 'write-only' not in r
    out ("\nuint32_t %s(uint32_t offset)\n{\n\tuint32_t val;" % name)
    dispatch_gen("offset", regs, filt, mangle, out, "val = 0; /* UNDEFINED! */")
    out ("\treturn val;\n}")

def write_dispatch_gen(name, regs, out):
    def mangle(r):
        return "%s_write(value);" % r['name']
    def filt(r):
        return 'read-only' not in r
    out ("\nvoid %s(uint32_t offset, uint32_t value)\n{" % name)
    dispatch_gen("offset", regs, filt, mangle, out, "/* UNDEFINED! */")
    out ("}")

def declaration_gen(rset, out):
    """Generate declarations for all registers that need them."""
    for r in rset:
        if 'write-only' in r or ('read-compute' in r and 'read-only' in r):
            pass
        else:
            out("\tuint32_t %s;" % r['name'])

def writer_gen(r, out):
    if 'read-only' in r:
        return
    out("\nvoid %s_write(uint32_t val)\n{" % r['name'])
    if 'set' in r:
        target = r['set']['name']
    else:
        target = r['name']
    if 'w1c' in r:
        out("\t%s &= ~val;\t// W1C" % target)
    elif 'w1s' in r:
        out("\t%s |= val;\t// W1S" % target)
    else:
        out("\t%s = val;" % target)

    if 'callback' in r:
        out("\t%s();" % r['callback'])
    out("}")

def reader_gen(r, out):
    if 'write-only' in r:
        return
    out("\nuint32_t %s_read()\n{" % r['name'])
    if 'read-compute' in r:
        out("\tuint32_t val = %s();" % r['read-compute'])
    else:
        out("\tuint32_t val = %s;" % r['name'])
    if 'rc' in r:
        out("\t%s = 0;\t// RC" % r['name'])
    out("\treturn val;")
    out("}")


def sanity_check(rset):
    """Check the register set description for basic consistency."""
    for r in rset:
        assert 'name' in r
        assert not ('read-compute' in r and 'rc' in r)

def class_gen(rset, out):
    sanity_check(rset)
    # Resolve set references
    for r in rset:
        if 'set' in r:
            for ri in rset:
                if ri['name'] == r['set']:
                    r['set'] = ri
                    break
    out("\n\t// Declarations")
    declaration_gen(rset, out)
    out("\n\t// Dispatch")
    write_dispatch_gen("dispatch_write", rset, out);
    read_dispatch_gen("dispatch_read", rset, out);
    out("\n\t// Readers")
    for r in rset:
        reader_gen(r, out)
    out("\n\t// Writers")
    for r in rset:
        writer_gen(r, out)

# EOF
