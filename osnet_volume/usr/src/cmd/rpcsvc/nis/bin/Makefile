#
# @(#)Makefile	1.6	92/04/21 SMI
#
#	Copyright (c) 1988-1992 Sun Microsystems Inc
#	All Rights Reserved.
#
# Makefile for NIS+ server utility commands
#

SCRIPTS=	nissetup  nisclient  nisserver  nispopulate  nisopaccess
PROG=	nisstat nisshowcache nisupdkeys nisping nisctl $(SCRIPTS)
# unsupported programs, which are built, but not installed
UPROG=	nisadm
SRCS=	nisadm.c nisctl.c nisstat.c nisshowcache.c nisupdkeys.c nisping.c
OBJS=	nisadm.o nisctl.o nisstat.o nisshowcache.o nisupdkeys.o nisping.o

include ../../../Makefile.cmd

ROOTNSLIB=	$(ROOT)/usr/lib/nis
ROOTNSPROG=	$(PROG:%=$(ROOTNSLIB)/%)
ROOTSCRIPTS=	$(SCRIPTS:%=$(ROOTNSLIB)/%)

CPPFLAGS += -I$(SRC)/lib/libnsl/include
LDLIBS += -lnsl

$(ROOTSCRIPTS) := FILEMODE = 755

.KEEP_STATE:

all: $(PROG) $(UPROG)

install: all $(ROOTNSLIB) $(ROOTNSPROG)

lint: lint_SRCS

$(ROOTNSLIB):
	$(INS.dir)

$(ROOTNSLIB)/%: %
	$(INS.file)

clean:
	-$(RM) $(OBJS)

include ../../../Makefile.targ
