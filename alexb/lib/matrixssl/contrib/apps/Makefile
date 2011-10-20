#
#	Makefile for example client and server apps
#   'make' builds optimized (Default).
#   'make debug' builds debug.
#
#	Copyright (c) PeerSec Networks, 2002-2011. All Rights Reserved
#

CC 	= gcc
O 	= .o
A   = .a
E	=

STATICS		= ../libmatrixssl$(A)

SERVER_EXE 	= server$(E) 
SERVER_OBJS = server$(O) http$(O)

CLIENT_EXE 	= client$(E) 
CLIENT_OBJS = client$(O) http$(O)

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
CFLAGS  = $(DFLAGS) -DMAKEFILE_BUILD -DPOSIX -I.. -I../sampleCerts
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
CC  = cc
CFLAGS += -DOSX -isystem -I/usr/include
LDFLAGS = -dead_strip
endif

all: compile

compile: $(SERVER_EXE) $(CLIENT_EXE)

$(SERVER_EXE):$(SERVER_OBJS) $(STATICS)
	$(CC) -o $@ $^ $(LDFLAGS) $(STATICS)

$(CLIENT_EXE):$(CLIENT_OBJS) $(STATICS)
	$(CC) -o $@ $^ $(LDFLAGS) $(STATICS)

#
#	Clean up all generated files
#
clean:
	rm -f $(SERVER_EXE) $(SERVER_OBJS) $(CLIENT_EXE) $(CLIENT_OBJS)

