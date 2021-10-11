#
#ident	"@(#)Makefile	1.6	98/07/26 SMI"
#
# Copyright (c) 1989-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# ucbcmd/ps/Makefile
#

PROG= ps

include ../Makefile.ucbcmd

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
	-$(RM) $(ROOTPROG)
	-$(LN) $(ISAEXEC) $(ROOTPROG)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ../Makefile.ucbtarg
