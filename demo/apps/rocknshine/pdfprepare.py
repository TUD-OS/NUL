#!/usr/bin/env python2
# -*- Mode: Python -*-

import sys
import subprocess
import zlib
import struct
import array
from stat import *

width  = 1024
height = 768

if (len(sys.argv) != 3):
    print("Usage: pdfprepare.py input.pdf output.rs")
    exit(1)

pdf = sys.argv[1]
out = sys.argv[2]

print("Input : " + pdf)
print("Output: " + out)

print("Converting to raw image data. Go get a coffee. This can take a while.")

# This also works instead of the call to pdftoppm, but is slow and
# generates bogus results for some PDFs.
# subprocess.check_call(['convert', '-density', '400',
#                        pdf, '-resize', '%dx%d!' % (width, height), '-depth', '8',
#                        tmpdir + '/page%d.png'])

print("FIXME Resolution is hardcoded.")
ppmdata = subprocess.Popen(['pdftoppm', '-r', '203.2', '-W', '1024', '-H', '768', pdf],
                           stdin = open("/dev/null"), stdout = subprocess.PIPE)
ppmstream = ppmdata.stdout

compressed_pages = []
while ppmstream.readline() == "P6\n":
    w,h = map(int, ppmstream.readline()[:-1].split())
    assert ppmstream.readline()=="255\n"
    rgb = array.array("c", ppmstream.read(w*h*3))
    rgb[0::3], rgb[2::3] = rgb[2::3],rgb[0::3]
    compressed = zlib.compress(rgb, 9)
    uncompressed_len = len(rgb)
    print("Compressed page to %d%%." % (100*len(compressed)/uncompressed_len))
    compressed_pages.append(compressed)

ppmstream.close()

# See README.org for file format details.
outfile = open(out, "wb")
outfile.write(struct.pack("ccccHHI", "P", "R", "E", "0",
                          width, height, len(compressed_pages)))

# Add empty page at end to encode file size.
offset = 12 + (len(compressed_pages)+1)*4
for page in compressed_pages + [""]:
    outfile.write(struct.pack("I", offset))
    offset += len(page)

# Write image data
for page in compressed_pages:
    outfile.write(page)

# EOF
