#
# ident	"@(#)Makefile	1.5	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#

# include global definitions
include ../../Makefile.master

SUBDIRS = form menu oeu oh proc qued sys vt wish xx

all :=		TARGET = all
install :=	TARGET = install
clean :=	TARGET = clean
clobber :=	TARGET = clobber
lint :=		TARGET = lint

.KEEP_STATE:

all: $(SUBDIRS)

install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
