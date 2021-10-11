#
#ident	"@(#)Makefile	1.11	98/07/30 SMI"
#
# Copyright (c) 1994-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/w/Makefile

PROG= w 

include ../Makefile.cmd

ROOTLINK= $(ROOTBIN)/uptime

SED= sed
DCFILE= $(PROG).dc

SUBDIRS=	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

.KEEP_STATE:

all: $(SUBDIRS) 

clean clobber lint:	$(SUBDIRS)

install:	$(SUBDIRS)
	-$(RM) $(ROOTPROG) $(ROOTLINK)
	-$(LN) $(ISAEXEC) $(ROOTPROG)
	-$(LN) $(ISAEXEC) $(ROOTLINK)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(DCFILE):
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) -t $(PROG).c
	$(SED) -e '/^domain/d' messages.po > $@
	$(RM) messages.po

FRC:

include ../Makefile.targ
