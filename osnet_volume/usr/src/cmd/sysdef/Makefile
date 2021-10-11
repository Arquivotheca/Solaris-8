#
#ident	"@(#)Makefile	1.11	97/10/22 SMI"
#
# Copyright (c) 1990-1997 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= 		sysdef 

include ../Makefile.cmd

SUBDIRS =	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

RELUSRSBIN=     ../usr/sbin
ROOTSYMLINK=    $(ROOTETC)/$(PROG)

OWNER= root
GROUP= sys
FILEMODE= 0555

.KEEP_STATE:

all:	$(SUBDIRS)

$(ROOTSYMLINK):
	-$(RM) $@; $(SYMLINK) $(RELUSRSBIN)/$(PROG) $@

clean clobber lint:	$(SUBDIRS)

install:	$(SUBDIRS) $(ROOTSYMLINK)
	-$(RM) $(ROOTUSRSBINPROG)
	-$(LN) $(ISAEXEC) $(ROOTUSRSBINPROG)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ../Makefile.targ
