#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile	1.1 99/07/11 SMI"
#
#	Makefile ia64
#

include $(SRC)/Makefile.master

SUBDIRS= domestic global

all :=          TARGET= all
clean :=        TARGET= clean
clobber :=      TARGET= clobber
install :=      TARGET= install
lint :=		TARGET= lint
check :=	TARGET = check
debug := 	TARGET = debug
tcov := 	TARGET = tcov
analyse := 	TARGET = analyse

# include library definitions
include $(SRC)/lib/Makefile.lib
include $(SRC)/lib/Makefile.lib.64

include ../Makefile.com

CFLAGS64 += -DMACHINE64

target_all install debug tcov : pics $(PICS) $(SUBDIRS)
lint clobber clean : $(SUBDIRS)

.PARALLEL:	$(OBJS) $(PROFS) $(PICS) DUMMY

include $(SRC)/lib/Makefile.targ

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

.KEEP_STATE:
