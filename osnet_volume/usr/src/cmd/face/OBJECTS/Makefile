#
#ident	"@(#)Makefile	1.2	90/09/01 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/face/OBJECTS/Makefile

include		../../Makefile.cmd
include		../src/Makefile.com

SUBDIRS=	dos mail spell programs pref lp

all:=		TARGET= all
install:=	TARGET= install

ROOTDIR=	$(ROOTVMSYS)/OBJECTS

OBJFILES=	Text.spell Text.show Text.mfhelp Text.merror Text.help Text.h \
		Text.exit Text.disp \
		Menu.waste Menu.users Menu.remove Menu.programs Menu.print \
		Menu.office Menu.mail Menu.h6.list Menu.h0.toc Menu.find \
		Menu.face Menu.create \
		Form.spell Form.send Form.sec Form.prin Form.org Form.find

ROOTBIN=	$(OBJFILES:%=$(ROOTDIR)/%)

FILEMODE=	755

# install rules

$(ROOTDIR)/% : %
	$(INS.file)

.KEEP_STATE:

all: $(OBJFILES) $(SUBDIRS)

install: all $(ROOTDIR) $(ROOTBIN) $(SUBDIRS)

$(ROOTDIR):
	-$(INS.dir)

clean clobber lint:

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
