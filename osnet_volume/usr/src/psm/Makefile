#
#ident "@(#)Makefile	1.1	94/09/26 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
# psm/Makefile

include ../Makefile.master

SUBDIRS = stand

all install clean clobber lint: $(SUBDIRS)

all	:=	TARGET = all
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint
install	:=	TARGET = install

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
