IMAGE=bin/gt.nova
# gt
OBJ          += gt.o asm.o
# environment
OBJ	     += vprintf.o logging.o simplemalloc.o
INCLUDES=-I../ -I../../include/ -I../../include/libc
vpath %.S  :../sys
vpath %.cc :../sys
include ../Makeconf