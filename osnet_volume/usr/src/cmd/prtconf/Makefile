#
#ident	"@(#)Makefile	1.12	97/10/20 SMI" /* SVr4.0 1.6.6.2 */
#
# Copyright (c) 1990-1997 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= 		prtconf 

include ../Makefile.cmd

SUBDIRS=	$(MACH)
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
