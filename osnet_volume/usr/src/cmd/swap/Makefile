#
#ident	"@(#)Makefile	1.12	98/06/17 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/swap/Makefile

PROG= swap

include ../Makefile.cmd

SUBDIRS=	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

CPPFLAGS += -D_LARGEFILE64_SOURCE

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

ROOTSYMLINK = 	$(ROOTETCPROG)

.KEEP_STATE:

all:	$(SUBDIRS)

$(ROOTSYMLINK):
	$(RM) $@; $(SYMLINK) ../usr/sbin/$(PROG) $@

clean clobber lint:	$(SUBDIRS)

install: $(SUBDIRS) $(ROOTSYMLINK)
	-$(RM) $(ROOTUSRSBINPROG)
	-$(LN) $(ISAEXEC) $(ROOTUSRSBINPROG)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ../Makefile.targ

