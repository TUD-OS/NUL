#!/usr/bin/env python

import sys

print("void print_%s(unsigned short *dst) {" % sys.argv[1])
input = sys.stdin
lpos = 0
while True:
    print ("// Line %d" % lpos)
    lpos += 1
    line = input.readline()
    if not line:
	break
    if (len(line)>0 and line[0] == '#'):
	continue
    pos = 0
    attr = 0x07
    for c in line:
	if c == '[':
	    attr = 0x0F
	elif c == ']':
	    attr = 0x07
	elif c in ['\n']:
	    pass
	else:
	    print(" *(dst+%d) = 0x%x;" % (pos, (attr << 8) | ord(c)))
	    pos += 1
    print(" dst += %d;" % 80)

print("}")
