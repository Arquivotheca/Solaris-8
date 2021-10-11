#
#ident "@(#)Makefile	1.15	96/03/14 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
# stand/Makefile

include ../Makefile.master

SUBDIRS = lib

all install clean lint: $(SUBDIRS)

clobber: $(SUBDIRS)
	$(RM) make.out lint.out

all	:=	TARGET = all
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint
install	:=	TARGET = install

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
