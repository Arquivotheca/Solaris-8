#
#ident	"@(#)Makefile	1.2	92/12/16 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

DEFRES = 240

LIBRARY= ../libimagen.a

OBJECTS = arc.o box.o charset.o circle.o close.o cont.o dot.o erase.o label.o \
	line.o linemod.o move.o open.o point.o scale.o space.o 

# include library definitions
include ../../../../lib/Makefile.lib

CPPFLAGS += -DDEFRES=$(DEFRES)

BUILD.AR = $(AR) $(ARFLAGS) $@ `$(LORDER) $(AROBJS) | $(TSORT)`


CLOBBERFILES= $(LIBRARY)

.KEEP_STATE:

all: $(LIBS)

include ../../../../lib/Makefile.targ

