#
#ident	"@(#)Makefile	1.3	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/face/src/proc/Makefile
#
LIBRARY = libproc.a

OBJECTS = suspend.o suspendcheck.o

# include library definitions
include ../../../../lib/Makefile.lib

SRCS = suspend.c


# inc must be first.
CPPFLAGS = -I../filecab/inc $(CPPFLAGS.master)

#
ARFLAGS = rv

#two objects from one source.
%/suspendcheck.o := CPPFLAGS += -DJUSTCHECK

.KEEP_STATE:

all: $(LIBS)

%/suspendcheck.o: suspend.c
	$(COMPILE.c) -o $@ suspend.c

install: all
	@echo "$(LIBRARY) is not installed."

# include library targets
include ../../../../lib/Makefile.targ
