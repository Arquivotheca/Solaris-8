#
# ident "@(#)Makefile 1.1     99/07/07 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ldap_cachemgr/Makefile
#

PROG=	ldap_cachemgr

include ../Makefile.cmd

#
# To compile in the $mgr SLP support
# 1. add -DSLP in to CPPFLAGS line
# 2. add cachemgr_discovery.o to the OBJS line
# 3. add -lslp in the LDLIBS line
#

ROOTLDAPLIB=	$(ROOT)/usr/lib/ldap
ROOTLDAPPROG=	$(PROG:%=$(ROOTLDAPLIB)/%)

OBJS=	cachemgr.o cachemgr_getldap.o cachemgr_parse.o

SRCS=	${OBJS:%.o=%.c} 

CPPFLAGS += 	-D_REENTRANT -DSUN_THREADS \
		-I../../lib/libsldap/common

# Message files
POFILE=		ldap_cachemgr.po
POFILES=	${OBJS:%.o=%.po}

LINTOUT=        lint.out

# TCOV_FLAG=	-ql
# GPROF_FLAG=	-xpg
# DEBUG_FLAG=	-g

LDLIBS += 	-ldoor -lnsl -lsldap -mt

# install macros and rule
#
GROUP=	bin
OWNER=	root

.KEEP_STATE:

clean := TARGET= clean
clobber := TARGET= clobber


all: $(PROG)

${PROG}: ${OBJS}
	${LINK.c} ${OPT} -o $@ ${OBJS} ${LDLIBS}
	${POST_PROCESS}

$(ROOTLDAPLIB):
	$(INS.dir)

$(ROOTLDAPLIB)/%:	%
	$(INS.file)

lint :=	LINTFLAGS=-x -b -u

lint:
	$(LINT.c) ${SRCS}  > $(LINTOUT) 2>&1

cstyle:
	${CSTYLE} ${SRCS}

install: all $(ROOTLDAPLIB) $(ROOTLDAPPROG)

clean:     
	${RM} ${OBJS} ${POFILE} ${POFILES}

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

include $(SRC)/cmd/Makefile.targ
