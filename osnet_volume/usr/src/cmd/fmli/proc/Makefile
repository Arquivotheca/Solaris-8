#
#ident	"@(#)Makefile	1.4	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/fmli/proc/Makefile
#
LIBRARY = libproc.a

OBJECTS = pclose.o pcurrent.o pctl.o pdefault.o list.o pnoncur.o \
	open.o suspend.o

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
