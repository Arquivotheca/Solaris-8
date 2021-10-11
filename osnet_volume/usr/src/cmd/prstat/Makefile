#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/prstat/Makefile
#

#ident	"@(#)Makefile	1.2	99/04/22 SMI"

PROG = prstat
OBJS = prstat.o prtable.o prsort.o prutil.o prfile.o
POFILES = $(OBJS:.o=.po)
CLOBBERFILES = $(POFILE) $(POFILES)

include ../Makefile.cmd

XGETFLAGS = -a -x prstat.xcl
POFILE = pprstat.po

.KEEP_STATE:

SUBDIRS =	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

all:		$(SUBDIRS)
clean:		$(SUBDIRS)
clobber:	$(SUBDIRS)
lint:		$(SUBDIRS)

$(POFILE): $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

install: $(SUBDIRS)
	-$(RM) $(ROOTPROG)
	-$(LN) $(ISAEXEC) $(ROOTPROG)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ../Makefile.targ
