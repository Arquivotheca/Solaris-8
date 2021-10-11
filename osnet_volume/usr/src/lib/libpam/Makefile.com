#
# Copyright (c) 1992-1995,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.5	99/01/25 SMI"
#
# lib/libpam/Makefile.com
#
LIBRARY= libpam.a
VERS= .1

OBJECTS= pam_framework.o pam_framework_utils.o

include ../../../Makefile.master

# include library definitions
include ../../Makefile.lib

SRCS=		$(OBJECTS:%.o=../%.c)

TEXT_DOMAIN=	SUNW_OST_SYSOSPAM
MAPFILE=	$(MAPDIR)/mapfile

CLEANFILES +=	$(LINTOUT) $(LINTLIB)
CLOBBERFILES +=	$(MAPFILE)

CFLAGS +=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	-M $(MAPFILE)

# library dependency
LDLIBS += -ldl -lc

#threads
CPPFLAGS += -DOPT_INCLUDE_XTHREADS_H -DSVR4 -I.. $(CPPFLAGS.master)

.KEEP_STATE:

lint: $(LINTLIB)


$(DYNLIB) : $(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile


# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
