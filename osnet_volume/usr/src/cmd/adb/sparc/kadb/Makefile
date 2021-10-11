#
#ident	"@(#)Makefile	1.11	99/10/04 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
#
# cmd/adb/sparc/kadb/Makefile
#
# builds occur in kernel-architecture subdirectories using Makefile.com
# in this directory.  The targets are deposited in this directory.
# A typical target would be sun4m-kadb.o
#

PROG=		kadb.o

SUBDIRS=	sun4m sun4d sun4u

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
