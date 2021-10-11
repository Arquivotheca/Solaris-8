#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.5	99/01/25 SMI"
#
# lib/libw/sparcv9/Makefile

MAPDIR=	../spec/sparcv9

include ../Makefile.com
include ../../Makefile.lib.64

BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) $(PICS) 2>&1 | \
		fgrep -v "No read-write segments found" | cat

OBJECTS = empty.o
SRCS= $(OBJECTS:%.o=../common/%.c)

LIBS =	$(DYNLIB)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64)

lint:

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $< 2>&1 | fgrep -v "empty translation unit" \
				   | cat
	$(POST_PROCESS_O)
