#
#ident  "@(#)Makefile 1.6     99/10/12 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sadm/dhcpmgr/com/sun/dhcpmgr/data/Makefile
#

DEFAULTS_INTERFACE = DhcpDefaults.java

CLASSFILES =	AsciiOptionValue.class \
		BogusOptionValue.class \
		BooleanOptionValue.class \
		DhcpClientRecord.class \
		DhcpDatastore.class \
		DhcpResource.class \
		DhcpdOptions.class \
		DhcptabRecord.class \
		IPAddress.class \
		IPInterface.class \
		IPOptionValue.class \
		IncludeOptionValue.class \
		Macro.class \
		Network.class \
		NumberOptionValue.class \
		OctetOptionValue.class \
		Option.class \
		OptionValueFactory.class \
		OptionsTable.class \
		QuickSort.class \
		ResourceStrings.class \
		StandardOptions.class \
		ValidationException.class

include		$(SRC)/Makefile.master

CLASSPATH=	$(SRC)/cmd/cmd-inet/usr.sadm/dhcpmgr

JAVAFILES =	$(CLASSFILES:.class=.java)

MSGDIR=		$(ROOT)/usr/share/lib/locale/com/sun/dhcpmgr/data
MSGDIRS =       $(ROOT)/usr/share/lib/locale \
                $(ROOT)/usr/share/lib/locale/com \
                $(ROOT)/usr/share/lib/locale/com/sun \
		$(ROOT)/usr/share/lib/locale/com/sun/dhcpmgr \
		$(MSGDIR)

MSGFILES=	ResourceBundle.properties
MSGS=		$(MSGFILES:%=$(MSGDIR)/%)

CLEANFILES=	*.class $(DEFAULTS_INTERFACE)
CLOBBERFILES=

.KEEP_STATE:

all: $(DEFAULTS_INTERFACE) $(CLASSFILES)

install: all

$(DEFAULTS_INTERFACE): $(SRC)/common/net/dhcp/dhcp_defaults.m4
	$(M4) -Djava $? > $@

_msg: $(MSGDIRS) $(MSGS)

$(MSGDIR)/%: %
	$(INS.file)

$(MSGDIRS):
	$(INS.dir)

lint:

clean:
	$(RM) $(CLEANFILES)

clobber: clean
