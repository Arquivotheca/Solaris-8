#
#ident	"@(#)Makefile	1.4	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/fmli/qued/Makefile
#
LIBRARY = libqued.a

OBJECTS = acs_io.o arrows.o copyfield.o fclear.o fgo.o fstream.o \
	fput.o fread.o initfield.o editmulti.o editsingle.o \
	getfield.o mfuncs.o multiline.o putfield.o setfield.o \
	scrollbuf.o sfuncs.o singleline.o vfuncs.o wrap.o

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
