#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.37	99/11/20 SMI"
#

include ../Makefile.com

DATAFILES +=	i.cronroot i.deflogin i.defsu i.etcremote 	\
		i.group	i.inittab i.etcrpc i.etcsystem \
		i.inetdconf i.iuap i.nsswitch i.netconfig i.passwd	\
		i.preserve i.renamenew i.syslogconf i.tiservices	\
		i.ttysrch i.definit i.etcprofile i.mailxrc i.shadow	\
		i.voldconf i.rmmconf i.sock2path i.protocols \
		i.drvalias \
		i.initd \
		i.locallogin \
		i.localprofile \
		i.nametomajor \
		i.nscd \
		i.fstypes \
		i.pamconf i.nfssecconf i.services

MACHDATAFILES = i.devlinktab i.kclasses i.minorperm
CLOBBERFILES += $(MACHDATAFILES)

.KEEP_STATE:

all: $(FILES) $(MACHDATAFILES)
install: all pkg

include ../Makefile.targ
