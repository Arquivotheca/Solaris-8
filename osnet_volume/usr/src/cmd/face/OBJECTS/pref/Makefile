#
#ident	"@(#)Makefile	1.2	90/09/01 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/face/OBJECTS/pref/Makefile

include		../../../Makefile.cmd
include		../../src/Makefile.com

ROOTDIR=	$(ROOTVMSYS)/OBJECTS/pref

OBJFILES=	Form.color Form.display Form.func Form.perm \
		Menu.color Menu.frame Menu.pref

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
