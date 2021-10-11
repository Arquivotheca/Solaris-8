#
#ident	"@(#)Makefile	1.5	97/10/29 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# lib/libplot/Makefile
#
# include global definitions
include ../../Makefile.master

SUBDIRS= plot t300 t300s t4014 t450 vt0

all:=	  TARGET= all
clean:=   TARGET= clean
clobber:= TARGET= clobber
install:= TARGET= install
lint:=	  TARGET= lint

HDRS=	plot.h
ROOTHDRDIR=	$(ROOT)/usr/include
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)

# install rule for install_h target
$(ROOTHDRDIR)/%: %
	$(INS.file)

.KEEP_STATE:

all clean clobber install lint: $(SUBDIRS)

install_h: $(ROOTHDRS)

check:  $(CHECKHDRS)
$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)


FRC:
