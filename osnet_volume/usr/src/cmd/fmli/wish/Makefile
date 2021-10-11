#
#ident	"@(#)Makefile	1.4	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/fmli/wish/Makefile
#
LIBRARY = libwish.a

OBJECTS = browse.o display.o error.o flush.o getstring.o global.o \
	mudge.o objop.o stubs.o virtual.o wdwcreate.o wdwlist.o \
	wdwmgmt.o

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
