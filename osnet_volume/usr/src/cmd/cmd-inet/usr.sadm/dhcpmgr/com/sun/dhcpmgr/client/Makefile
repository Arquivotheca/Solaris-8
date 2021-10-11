#
#ident  "@(#)Makefile 1.6     99/06/02 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sadm/dhcpmgr/com/sun/dhcpmgr/client/Makefile
#

CLASSFILES =	DataManager.class \
                ResourceStrings.class \
		ErrorTable.class \
		DhcptabNameField.class \
		MacroNameField.class \
		OptionNameField.class \
		DhcpmgrDialog.class \
		MultipleOperationDialog.class \
                SelectOptionDialog.class \
                ViewMacroDialog.class \
                CreateMacroDialog.class \
                DeleteMacroDialog.class \
                CreateOptionDialog.class \
                DeleteOptionDialog.class \
                CreateAddressDialog.class \
                DeleteAddressDialog.class \
                AddressWizard.class \
                ModifyAddressesDialog.class \
                ReleaseAddressDialog.class \
                DisableServiceDialog.class \
                ConfigureRelayDialog.class \
                ConfigureChoiceDialog.class \
                ConfigWizard.class \
                ServerOptionsDialog.class \
                DeleteNetworksDialog.class \
                UnconfigureDialog.class \
                AddressView.class \
                OptionView.class \
                MacroView.class \
                RelayView.class \
                DhcpmgrApplet.class

include		$(SRC)/Makefile.master

CLASSPATH=	$(SRC)/cmd/cmd-inet/usr.sadm/dhcpmgr

JAVAFILES =	$(CLASSFILES:.class=.java)

MSGDIR=		$(ROOT)/usr/share/lib/locale/com/sun/dhcpmgr/client
MSGDIRS =       $(ROOT)/usr/share/lib/locale \
                $(ROOT)/usr/share/lib/locale/com \
                $(ROOT)/usr/share/lib/locale/com/sun \
		$(ROOT)/usr/share/lib/locale/com/sun/dhcpmgr \
		$(MSGDIR)

MSGFILES=	ResourceBundle.properties \
		OptionDescriptions.properties
MSGS=		$(MSGFILES:%=$(MSGDIR)/%)

CLEANFILES=	*.class
CLOBBERFILES=

.KEEP_STATE:

all: $(CLASSFILES)

install: all

_msg: $(MSGDIRS) $(MSGS)

$(MSGDIR)/%: %
	$(INS.file)

$(MSGDIRS):
	$(INS.dir)

lint:

clean:
	$(RM) $(CLEANFILES)

clobber: clean
