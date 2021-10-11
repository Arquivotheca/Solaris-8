#
#ident	"@(#)Makefile.com	1.4	97/11/07 SMI"
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/context/x500/libldap/Makefile
#

LIBRARY= libldap.a
VERS = .1

LIBLDAP_OBJECTS = \
	abandon.o \
	add.o \
	addentry.o \
	bind.o \
	cache.o \
	cldap.o \
	compare.o \
	delete.o \
	dsparse.o \
	error.o \
	free.o \
	friendly.o \
	getattr.o \
	getdn.o \
	getentry.o \
	getfilter.o \
	getvalues.o \
	kbind.o \
	modify.o \
	modrdn.o \
	open.o \
	os-ip.o \
	regex.o \
	request.o \
	result.o \
	sbind.o \
	search.o \
	sort.o \
	srchpref.o \
	ufn.o \
	unbind.o

LIBLBER_OBJECTS = \
	decode.o \
	encode.o \
	io.o

OBJECTS= $(LIBLDAP_OBJECTS) $(LIBLBER_OBJECTS)

# include library definitions
include ../../../../Makefile.libfn

# do after include Makefile.lib, which also sets ROOTLIBDIR
ROOTLIBDIR=	$(ROOT)/usr/lib/fn
ROOTLIBDIR64=	$(ROOT)/usr/lib/fn/$(MACH64)

LIBS = $(DYNLIB)

LDLIBS += -lsocket -lnsl -lc

LDAP_FLAGS = -Dsunos5 -DLDAP_REFERRALS -DLDAP_LDBM \
          -DCLDAP -DLDAP_SHELL -DLDAP_PASSWD -DLDBM_USE_NDBM \
          -DFILTERFILE="\"/etc/fn/ldapfilter.conf\"" \
          -DTEMPLATEFILE="\"/etc/fn/ldaptemplates.conf\""

debug := LDAP_FLAGS += -DLDAP_DEBUG

CFLAGS += $(LDAP_FLAGS)
CFLAGS64 += $(LDAP_FLAGS)

LINTFLAGS += $(LDAP_FLAGS)

.KEEP_STATE:

all: $(LIBS)

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

lint:	$(SRC:.c=.ln) $(LINTLIB)

# include library targets
include ../../../../Makefile.targ

objs/%.o profs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
