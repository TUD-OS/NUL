#!/usr/bin/env python2

# Analyze the L1 cache footprint of a given binary.

# TODO Nehalem cache layout is hardcoded.
# TODO Not finished. Add execution probability to the analysis.

import subprocess
import re

cache_line_size = 64

def cache_set(addr):
    return (addr >> 6) & 0x7F

def cl_down(a):
    return a & ~(cache_line_size - 1)

def cl_up(a):
    return (a + cache_line_size - 1) & ~(cache_line_size - 1)


class Symbol(object):
    def __init__(self, name, addr, size):
        self.name = name
        self.addr = addr
        self.size = size

        self.sets = dict()
        for clad in range(cl_down(addr), cl_up(addr + size), cache_line_size):
            if ((addr <= clad) and (clad < addr+size)):
                aset = cache_set(clad)
                if aset not in self.sets:
                    self.sets[aset] = 0
                self.sets[aset] += 1
            
class Binary:
    def __init__(self, name):
        self.name = name
        

nm_command = ["nm", "--numeric-sort", "-SC"]
#004343e2 0000002a T Logging::printf(char const*, ...)
#00400398 000002d3 W GenericProtocol::call_server(Utcb&, bool)
regexp = re.compile(r"([0-9a-f]+)\s([0-9a-f]+)\s[tTwW]\s(.+)$")

def symbols_from_file(filename):
    nm = subprocess.Popen(nm_command + [filename], stdout=subprocess.PIPE)
    symbols = []
    while True:
        line = nm.stdout.readline()
        if (line == ''):
            break
        m = regexp.match(line)
        if m:
            ns = Symbol(m.group(3), int(m.group(1), 16), int(m.group(2), 16))
            for s in symbols:
                if s.addr == ns.addr:
                    print("Duplicate symbol %s %x %x. Ignore." % (s.name, s.size, ns.size))
                    break
            else:
                symbols.append(ns)
#        else:
#            print("Ignore line %s" % line),
    return symbols

def cache_stats(symbols):
    s = dict()
    for sym in symbols:
        for aset, byte in sym.sets.items():
            if aset not in s:
                s[aset] = 0
            s[aset] += byte
    return s

def pretty_stats(s):
    for aset, byte in sorted(s.items(), key=lambda p: p[0]):
        print("%3d: %s" % (aset, "*" * byte))

def cl_is_used_no(addr, syms):
    assert cl_down(addr) == addr
    times = 0
    by    = []
    for s in syms:
        if (((addr >= s.addr) and (addr < s.addr + s.size)) or
            ((addr + cache_line_size - 1 >= s.addr) and (addr + cache_line_size - 1 < s.addr + s.size))):
            times += 1
            by.append(s)
    return (times, by)
    

def analyse(syms):
    sortsym = sorted(syms, key=lambda v: v.addr)
    minaddr = sortsym[0].addr
    maxaddr = sortsym[-1].addr + sortsym[-1].size
    print("Searching from %x to %x" % ( minaddr, maxaddr ))
    for a in range(cl_down(minaddr), cl_up(maxaddr), cache_line_size):
        times, by = cl_is_used_no(a, syms)
        if times != 1:
            print("%x -> %u %s" % (a, times, [s.name for s in by]))
        

def main(filename):
    pass

#if __name__ == "__main__":
#    main()

# EOF
