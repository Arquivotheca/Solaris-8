#
#ident	"@(#)Makefile	1.2	90/09/01 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/face/OBJECTS/dos/Makefile

include		../../../Makefile.cmd
include		../../src/Makefile.com

ROOTDIR=	$(ROOTVMSYS)/OBJECTS/dos

OBJFILES=	Form.add Form.mod Form.udconv \
		Menu.install Menu.list Text.conf

ROOTBIN=	$(OBJFILES:%=$(ROOTDIR)/%)

FILEMODE=	755

# install rules

$(ROOTDIR)/% : %
	$(INS.file)

.KEEP_STATE:

all: $(OBJFILES)

install: all $(ROOTDIR) $(ROOTBIN)

$(ROOTDIR):
	-$(INS.dir)

clean clobber lint:
