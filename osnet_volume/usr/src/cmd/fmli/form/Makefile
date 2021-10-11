#
#ident	"@(#)Makefile	1.6	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/fmli/form/Makefile
#
LIBRARY = libform.a

OBJECTS = fcheck.o fclose.o fctl.o fcurrent.o fcustom.o fdefault.o \
	frefresh.o

# include library definitions
include ../../../lib/Makefile.lib

# inc must be first.
CPPFLAGS = -I../inc $(CPPFLAGS.master)

.KEEP_STATE:

all: $(LIBS)

install: all
	@echo "$(LIBRARY) is not installed."

# include library targets
include ../../../lib/Makefile.targ
