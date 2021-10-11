#
# Copyright (c) 1992-1995, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.3	98/05/18 SMI"
#
LIBRARY=pam_dial_auth.so
VERS= .1

OBJECTS= dial_auth.o
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

CFLAGS	+=	-v
CFLAGS64 +=	-v

DYNFLAGS32 +=	-M $(MAPFILE)
DYNFLAGS64 +=	-M $(MAPFILE64)

#overwrite LIBNAME value
LIBNAME=$(LIBRARY:%.so=%)
lint: $(LINTLIB)

CLOBBERFILES += $(LINTLIB) $(LINTOUT)

# include library targets
include ../../../Makefile.targ

# library dependency
LDLIBS += -lc -lpam -lnsl -lsocket

$(DYNLIB): 	$(MAPFILE)
$(DYNLIB64): 	$(MAPFILE64)

pics/%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
