#
#ident  "@(#)Makefile 1.6     99/05/07 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sadm/dhcpmgr/com/sun/dhcpmgr/bridge/Makefile
#

JNIHDR_FILES =	com_sun_dhcpmgr_bridge_Bridge.h

CLASSFILES = 	Bridge.class \
		BridgeException.class \
		ExistsException.class \
		MemoryException.class \
		NoEntryException.class \
		HostExistsException.class \
		NoHostsEntryException.class \
		NotRunningException.class

JNICLASSFILES =	Bridge.class

include		$(SRC)/lib/Makefile.lib

CLASSPATH=	$(SRC)/cmd/cmd-inet/usr.sadm/dhcpmgr

JAVAFILES =	$(CLASSFILES:.class=.java)

CLEANFILES=	*.class $(JNIHDR_FILES)
CLOBBERFILES=

.KEEP_STATE:

all: $(CLASSFILES) $(JNIHDR_FILES)

install: all

#
# Build jni header file
# Use $@ instead of the "unreliable" $*
#
$(JNIHDR_FILES): $(JNICLASSFILES)
	$(JAVAH) -jni -classpath $(CLASSPATH) \
		`$(ECHO) $@ | sed 's/.h$$//' | tr _ .`

_msg:

include $(SRC)/lib/Makefile.targ
