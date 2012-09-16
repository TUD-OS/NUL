#!/usr/bin/env python2

import sys
from StringIO import StringIO

table = StringIO()
table.write(" unsigned written = 0;\n")
table.write(" switch (line) {\n")

input = sys.stdin
lpos = 0
screen_lines = 0

while True:
    lpos += 1
    line = input.readline()
    if not line:
	break
    if (len(line)>0 and line[0] == '#'):
	continue
    pos = 0
    attr = 0x07

    screen_lines += 1
    table.write("case %d:\n" % (screen_lines - 1))
    if (screen_lines > 25):
        table.write(" if (++written > 25) break;\n")
    for c in line:
	if c == '[':
	    attr = 0x0F
	elif c == ']':
	    attr = 0x07
	elif c in ['\n']:
	    pass
	else:
	    table.write("  *(dst+%d) = 0x%x;\n" % (pos, (attr << 8) | ord(c)))
	    pos += 1
    table.write("  memset(dst + %d, 0, 160 - 2*%d);\n" % (pos, pos))
    table.write("  dst += %d;\n" % 80)
table.write(" }\n")
table.write("memset(dst, 0, (25 - written) * 2*80);\n");

print("#include <service/string.h>")
print("void print_%s(unsigned short *dst, unsigned &line) {" % sys.argv[1])

lline = (screen_lines - 24) if (screen_lines > 24) else 0
print(" if (line > %d) line = %d; " % ( lline, lline ))
print(table.getvalue())
table.close()
print("}")
