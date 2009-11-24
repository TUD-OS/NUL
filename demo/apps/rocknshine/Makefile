IMAGE=bin/rocknshine.nova

# rocknshine
OBJ          += rocknshine.o asm.o
# tinf
OBJ          += tinfgzip.o tinfzlib.o adler32.o crc32.o tinflate.o
# environment
OBJ	     += vprintf.o logging.o simplemalloc.o
INCLUDES=-I../ -I../../include/ -I../../include/libc -I../../contrib/tinf/src
vpath %.S  :../sys
vpath %.cc :../sys
vpath %.c  :../../contrib/tinf/src
include ../Makeconf
