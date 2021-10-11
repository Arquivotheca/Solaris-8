#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.11	99/12/06 SMI"
#
# Makefile for the nisaddcred program
#
PROG=	nisaddcred

SRCS= \
	nisaddcred.c   makelocalcred.c  makedescred.c \
	makersacred.c  makekerbcred.c	makedhextcred.c
OBJS= \
	nisaddcred.o   makelocalcred.o  makedescred.o \
	makersacred.o  makekerbcred.o	makedhextcred.o
UTIL= ../util.a

include $(SRC)/cmd/Makefile.cmd

CPPFLAGS += -I$(SRC)/lib/libnsl/include
CFLAGS += -I$(ROOT)/usr/include 
LDLIBS += -lnsl -lmp $(UTIL)

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTPROG)

lint:	lint_SRCS

clean:
	$(RM) $(OBJS)

$(UTIL):
	@cd ..; pwd; $(MAKE) util.a

$(PROG): $(OBJS) $(UTIL)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)


include $(SRC)/cmd/Makefile.targ
