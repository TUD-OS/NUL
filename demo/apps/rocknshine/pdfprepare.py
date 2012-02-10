#!/usr/bin/env python
# -*- Mode: Python -*-

import sys
import glob
import os
import subprocess
import re
import zlib
import struct
import tempfile
from stat import *

width  = 1024
height = 768

if (len(sys.argv) != 3):
    print("Usage: pdfprepare.py input.pdf output.rs")
    exit(1)

pdf = sys.argv[1]
out = sys.argv[2]
tmpdir = tempfile.mkdtemp()

print("Input : " + pdf)
print("Output: " + out)
print
print("Directory for (huge) temporary files: " + tmpdir)
print

print("Converting to raw image data. Go get a coffee. This can take a while.")

# This also works instead of the call to pdftoppm, but is slow and
# generates bogus results for some PDFs.
# subprocess.check_call(['convert', '-density', '400',
#                        pdf, '-resize', '%dx%d!' % (width, height), '-depth', '8',
#                        tmpdir + '/page%d.png'])

print("FIXME Resolution is hardcoded.")
subprocess.check_call(['pdftoppm', '-r', '203.2', '-W', '1024', '-H', '768', pdf, tmpdir + '/page'])

def numcomp(x, y):
    xnum = int(re.search('(?<=page-)[0-9]+', x).group(0))
    ynum = int(re.search('(?<=page-)[0-9]+', y).group(0))
    return xnum - ynum;

# TODO Use ImageMagick python interface
compressed_pages = []
for page in sorted(glob.glob(tmpdir + '/page-*.ppm'), numcomp):
    subprocess.check_call(['convert', page, '-separate', '-swap', '0,2', '-combine', '-depth', '8', page + '.rgb'])
    os.remove(page)
    uncompressed_len = os.stat(page + '.rgb')[ST_SIZE]
    assert (width*height*3) == uncompressed_len
    file = open(page + '.rgb', 'rb')
    os.remove(page + '.rgb')
    data = file.read()
    file.close()
    compressed = zlib.compress(data, 9)
    print("Compressed page to %d%% (%s)." %
          (100*len(compressed)/uncompressed_len, hex(len(compressed))))
    compressed_pages.append(compressed)

try:
    os.rmdir(tmpdir)
except OSError:
    print("Temporary directory is not empty. Not removed.")


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

# Write image data
for page in compressed_pages:
    print("CRC32 of compressed page: " + str(zlib.crc32(page)))
    outfile.write(page)

# EOF
