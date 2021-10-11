#
# Copyright (c) 1992-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com 1.4     99/07/19 SMI"

# this library is private and is statically linked by usr/src/cmd/passwd.c
LIBRARY=pam_smartcard.so
VERS=.1

SCHOBJECTS=	smartcard_authenticate.o smartcard_utils.o

OBJECTS= $(SCHOBJECTS)

include ../../../../Makefile.master
include ../../../Makefile.lib

CPPFLAGS=	$(CPPFLAGS.master)

MKF=../Makefile.com

SRCS= $(OBJECTS:%.o=../%.c)

OWNER= root
GROUP= sys

MAPFILE=	../mapfile
MAPFILE64=	../mapfile

# library dependency
LDLIBS += -lc -lpam -lsmartcard

# resolve with local variables in shared library
DYNFLAGS += -Bsymbolic
CFLAGS	+=	-v -I$(SRC)/lib/smartcard/libsc/include
CFLAGS64 +=	-v
# DYNFLAGS32=	-Wl,-M,$(MAPFILE)
# DYNFLAGS64=	-Wl,-M,$(MAPFILE64)

CPPFLAGS += -I.. 

DYNLIB= $(LIBRARY)$(VERS)
LIBS=$(DYNLIB)
CLOBBERFILES += $(LIBS) $(LINTLIB) $(LINTOUT)
ZCOMBRELOC=

ROOTLIBDIR=	$(ROOT)/usr/lib/security
ROOTLIBS=	$(LIBS:%=$(ROOTLIBDIR)/%)
ROOTLIBDIR64=	$(ROOT)/usr/lib/security/$(MACH64)
ROOTLIBS64=	$(LIBS:%=$(ROOTLIBDIR64)/%)

# $(DYNLIB): 	$(MAPFILE)
# $(DYNLIB64): 	$(MAPFILE64)

$(ROOTLIBDIR):
	$(INS.dir)

$(ROOTLIBDIR64):
	$(INS.dir)

install: $(ROOTLIBDIR) $(BUILD64) $(ROOTLIBDIR64)

.KEEP_STATE:

LIBNAME=$(LIBRARY:%.so=%)
lint: $(LINTLIB)

include ../../../Makefile.targ

objs/%.o pics/%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
