#
#ident  "@(#)Makefile 1.3     99/05/07 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sadm/dhcpmgr/com/sun/dhcpmgr/server/Makefile
#


CLASSFILES = 	DhcpNetMgrImpl.class \
		DhcpServiceMgrImpl.class \
		DhcptabMgrImpl.class \
		DhcpMgrImpl.class


include		$(SRC)/lib/Makefile.lib

CLASSPATH=	$(SRC)/cmd/cmd-inet/usr.sadm/dhcpmgr

JAVAFILES =	$(CLASSFILES:.class=.java)

CLEANFILES=	*.class
CLOBBERFILES=

.KEEP_STATE:

all: $(CLASSFILES)

install: all

_msg:

include $(SRC)/lib/Makefile.targ
