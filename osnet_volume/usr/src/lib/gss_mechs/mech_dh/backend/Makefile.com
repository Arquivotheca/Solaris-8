#
#pragma ident	"@(#)Makefile.com	1.3	99/05/04 SMI"
#
# Copyright (c) 1997,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/gss_mechs/mech_dh/backend/Makefile.com
#
#
# This make file will build mech_dh.so.1. This shared object
# contains all the functionality needed to support Diffie-Hellman GSS-API
# mechanism. 
#

LIBRARY= mech_dh.a
VERS = .1

MECH =	context.o context_establish.o cred.o crypto.o dhmech.o \
	MICwrap.o name.o oid.o seq.o token.o support.o validate.o

DERIVED_OBJS = xdr_token.o

CRYPTO = md5.o

OBJECTS= $(MECH) $(CRYPTO) $(DERIVED_OBJS)


# include library definitions
include ../../../../Makefile.lib

CPPFLAGS += -I../mech -I../crypto -I$(SRC)/uts/common/gssapi/include

$(PICS) := 	CFLAGS += -xF
$(PICS) := 	CCFLAGS += -xF
$(PICS) :=	CFLAGS64 += -xF
$(PICS) :=	CCFLAGS64 += -xF

LIBS = $(DYNLIB)
LIBNAME = $(LIBRARY:%.a=%)

MAPFILE = ../mapfile-vers
DYNFLAGS += -M $(MAPFILE)

LDLIBS +=  -lgss -lsocket -lnsl -lmp -ldl -lc 

RPCGEN += -C
SED = sed

.KEEP_STATE:

CSRCS= $(MECH:%.o=../mech/%.c) $(CRYPTO:%.o=../crypto/%.c)
SRCS=	$(CSRCS)

ROOTLIBDIR = $(ROOT)/usr/lib/gss
ROOTLIBDIR64 = $(ROOT)/usr/lib/$(MACH64)/gss

#LINTFLAGS += -dirout=lint -errfmt=simple
#LINTFLAGS64 += -dirout=lint -errfmt=simple -errchk all
LINTOUT =	lint.out
LINTSRC =	$(LINTLIB:%.ln=%)
ROOTLINTDIR =	$(ROOTLIBDIR)
#ROOTLINT = 	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES += $(LINTOUT) $(LINTLIB)

lint: $(LINTLIB)

$(ROOTLIBDIR):
	$(INS.dir)

$(ROOTLIBDIR64):
	$(INS.dir)

$(OBJS): ../mech/dh_gssapi.h ../mech/token.h ../mech/oid.h


objs/%.o profs/%.o pics/%.o: ../crypto/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../mech/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../profile/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

# include library targets
include ../../../../Makefile.targ
