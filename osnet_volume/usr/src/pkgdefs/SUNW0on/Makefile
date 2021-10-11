#
#ident	"@(#)Makefile	1.8	98/11/13 SMI"
#
# Copyright (c) 1995, by Sun Microsystems, Inc.
# All rights reserved.
#
# pkgdefs/SUNW0on/Makefile

include ../Makefile.com

ROOTDIR=$(ROOT)/usr/lib/locale/C
ROOTMSGDIR=$(ROOTDIR)/LC_MESSAGES
ROOTTIMEDIR=$(ROOTDIR)/LC_TIME

SED=	/usr/bin/sed
CP=	/usr/bin/cp
OWNER=		root
GROUP=		sys
FILEMODE=	644

PO_LABEL=' The following lines are contents of '

.KEEP_STATE:

all: $(FILES)
install: all pkg

include ../Makefile.targ

# The _msg target gathers the output of the top-level _msg target into
# text-domain-specific message files under the ROOTMSGDIR for packaging.
# It also processes time-message files in subdirectories of the LC_TIME
# directory and copies or creates other files needed by this package.
#
_msg:
	[ -d $(ROOTMSGDIR) ] || mkdir -p $(ROOTMSGDIR)
	[ -d $(ROOTTIMEDIR) ] || mkdir -p $(ROOTTIMEDIR)
	[ -d $(MSGROOT) ] || $(ECHO) "need to build _msg in $(SRC)"
	(cd $(MSGROOT); \
	for d in SUNW*;\
	do \
		x="`$(ECHO) $$d/*.po`"; \
		y="$$d/"'*.po'; \
		$(RM) $(ROOTMSGDIR)/$$d.po; \
		if test "$$x" != "$$y"; then \
		for f in $$d/*.po; \
		do \
			$(ECHO) '#'$(PO_LABEL)$$f >> $(ROOTMSGDIR)/$$d.po; \
			$(SED) -e '/^# msg/d' -e '/^domain/d' $$f >> \
			    $(ROOTMSGDIR)/$$d.po; \
		done; \
		$(CHOWN) $(OWNER) $(ROOTMSGDIR)/$$d.po; \
		$(CHGRP) $(GROUP) $(ROOTMSGDIR)/$$d.po; \
		$(CHMOD) $(FILEMODE) $(ROOTMSGDIR)/$$d.po; \
		fi; \
		x="`$(ECHO) $$d/*.msg`"; \
		y="$$d/"'*.msg'; \
		if test "$$x" != "$$y"; then \
		for f in $$d/*.msg; \
		do \
			x="`basename $$f`"; \
			$(RM) $(ROOTMSGDIR)/$$x; \
			$(CP) $$f $(ROOTMSGDIR); \
			$(CHOWN) $(OWNER) $(ROOTMSGDIR)/$$x; \
			$(CHGRP) $(GROUP) $(ROOTMSGDIR)/$$x; \
			$(CHMOD) $(FILEMODE) $(ROOTMSGDIR)/$$x; \
		done; \
		fi; \
	done)
	(cd $(MSGROOT)/LC_TIME; \
	for d in SUNW*;\
	do \
		$(RM) $(ROOTTIMEDIR)/$$d.po; \
		for f in $$d/*.po; \
		do \
			$(ECHO) '#'$(PO_LABEL)$$f >> $(ROOTTIMEDIR)/$$d.po; \
			$(SED) -e '/^# msg/d' -e '/^domain/d' $$f >> \
			    $(ROOTTIMEDIR)/$$d.po; \
		done; \
		$(CHOWN) $(OWNER) $(ROOTTIMEDIR)/$$d.po; \
		$(CHGRP) $(GROUP) $(ROOTTIMEDIR)/$$d.po; \
		$(CHMOD) $(FILEMODE) $(ROOTTIMEDIR)/$$d.po; \
	done)
	-$(RM) $(ROOTMSGDIR)/mailx.help
	$(CP) $(SRC)/cmd/mailx/misc/mailx.help $(ROOTMSGDIR)
	-$(CHOWN) $(OWNER) $(ROOTMSGDIR)/mailx.help
	-$(CHGRP) $(GROUP) $(ROOTMSGDIR)/mailx.help
	-$(CHMOD) $(FILEMODE) $(ROOTMSGDIR)/mailx.help
	-$(RM) $(ROOTMSGDIR)/more.help
	$(CP) $(SRC)/cmd/more/more.help $(ROOTMSGDIR)
	-$(CHOWN) $(OWNER) $(ROOTMSGDIR)/more.help
	-$(CHGRP) $(GROUP) $(ROOTMSGDIR)/more.help
	-$(CHMOD) $(FILEMODE) $(ROOTMSGDIR)/more.help
	-$(RM) $(ROOTMSGDIR)/uxlibc.src
	$(CP) uxlibc.src $(ROOTMSGDIR)
	-$(CHOWN) $(OWNER) $(ROOTMSGDIR)/uxlibc.src
	-$(CHGRP) $(GROUP) $(ROOTMSGDIR)/uxlibc.src
	-$(CHMOD) $(FILEMODE) $(ROOTMSGDIR)/uxlibc.src
	-$(RM) $(ROOTMSGDIR)/SUNW_OST_LINFO
	($(ECHO) "yes"; $(ECHO) "no" ) > $(ROOTMSGDIR)/SUNW_OST_LINFO
	-$(CHOWN) $(OWNER) $(ROOTMSGDIR)/SUNW_OST_LINFO
	-$(CHGRP) $(GROUP) $(ROOTMSGDIR)/SUNW_OST_LINFO
	-$(CHMOD) $(FILEMODE) $(ROOTMSGDIR)/SUNW_OST_LINFO
