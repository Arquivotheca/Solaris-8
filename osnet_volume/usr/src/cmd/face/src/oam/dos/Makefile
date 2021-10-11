#
#ident	"@(#)Makefile	1.3	90/09/01 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/face/src/oam/dos/Makefile

include		../../../../Makefile.cmd

# include common face definitions and rules
include		../../Makefile.com

ADDFILES=	Form.add
LISTFILES=	Menu.list
MODIFYFILES=	Menu.mod Form.mod
REMOVEFILES=	Menu.del Text.conf
HFILES=		add.help list.help modify.help remove.help

APPLFILES=	$(ADDFILES) $(LISTFILES) $(MODIFYFILES) $(REMOVEFILES) \
		$(HFILES) Menu.dos dos.help

ROOTDOS=	$(ROOTFACE)/dos_programs

ROOTDIRS=	$(ROOTDOS) $(ROOTDOS)/add $(ROOTDOS)/list \
		$(ROOTDOS)/modify $(ROOTDOS)/remove

ROOTFILES=	$(ADDFILES:%=$(ROOTDOS)/add/%) \
		$(LISTFILES:%=$(ROOTDOS)/list/%) \
		$(MODIFYFILES:%=$(ROOTDOS)/modify/%) \
		$(REMOVEFILES:%=$(ROOTDOS)/remove/%) \
		$(HFILES:%.help=$(ROOTDOS)/%/Help) \
		$(ROOTDOS)/Help \
		$(ROOTDOS)/Menu.dos

GROUP=		sys

FILEMODE=	755

# installation rules
$(ROOTDOS)/% \
$(ROOTDOS)/add/% \
$(ROOTDOS)/list/% \
$(ROOTDOS)/modify/% \
$(ROOTDOS)/remove/% : %
	$(INS.file)

$(ROOTDOS)/%/Help: %.help
	$(INS.rename)

$(ROOTFACE)/%_programs/Help: %.help
	$(INS.rename)

.KEEP_STATE:

all: $(APPLFILES)

install: all $(ROOTDIRS) $(ROOTFILES)

$(ROOTDIRS):
	-$(INS.dir)

clean clobber list:

FRC:
