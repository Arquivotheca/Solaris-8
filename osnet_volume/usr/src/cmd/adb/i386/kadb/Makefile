#
#ident	"@(#)Makefile	1.2	93/08/30 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/adb/i386/kadb/Makefile
#
# builds occur in kernel-architecture subdirectories using Makefile.com
# in this directory.  The targets are deposited in this directory.
#

PROG=		kadb.o

SUBDIRS=	i86pc

# ALLKADBS should fit the pattern below
ALLKADBS=	$(SUBDIRS:%=%-$(PROG))

# any target fitting the pattern will expect and use a corresponding
# architecture's subdirectory
#
%-$(PROG): %  FRC
	@cd $<; pwd; $(MAKE) "ARCHOPTS=$(ARCHOPTS)" ../$@
	@pwd

# conditional assignments
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

.KEEP_STATE:

all: $(ALLKADBS)

clean clobber lint: $(SUBDIRS)

install:

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)
	@pwd

FRC:
