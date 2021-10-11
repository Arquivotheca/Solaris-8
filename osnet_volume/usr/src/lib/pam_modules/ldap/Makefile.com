#
# ident	"@(#)Makefile.com	1.1	99/07/07 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/pam_modules/ldap/Makefile.com
#

LIBRARY=pam_ldap.so
VERS= .1

OBJECTS= 	ldap_authenticate.o \
		ldap_setcred.o \
		ldap_acct_mgmt.o \
		ldap_close_session.o \
		ldap_open_session.o \
		ldap_chauthtok.o \
		update_password.o \
		ldap_utils.o

include ../../../../Makefile.master

# include library definitions
include ../../../Makefile.lib

CPPFLAGS=       $(CPPFLAGS.master)

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
DYNFLAGS32=	-M $(MAPFILE)
DYNFLAGS64=	-M $(MAPFILE64)

CPPFLAGS += -I$(SRC)/lib/libsldap/common

# library dependency
LDLIBS += -lc -lpam -lnsl -lcmd -lsldap

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
