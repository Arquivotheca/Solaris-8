#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.1	99/08/24 SMI"
#
# lib/libprtdiag_psr/sparc/Makefile.com

LIBRARY= libprtdiag_psr.a
VERS= .1

#
# PLATFORM_OBJECTS is defined in ./desktop ./wgs ./sunfire ./starfire Makefiles
#
OBJECTS= $(PLATFORM_OBJECTS)

# include library definitions
include ../../../Makefile.lib
include ../../../../Makefile.psm

SRCS=		$(OBJECTS:%.o=./common/%.c)

LIBS = $(DYNLIB)

# definitions for lint

LINTFLAGS=	-u
LINTFLAGS64=	-u
LINTOUT=	lint.out
CLEANFILES=	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v
CFLAGS64 +=	-v

LDLIBS +=	-lc

# removing the -z defs from the options
ZDEFS = 	

.KEEP_STATE:

all: $(LIBS)

lint:	$(LINTLIB)

# include library targets
include ../../../Makefile.targ

objs/%.o pics/%.o: ./common/%.c
	$(COMPILE.c) $(IFLAGS) -o $@ $<
	$(POST_PROCESS_O)
