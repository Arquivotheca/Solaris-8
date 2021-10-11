#
# Copyright (c) 1992-1995,1999, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.4	00/09/14 SMI" 
#
LIBRARY=pam_krb5.so
VERS= .1
MAPFILE=	../mapfile-vers
MAPFILE64=	../mapfile-vers

CFLAGS	+=	-v
CFLAGS64 +=	-v

# include library definitions
include ../../../../Makefile.master
include $(SRC)/lib/gss_mechs/mech_krb5/Makefile.mech_krb5

OBJECTS=	krb5_authenticate.o \
		krb5_setcred.o \
		krb5_acct_mgmt.o \
		krb5_password.o \
		krb5_session.o \
		utils.o \
		authtok_utils.o \
		kwarnd_clnt_stubs.o \
		kwarnd_clnt.o \
		kwarnd_handle.o \
		kwarnd_xdr.o \
		krpc_sys.o \
		xfn_mapping.o

include ../../../../Makefile.master

# include library definitions
include ../../../Makefile.lib

SRCS= $(OBJECTS:%.o=../%.c)

RPCSYS_DIR = ../../../libc

TEXT_DOMAIN = SUNW_OST_SYSOSPAM
POFILE = pam_krb5.po
POFILES = generic.po

# resolve with local variables in shared library
DYNFLAGS += -Bsymbolic

LINTFLAGS=-I$(ROOT)/usr/include


DYNFLAGS+=	-M $(MAPFILE)
DYNFLAGS32+=	-M $(MAPFILE)
DYNFLAGS64+=	-M $(MAPFILE64)

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

i386_CPPFLAGS = -DDSHLIB
sparc_CPPFLAGS += -D$(MACH)
sparcv9_CPPFLAGS += -D$(MACH)

DYNFLAGS32=	-M $(MAPFILE)
DYNFLAGS64=	-M $(MAPFILE64)

# library dependency
LDLIBS += -lc -lpam -lnsl -lsocket

#overwrite LIBNAME value
LIBNAME=$(LIBRARY:%.so=%)
lint: $(LINTLIB)

CLOBBERFILES += $(LINTLIB) $(LINTOUT)

# include library targets
include ../../../Makefile.targ

$(DYNLIB): 	$(MAPFILE)
$(DYNLIB64): 	$(MAPFILE64)

CPPFLAGS += -I../../../gss_mechs/mech_krb5/include  -I../../.. \
	-I$(SRC)/uts/common/gssapi/include/ \
	-I$(SRC)/uts/common/gssapi/mechs/krb5/include \
	-I$(SRC)/lib/gss_mechs/mech_krb5 \
	-I$(SRC)/lib/krb5 \
	-I. \
	-I$(SRC)/cmd/krb5/kwarn

# add in XFN support
CPPFLAGS += -DXFN_MAPPING -DOPT_INCLUDE_XTHREADS_H -DSVR4

i386_KPIC=
sparc_KPIC=	-I$(SRC)/lib/libc/inc -I$(SRC)/lib/libc/$(MACH)/inc \
		-DPIC -D_TS_ERRNO -K pic
sparcv9_KPIC=	-I$(SRC)/lib/libc/inc -I$(SRC)/lib/libc/$(MACH)/inc \
		-DPIC -D_TS_ERRNO -K pic
KPIC = $($(MACH)_KPIC)

pics/%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

pics/%.o: $(SRC)/cmd/krb5/kwarn/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

_msg: $(MSGDOMAIN) .WAIT $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)
	$(RM) $(POFILES)

$(POFILE): $(DERIVED_FILES) .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

generic.po:
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext *.[ch]`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po

$(MSGDOMAIN):
	$(INS.dir)

