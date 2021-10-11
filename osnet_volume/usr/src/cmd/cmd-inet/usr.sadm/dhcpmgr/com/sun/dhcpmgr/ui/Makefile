#
#ident  "@(#)Makefile 1.5     99/05/26 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sadm/dhcpmgr/com/sun/dhcpmgr/ui/Makefile
#

CLASSFILES =	ALIGNMENT.class \
		AutosizingTable.class \
		ButtonLayout.class \
		ButtonPanel.class \
		DownButton.class \
		ExtendedCellRenderer.class \
		FieldLayout.class \
		HelpIds.class \
		HostnameField.class \
		IPAddressField.class \
		IPAddressList.class \
		ImageButton.class \
		IntegerField.class \
		LeftButton.class \
		MainFrame.class \
		NoSpaceField.class \
		ProgressManager.class \
		ProportionalLayout.class \
		ResourceStrings.class \
		RightButton.class \
		SortedHeaderRenderer.class \
		SwingWorker.class \
		TableMap.class \
		TableSorter.class \
		UpButton.class \
		VerticalButtonLayout.class \
		Wizard.class

include		$(SRC)/Makefile.master

CLASSPATH=	$(SRC)/cmd/cmd-inet/usr.sadm/dhcpmgr

JAVAFILES =	$(CLASSFILES:.class=.java)

MSGDIR=		$(ROOT)/usr/share/lib/locale/com/sun/dhcpmgr/ui
MSGDIRS =       $(ROOT)/usr/share/lib/locale \
                $(ROOT)/usr/share/lib/locale/com \
                $(ROOT)/usr/share/lib/locale/com/sun \
		$(ROOT)/usr/share/lib/locale/com/sun/dhcpmgr \
		$(MSGDIR)

MSGFILES=	ResourceBundle.properties
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
