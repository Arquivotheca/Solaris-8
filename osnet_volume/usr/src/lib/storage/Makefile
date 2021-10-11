#
#pragma ident	"@(#)Makefile	1.20	99/07/26 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/storage/Makefile
#

# Note that the 'common' subdirectory must be built first
SUBDIRS= common libg_fc liba5k
DOTCDIRS= libg_fc liba5k

# MACROs for the packaging up of source code.
SRCDIRS = 	${SUBDIRS:%=lib/storage/%}

# time to clean up
CLOBBERFILES = 	$(SRCARCS)

install	:= TARGET = install
install_h := TARGET = install_h
all	:= TARGET = all
clean	:= TARGET = clean
clobber	:= TARGET = clobber
check	:= TARGET = check
debug	:= TARGET = debug
tcov	:= TARGET = tcov
lint	:= TARGET = lint
docs	:= TARGET = docs
gencat	:= TARGET = gencat

all clean clobber debug : $(SUBDIRS)

lint gencat: $(DOTCDIRS)

install install_h : $(ROOTDIRS) $(SUBDIRS)

source:	${SRCARCS}

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

.KEEP_STATE:

include $(SRC)/lib/Makefile.targ
