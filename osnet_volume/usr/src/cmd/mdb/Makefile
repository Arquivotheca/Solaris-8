#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	99/11/19 SMI"

.PRECIOUS: cscope.out
.KEEP_STATE:

sparc_MACHDIR = sparc
i386_MACHDIR = intel

sparc_SUBDIRS = sparc sun4d sun4m sun4u
i386_SUBDIRS = intel i86pc

MACHDIR = $($(MACH)_MACHDIR)
SUBDIRS = $($(MACH)_SUBDIRS) demo
TARGET =

all := TARGET = all
clean.lint := TARGET = clean.lint
clean := TARGET = clean
clobber := TARGET = clobber
dmods := TARGET = dmods
install := TARGET = install
install_h := TARGET = install_h
lint := TARGET = lint

all lint: $(SUBDIRS)

dmods install: install_h $(SUBDIRS)

install_h: FRC
	@cd common/mdb; pwd; $(MAKE) install_h

clean.lint clean: $(SUBDIRS) packages

clobber: $(SUBDIRS) packages
	rm -f cscope.files

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

pkg: install scripts
	@cd packages; pwd; $(MAKE) install

CSCOPE = cscope-fast
CSFLAGS = -b
CSPATHS = common $(sparc_SUBDIRS) $(i386_SUBDIRS)
CSINCS = $(CSPATHS:%=-I%)

cscope: cscope.out

cscope.out: cscope.files FRC
	$(CSCOPE) $(CSFLAGS)

cscope.files: FRC
	echo "$(CSINCS)" > cscope.files
	find $(CSPATHS) \( -type d -name SCCS -prune \) -o \
	\( -type f \! -name mdb_lex.c -a \! -name mdb_grammar.c -a \
	-name '*.[chly]' -print \) >> cscope.files

FRC:
