#
#pragma ident	"@(#)Makefile.com	1.7	99/10/21 SMI"
#
# Copyright (c) 1996-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ldap/Makefile.com
#
include ../../Makefile.cmd

LDAPMOD=	ldapmodify
LDAPMODSRC=	$(LDAPMOD:%=../common/%.c)
LDAPADD=	ldapadd
LDAPPROG=	ldapmodrdn ldapsearch ldapdelete
#LDAPOBJS=	ldapmodrdn.o ldapsearch.o ldapdelete.o

# LDAP Naming service commands
NSLDAPPROG=	ldaplist
NSLDAPSRC=	ldaplist.c mapping.c printResult.c
NSLDAPOBJ=	$(NSLDAPSRC:%.c=%.o)

# LDAP client creation that uses libsldap
LDAPCLIENT=	ldapclient
LDAPGENPROF=	ldap_gen_profile
CLIENTSRC=	ldapclient.c
CLIENTOBJ=	$(CLIENTSRC:%.c=%.o)

OBJS=		$(NSLDAPOBJ) $(CLIENTOBJ)
SRCS=		$(LDAPPROG:%=../common/%.c) $(LDAPMODSRC) $(NSLDAPSRC) \
		$(CLIENTSRC)
ROOTUSRSBIN=	$(ROOT)/usr/sbin
ROOTUSBPROG=	$(LDAPCLIENT:%=$(ROOTUSRSBIN)/%)
ROOTUSBLINKS=	$(ROOTUSRSBIN)/$(LDAPGENPROF)

PROG=		$(LDAPPROG) $(LDAPMOD) $(NSLDAPPROG)
ROOTADD=	$(ROOTBIN)/$(LDAPADD)
ROOTMOD=	$(ROOTBIN)/$(LDAPMOD)
ALLPROG=	all $(ROOTADD)

CLOBBERFILES += $(NSLDAPOBJ) $(CLIENTOBJ) $(LDAPCLIENT)

# creating /var/ldap directory
ROOTVAR_LDAP=	$(ROOT)/var/ldap
$(ROOTVAR_LDAP) :=				OWNER=		root
$(ROOTVAR_LDAP) :=				GROUP=		sys

all:=           TARGET= all
install:=       TARGET= install
clean:=         TARGET= clean
clobber:=       TARGET= clobber
lint:=          TARGET= lint

CPPFLAGS +=	-DSUN -DSVR4 -D_SYS_STREAM_H
LDLIBS +=	$(COMPLIB) -lldap -lsocket

.KEEP_STATE:

all:	$(PROG) $(LDAPCLIENT)

$(LDAPADD):	$(LDAPMOD)
		@$(RM) $(LDAPADD); $(LN) $(LDAPMOD) $(LDAPADD)

$(LDAPPROG):	../common/$$@.c
		$(LINK.c) -o $@ ../common/$@.c $(LDLIBS)
		$(POST_PROCESS)

ldapmodify:	../common/$$@.c
		$(LINK.c) -o $@ ../common/$@.c $(LDLIBS) -lpthread
		$(POST_PROCESS)

%.o:		../ns_ldap/%.c
		$(COMPILE.c) -o $@ $<
		$(POST_PROCESS_O)

ldaplist:	$(NSLDAPOBJ)
		$(LINK.c) -o $@ $(NSLDAPOBJ) $(LDLIBS) -lsldap
		$(POST_PROCESS)

ldapclient:	$(CLIENTOBJ)
		$(LINK.c) -o $@ $(CLIENTOBJ) $(LDLIBS) -lsldap
		$(POST_PROCESS)

install: all $(ROOTVAR_LDAP) $(ROOTADD) $(ROOTUSBPROG) $(ROOTUSBLINKS)

$(ROOTVAR_LDAP):
		$(INS.dir)

$(ROOTADD):	$(ROOTPROG)
		$(RM) $@
		$(LN) $(ROOTMOD) $@

$(ROOTUSBLINKS):   $(ROOTUSBPROG)
	$(RM) $@
	$(LN) $(ROOTUSBPROG) $@

FRC:

clean:

lint:	lint_SRCS

include ../../Makefile.targ
