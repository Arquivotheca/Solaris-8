#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/05/04 SMI"
#
# lib/libw/ia64/Makefile

MAPDIR=	../spec/ia64

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
