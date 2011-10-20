#
#	makefile for MatrixSSL single process protocol testing
#   'make' builds optimized (Default).
#   'make debug' builds debug.
#
#	Copyright (c) PeerSec Networks, 2002-2011. All Rights Reserved
#

CC 	= gcc
O 	= .o
A   = .a
E	=

STATICS		= ../../libmatrixssl$(A)

TEST_EXE 	= sslTest$(E) 
TEST_OBJS 	= sslTest$(O) 

#
# This is set by the debug target below
#
ifdef PS_DEBUG
DFLAGS  = -g -Wall -DDEBUG
else
DFLAGS	= -Os
endif

gold:
	@$(MAKE) compile

debug:
	@$(MAKE) compile "PS_DEBUG = 1"

default: gold

#
#	Compile options
#
CFLAGS  = $(DFLAGS) -DPOSIX -I.. -I../.. -I../../sampleCerts
LDFLAGS = -lc

#
#   Override variables for compilation on x86-64 (AMD64, Nacona, etc)
#
ifeq ($(shell uname -m),x86_64)
DFLAGS	+= -march=k8 -fPIC -DRDTSC
endif

#
#   Override variables for compilation on Mac OS X (Darwin)
#
ifeq ($(shell uname),Darwin)
CFLAGS += -isystem -I/usr/include
LDFLAGS =
endif

all: compile

compile: $(TEST_EXE)

$(TEST_EXE):$(TEST_OBJS) $(STATICS)
	$(CC) -o $@ $^ $(LDFLAGS) $(STATICS)

#
#	Clean up all generated files
#
clean:
	rm -f $(TEST_EXE) $(TEST_OBJS)
