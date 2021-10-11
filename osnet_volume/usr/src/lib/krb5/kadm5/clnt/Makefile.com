#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.1	99/07/18 SMI"
#

LIBRARY= libkadm5clnt.a
VERS= .1
MAPFILE= $(MAPDIR)/mapfile

CLNT_OBJS = clnt_policy.o \
	client_rpc.o \
	client_principal.o \
	client_init.o \
	clnt_privs.o \
	clnt_chpass_util.o \
	logger.o 

SHARED_OBJS = \
	alt_prof.o \
	chpass_util.o \
	kadm_rpc_xdr.o \
	misc_free.o \
	kadm_host_srv_names.o \
	str_conv.o

OBJECTS= $(CLNT_OBJS) $(SHARED_OBJS)

# include library definitions
include ../../../Makefile.lib

SRCS=		$(CLNT_OBJS:%.o=../%.c) \
		$(SHARED_OBJS:%.o=../../%.c)

ROOTLIBDIR= 	$(ROOT)/usr/lib/krb5
LIBS=		$(DYNLIB)

sparcv9_CFLAGS += -K PIC

include $(SRC)/lib/gss_mechs/mech_krb5/Makefile.mech_krb5

TEXT_DOMAIN = SUNW_OST_OSLIB
POFILE = $(LIBRARY:%.a=%.po)
POFILES = generic.po

#override liblink
INS.liblink=	-$(RM) $@; $(SYMLINK) $(LIBLINKS)$(VERS) $@

#threads
CPPFLAGS.master=$(DTEXTDOM) \
	$(ENVCPPFLAGS1) $(ENVCPPFLAGS2) \
	$(ENVCPPFLAGS3) $(ENVCPPFLAGS4)
CPPFLAGS=	$(CPPFLAGS.master)

CPPFLAGS += -I.. -I../.. -I../../.. -I$(SRC)/lib/gss_mechs/mech_krb5/include \
	-I$(SRC)/lib/gss_mechs/mech_krb5/include/krb5 \
	-I$(SRC)/uts/common/gssapi/include/ \
	-I$(SRC)/uts/common/gssapi/mechs/krb5/include \
	-DHAVE_STDLIB_H -DUSE_SOLARIS_SHARED_LIBRARIES \
	-DHAVE_LIBSOCKET=1 -DHAVE_LIBNSL=1 -DSETRPCENT_TYPE=void \
	-DENDRPCENT_TYPE=void -DHAVE_SYS_ERRLIST=1 -DNEED_SYS_ERRLIST=1 \
	-DHAVE_SYSLOG_H=1 -DHAVE_OPENLOG=1 -DHAVE_SYSLOG=1 -DHAVE_CLOSELOG=1 \
	-DHAVE_STRFTIME=1 -DHAVE_VSPRINTF=1

HDRS=

# definitions for lint

LINTFLAGS=	-u -I..
LINTFLAGS64=	-u -I..
LINTOUT=	lint.out

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES += 	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v -I..
CFLAGS64 +=	-v -I..
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-lc

CLOBBERFILES += $(MAPFILE)
CLEANFILES += $(MAPFILE)

.KEEP_STATE:

all:	$(LIBS)

lint: $(LINTLIB)

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../../../Makefile.targ

objs/%.o profs/%.o pics/%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../%
	$(INS.file)

install_h:

FRC:

_msg: $(MSGDOMAIN) .WAIT $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(POFILE): $(DERIVED_FILES) .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

generic.po: FRC
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext ../*.[ch] ../../*.[ch]`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po

$(MSGDOMAIN):
	$(INS.dir)
