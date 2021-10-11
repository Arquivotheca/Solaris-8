#
# ident	"@(#)Makefile 1.23 98/03/23 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/tnf/prex/Makefile
#

PROG=  prex

include	../../Makefile.cmd

POFILE=prex.po
POFILES=cmd.po fcn.po list.po main.po new.po prbk.po source.po spec.po util.po

SUBDIRS = 	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

.KEEP_STATE:

all:    $(SUBDIRS)

clean clobber lint: 	$(SUBDIRS)

install:	$(SUBDIRS)
	-$(RM) $(ROOTPROG)
	-$(LN) $(ISAEXEC) $(ROOTPROG)


$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(POFILE):	$(POFILES)
	rm -f $@
	cat $(POFILES) > $@
	
FRC:

include	../../Makefile.targ
