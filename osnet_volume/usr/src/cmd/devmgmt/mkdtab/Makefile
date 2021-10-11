#
#ident	"@(#)Makefile	1.3	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/devintf/mkdtab/Makefile
#

PROG= 	mkdtab
PKGINFO=#pkginfo
TXTS= 	$(PKGINFO)
APPLDIR = $(ROOT)/usr/sadm/sysadm/bin
APPLROOT = $(PROG:%=$(APPLDIR)/%)

include ../../Makefile.cmd

LDLIBS += -ladm -lgen

FILEMODE= 0755

CFLAGS += -v

.KEEP_STATE:

all: $(PROG) $(TXTS)

install: all $(APPLDIR) $(APPLROOT)

$(APPLDIR):
	$(INS.dir)

$(APPLDIR)/%: %
	$(INS.file)

clean:

lint:	lint_PROG

include ../../Makefile.targ
