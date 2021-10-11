#
# Copyright (c) 1992-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.13	99/12/06 SMI"

# this library is private and is statically linked by usr/src/cmd/passwd.c
PASSWD_LIB=libunixpasswd.a
LIBRARY=pam_unix.so
VERS=.1

SCHOBJECTS=	unix_authenticate.o \
		unix_setcred.o \
		unix_acct_mgmt.o \
		unix_close_session.o \
		unix_open_session.o \
		unix_chauthtok.o \
		unix_update_authtok.o \
		unix_set_authtokattr.o \
		unix_get_authtokattr.o \
		unix_update_authtok_file.o \
		unix_update_authtok_nis.o \
		unix_update_authtok_nisplus.o \
		unix_update_authtok_ldap.o \
		nispasswd_xdr.o \
		npd_clnt.o \
		switch_utils.o \
		unix_utils.o

PASSWDLIB_OBJS=	unix_update_authtok.o \
		unix_set_authtokattr.o \
		unix_get_authtokattr.o \
		unix_update_authtok_file.o \
		unix_update_authtok_nis.o \
		unix_update_authtok_nisplus.o \
		unix_update_authtok_ldap.o \
		nispasswd_xdr.o \
		npd_clnt.o \
		switch_utils.o \
		unix_utils.o

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
LDLIBS += -lc -lpam -lnsl -lcmd -lmp -zlazyload -lsldap

# resolve with local variables in shared library
DYNFLAGS += -Bsymbolic
CFLAGS	+=	-v
CFLAGS64 +=	-v
DYNFLAGS32=	-Wl,-M,$(MAPFILE)
DYNFLAGS64=	-Wl,-M,$(MAPFILE64)

CPPFLAGS += -I$(SRC)/lib/libnsl/include -I$(SRC)/lib/libsldap/common -I.. -DPAM_SECURE_RPC -DPAM_NIS -DPAM_NISPLUS -DPAM_LDAP

PROTOCOL_DIR = ../../../../head/rpcsvc
DERIVED_FILES= nispasswd_xdr.c


DYNLIB= $(LIBRARY)$(VERS)
LIBS=$(DYNLIB) $(PASSWD_LIB)
CLOBBERFILES += $(LIBS) $(LINTLIB) $(LINTOUT) $(DERIVED_FILES)

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

# DERIVED_FILES
nispasswd_xdr.c: $(PROTOCOL_DIR)/nispasswd.x
	$(RPCGEN) -c -C -M $(PROTOCOL_DIR)/nispasswd.x > nispasswd_xdr.c


include ../../../Makefile.targ

UNIX_PASSWD_LIB_OBJS=$(PASSWDLIB_OBJS:%=objs/%)

$(PASSWD_LIB) : $(UNIX_PASSWD_LIB_OBJS)
	$(AR) -r $(PASSWD_LIB) $(UNIX_PASSWD_LIB_OBJS)

objs/%.o pics/%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
