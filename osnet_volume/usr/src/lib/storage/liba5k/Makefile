#
#pragma ident	"@(#)Makefile	1.7	99/07/29 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/liba5k/Makefile
#

include $(SRC)/Makefile.master

SUBDIRS = $(MACH)

# conditional assignments
all :=		TARGET= all
install :=	TARGET= install
install_h :=	TARGET= install_h
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
test :=		TARGET= test
debug	:=	TARGET= debug
browser	:=	TARGET= browser
debug_browser	:=	TARGET= debug_browser
gencat	:=	TARGET= gencat

.KEEP_STATE:
 
all install_h clean clobber debug browser debug_browser: $(SUBDIRS)

install: gencat install_h $(SUBDIRS)

gencat lint: $(MACH)

docs:

$(MACH): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

