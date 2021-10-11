#
#ident	"@(#)Makefile	1.6	94/09/19 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/face/src/Makefile

# include global definitions
include ../../../Makefile.master

#
#	Subdirectory oam not made to suppress sysadm installation.
#
#	Subdirectory proc is made indirectly by xx

SUBDIRS = filecab vinstall xx

all :=          TARGET = all
install :=      TARGET = install
clean :=        TARGET = clean
clobber :=      TARGET = clobber
lint :=         TARGET = lint

.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all: $(SUBDIRS)

install: $(SUBDIRS)

clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
