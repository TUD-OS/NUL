#!/usr/bin/env python
# -*- Mode: Python -*-

import sys
import glob
import os
import subprocess
import re
import zlib
import struct
from stat import *

width  = 1024
height = 768

pdf = sys.argv[1]
out = sys.argv[2]
tmpdir = "/tmp"

print("Input : " + pdf)
print("Output: " + out)
print
print("Directory for (huge) temporary files: " + tmpdir)
print

print("Converting to raw image data. Go get a coffee. This can take a while.")
subprocess.check_call(['convert', '-density', '400',
                       pdf, '-resize', '%dx%d!' % (width, height), '-depth', '8',
                       tmpdir + '/page%d.bgr'])

def numcomp(x, y):
    xnum = int(re.search('(?<=page)[0-9]+', x).group(0))
    ynum = int(re.search('(?<=page)[0-9]+', y).group(0))
    return xnum - ynum;

compressed_pages = []
for page in sorted(glob.glob(tmpdir + '/page*.bgr'), numcomp):
    uncompressed_len = os.stat(page)[ST_SIZE]
    file = open(page, 'rb')
    os.remove(page)
    data = file.read()
    file.close()
    compressed = zlib.compress(data, 9)
    print("Compressed page to %d%% (%s)." %
          (100*len(compressed)/uncompressed_len, hex(len(compressed))))
    compressed_pages.append(compressed)

# See README.org for file format details.
outfile = open(out, "wb")
outfile.write(struct.pack("ccccHHI", "P", "R", "E", "0",
                          width, height, len(compressed_pages)))

# Add empty page at end to encode file size.
offset = 12 + (len(compressed_pages)+1)*4
for page in compressed_pages + [""]:
    outfile.write(struct.pack("I", offset))
    print("offset " + hex(offset))
    offset += len(page)

for page in compressed_pages:
    print("CRC32 of compressed page: " + str(zlib.crc32(page)))
    outfile.write(page)

# EOF
