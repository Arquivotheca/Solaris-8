#
#ident	"@(#)Makefile	1.3	92/07/14 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# lib/libgenIO/Makefile
#
LIBRARY= libgenIO.a

OBJECTS= g_init.o   g_read.o   g_write.o

# include library definitions
include ../Makefile.lib

.KEEP_STATE:

all: $(TXTS) $(LIBS)

install: $(ROOTLIBS)

# include library targets
include ../Makefile.targ
