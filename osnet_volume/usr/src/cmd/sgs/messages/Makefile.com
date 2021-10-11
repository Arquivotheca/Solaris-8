#
#ident	"@(#)Makefile.com	1.5	99/09/14 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/messages/Makefile.com

include		$(SRC)/Makefile.master
include		$(SRC)/cmd/sgs/Makefile.com


# Establish our own domain.

TEXT_DOMAIN=	SUNW_OST_SGS

POFILE=		sgs.po

MSGFMT=		msgfmt

# The following message files are generated as part of each utilites build via
# sgsmsg(1l).  By default each file is formatted as a portable object file
# (.po) - see msgfmt(1).  If the sgsmsg -C option has been employed, each file
# is formatted as a message text source file (.msg) - see gencat(1).

POFILES=	ld		ldd		libld		liblddbg \
		libldstab	librtld		rtld		libelf \
		ldprof		libcrle		crle		pvs \
		elfdump


# Define a local version of the message catalog.  Test using: LANG=jive

MSGDIR=		$(ROOT)/usr/lib/locale/jive/LC_MESSAGES
TEST_MSGID=	test-msgid.po
TEST_MSGSTR=	test-msgstr.po
TEST_POFILE=	test-msg.po
TEST_MOFILE=	$(TEXT_DOMAIN).mo


CLEANFILES=	$(POFILE) $(TEST_MSGID) $(TEST_MSGSTR) $(TEST_POFILE) \
		$(TEST_MOFILE)
CLOBBERFILES=	$(POFILES)
