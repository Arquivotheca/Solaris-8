#
#ident	"@(#)Makefile	1.4	94/08/12 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/face/src/oam/users/Makefile

include         ../../../../Makefile.cmd

# include common face definitions and rules
include		../../Makefile.com

# group files by target directory
#
ADDFILES=	Form.define Text.define
MODIFYFILES=	Form.mod Text.mod
REMOVEFILES=	Form.undefine Text.undefine

# derived names of help files used by TMPFILES
HFILES=         add.help list.help modify.help remove.help

APPLFILES=	$(ADDFILES) Menu.ulist $(MODIFYFILES) $(REMOVEFILES) users.help

ROOTUSERS=	$(ROOTFACE)/users

ROOTDIRS=	$(ROOTUSERS) \
		$(ROOTUSERS)/add $(ROOTUSERS)/list \
		$(ROOTUSERS)/modify $(ROOTUSERS)/remove

ROOTFILES=	$(ADDFILES:%=$(ROOTUSERS)/add/%) \
		$(ROOTUSERS)/list/Menu.ulist \
		$(MODIFYFILES:%=$(ROOTUSERS)/modify/%) \
		$(REMOVEFILES:%=$(ROOTUSERS)/remove/%) \
		$(HFILES:%.help=$(ROOTUSERS)/%/Help) \
		$(ROOTUSERS)/Help

GROUP=          sys

FILEMODE=       755

# renamed temporary help files to be able to use pattern matching
TMPFILES=	$(HFILES:%=tmp/%)

# installation rules
$(ROOTUSERS)/%  \
$(ROOTUSERS)/add/%  \
$(ROOTUSERS)/list/% \
$(ROOTUSERS)/modify/% \
$(ROOTUSERS)/remove/% : %
	$(INS.file)

$(ROOTFACE)/%/Help : %.help
	$(INS.rename)

$(ROOTUSERS)/%/Help: tmp/%.help
	$(INS.rename)

.KEEP_STATE:

all: $(ALL) $(TXTS) $(APPLFILES) tmp $(TMPFILES)

install: all $(ROOTDIRS) $(ROOTFILES)

$(ROOTDIRS):
	-$(INS.dir)

tmp:
	-$(MKDIR) tmp

tmp/add.help: fdefine.help
	$(RM) $@; $(CP) fdefine.help $@

tmp/list.help: mulist.help
	$(RM) $@; $(CP) mulist.help $@

tmp/modify.help: fmod.help
	$(RM) $@; $(CP) fmod.help $@

tmp/remove.help: fundefine.help
	$(RM) $@; $(CP) fundefine.help $@

clean clobber:
	$(RM) $(TMPFILES)

FRC:
