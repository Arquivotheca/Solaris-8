#
#ident	"@(#)Makefile	1.4	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/fmli/oeu/Makefile
#
LIBRARY = liboeu.a

OBJECTS = oeu.o oeucheck.o rm_atob.o genparse.o

# include library definitions
include ../../../lib/Makefile.lib


# inc must be first.
CPPFLAGS = -I../inc $(CPPFLAGS.master)

.KEEP_STATE:

# two objects from one source.
%/oeucheck.o := CPPFLAGS += -DJUSTCHECK

%/oeucheck.o: oeu.c
	$(COMPILE.c) -o $@ oeu.c

all: $(LIBS)

install: all
	@echo "$(LIBRARY) is not installed."

# include library targets
include ../../../lib/Makefile.targ
