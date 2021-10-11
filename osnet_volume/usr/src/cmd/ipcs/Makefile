#
#ident	"@(#)Makefile	1.4	99/10/07 SMI"
#
# Copyright (c) 1990-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ipcs/Makefile
#

PROG= ipcs
XPG4PROG= ipcs

include ../Makefile.cmd

SUBDIRS=	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

.KEEP_STATE:

all:	$(SUBDIRS)

clean clobber lint:	$(SUBDIRS)

install:	$(SUBDIRS)
	-$(RM) $(ROOTPROG) $(ROOTXPG4PROG)
	-$(LN) $(ISAEXEC) $(ROOTPROG)
	-$(LN) $(ISAEXEC) $(ROOTXPG4PROG)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ../Makefile.targ
