#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile.com	1.1 99/07/11 SMI"
#
#	Common makefile for AMI PAM module
#

LIBRARY=pam_ami.so
VERS=.1

OBJECTS=	ami_authenticate.o \
		ami_setcred.o \
		ami_chauthtok.o \
		ami_utils.o

include ../../../../Makefile.master
include ../../../Makefile.lib

CPPFLAGS=	$(CPPFLAGS.master)

SRCS= $(OBJECTS:%.o=../%.c)

OWNER= root
GROUP= sys

MAPFILE=	../mapfile
MAPFILE64=	../mapfile

# library dependency
LDLIBS += -lpam -lnsl -lami -lthread -lc

# resolve with local variables in shared library
DYNFLAGS += -Bsymbolic
CFLAGS	+=	-v
CFLAGS64 +=	-v
DYNFLAGS32=	-Wl,-M,$(MAPFILE)
DYNFLAGS64=	-Wl,-M,$(MAPFILE64)

CPPFLAGS += -I.. -I../../../libpam

DYNLIB= $(LIBRARY)$(VERS)
LIBS=$(DYNLIB)
CLOBBERFILES += $(LIBS) $(LINTLIB) $(LINTOUT)

ROOTLIBDIR=	$(ROOT)/usr/lib/security
ROOTLIBS=	$(LIBS:%=$(ROOTLIBDIR)/%)
ROOTLIBDIR64=	$(ROOT)/usr/lib/security/$(MACH64)
ROOTLIBS64=	$(LIBS:%=$(ROOTLIBDIR64)/%)

$(DYNLIB): 	$(MAPFILE)
$(DYNLIB64): 	$(MAPFILE64)

$(ROOTLIBDIR):
	$(INS.dir)

$(ROOTLIBDIR64):
	$(INS.dir)

install: $(ROOTLIBDIR) $(BUILD64) $(ROOTLIBDIR64)

.KEEP_STATE:

LIBNAME=$(LIBRARY:%.so=%)
lint: $(LINTLIB)
ROOTLINTS=$(LINTLIB:%=$(ROOTLIBDIR)/%)
ROOTLINTS64=$(LINTLIB:%=$(ROOTLIBDIR64)/%)

include ../../../Makefile.targ

objs/%.o pics/%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
