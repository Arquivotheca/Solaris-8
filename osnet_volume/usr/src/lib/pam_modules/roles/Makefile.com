#
# Copyright (c) 1999, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.2	99/09/16 SMI"
#
LIBRARY=pam_roles.so
VERS= .1

OBJECTS= 	roles.o

include ../../../../Makefile.master

# include library definitions
include ../../../Makefile.lib

SRCS= $(OBJECTS:%.o=../%.c)

# resolve with local variables in shared library
DYNFLAGS += -Bsymbolic

LINTFLAGS=-I$(ROOT)/usr/include

DYNLIB= $(LIBRARY)$(VERS)

# override ROOTLIBDIR and ROOTLINKS
ROOTLIBDIR=	$(ROOT)/usr/lib/security
ROOTLIBS=       $(LIBS:%=$(ROOTLIBDIR)/%)
ROOTLIBDIR64=	$(ROOT)/usr/lib/security/$(MACH64)
ROOTLIBS64=	$(LIBS:%=$(ROOTLIBDIR64)/%)

$(ROOTLIBDIR):
	$(INS.dir)

$(ROOTLIBDIR64):
	$(INS.dir)

install: $(ROOTLIBDIR) $(BUILD64) $(ROOTLIBDIR64)

OWNER= root
GROUP= sys

.KEEP_STATE:

MAPFILE=	../mapfile
MAPFILE64=	../mapfile

CFLAGS	+=	-v -g
CFLAGS64 +=	-v

DYNFLAGS32=	-M $(MAPFILE)
DYNFLAGS64=	-M $(MAPFILE64)

# library dependency
LDLIBS += -lc -lpam -lsocket -lsecdb -lnsl

#overwrite LIBNAME value
LIBNAME=$(LIBRARY:%.so=%)
lint: $(LINTLIB)

CLOBBERFILES += $(LINTLIB) $(LINTOUT)

# include library targets
include ../../../Makefile.targ

$(DYNLIB): 	$(MAPFILE)
$(DYNLIB64): 	$(MAPFILE64)

pics/%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
