#
#ident	"@(#)Makefile	1.4	94/08/12 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# /usr/src/cmd/face/src/oam/programs/Makefile

include		../../../../Makefile.cmd
include		../../Makefile.com

#
# mail services install
#

MAILADDFILES=		Text.amail
MAILREMOVEFILES=	Text.dmail
MAILHFILES=		add.help remove.help
MAILHFILETOP=		mail_services.help

MAILAPPLFILES=	$(MAILADDFILES) $(MAILREMOVEFILES) mail_svc.help

ROOTMAIL=	$(ROOTFACE)/mail_services

ROOTMAILDIRS=	$(ROOTMAIL) \
		$(ROOTMAIL)/add $(ROOTMAIL)/remove

ROOTMAILFILES=	$(MAILADDFILES:%=$(ROOTMAIL)/add/%) \
		$(MAILREMOVEFILES:%=$(ROOTMAIL)/remove/%) \
		$(MAILHFILES:%.help=$(ROOTMAIL)/%/Help) \
		$(MAILHFILETOP:%.help=$(ROOTFACE)/%/Help)

# renamed tmp help files to be able to use pattern matching
MAILTMPFILES=	$(MAILHFILES:%=tmp/mail/%) \
		$(MAILHFILETOP:%=tmp/mail/%)

# install rules
$(ROOTMAIL)/%  \
$(ROOTMAIL)/add/%  \
$(ROOTMAIL)/remove/% : %
	$(INS.file)

$(ROOTFACE)/%/Help: tmp/mail/%.help
	$(INS.rename)

$(ROOTMAIL)/%/Help: tmp/mail/%.help
	$(INS.rename)

#
# spell checker install
#

SPELLADDFILES=		Text.aspell
SPELLREMOVEFILES=	Text.dspell
SPELLHFILES=		add.help remove.help
SPELLHFILETOP=		spell_checker.help

SPELLAPPLFILES=	$(SPELLADDFILES) $(SPELLREMOVEFILES) spell_ck.help

ROOTSPELL=	$(ROOTFACE)/spell_checker

ROOTSPELLDIRS=	$(ROOTSPELL) \
		$(ROOTSPELL)/add $(ROOTSPELL)/remove

ROOTSPELLFILES=	$(SPELLADDFILES:%=$(ROOTSPELL)/add/%) \
		$(SPELLREMOVEFILES:%=$(ROOTSPELL)/remove/%) \
		$(SPELLHFILES:%.help=$(ROOTSPELL)/%/Help) \
		$(SPELLHFILETOP:%.help=$(ROOTFACE)/%/Help)

# rename tmp help files
SPELLTMPFILES=	$(SPELLHFILES:%=tmp/spell/%) \
		$(SPELLHFILETOP:%=tmp/spell/%)

# install rules
$(ROOTSPELL)/%  \
$(ROOTSPELL)/add/%  \
$(ROOTSPELL)/remove/% : %
	$(INS.file)

$(ROOTFACE)/%/Help: tmp/spell/%.help
	$(INS.rename)

$(ROOTSPELL)/%/Help: tmp/spell/%.help
	$(INS.rename)

#
# programs install
#

PROGADDFILES=	Form.add
PROGLISTFILES=	Menu.list
PROGMODFILES=	Menu.mod Form.mod
PROGREMOVEFILES=Menu.del Text.conf
PROGHFILES=	add.help list.help modify.help remove.help

PROGAPPLFILES=	$(PROGADDFILES) $(PROGLISTFILES) $(PROGMODFILES) \
		$(PROGREMOVEFILES) programs.help

ROOTPROG=	$(ROOTFACE)/programs

ROOTPROGDIRS=	$(ROOTPROG) \
		$(ROOTPROG)/add $(ROOTPROG)/list \
		$(ROOTPROG)/modify $(ROOTPROG)/remove

ROOTPROGFILES=	$(PROGADDFILES:%=$(ROOTPROG)/add/%) \
		$(PROGLISTFILES:%=$(ROOTPROG)/list/%) \
		$(PROGMODFILES:%=$(ROOTPROG)/modify/%) \
		$(PROGREMOVEFILES:%=$(ROOTPROG)/remove/%) \
		$(PROGHFILES:%.help=$(ROOTPROG)/%/Help) \
		$(ROOTFACE)/programs/Help

# rename tmp help files
PROGTMPFILES=	$(PROGHFILES:%=tmp/%)

# install rules
$(ROOTPROG)/%  \
$(ROOTPROG)/add/%  \
$(ROOTPROG)/list/%  \
$(ROOTPROG)/modify/%  \
$(ROOTPROG)/remove/% : %
	$(INS.file)

$(ROOTFACE)/%/Help: %.help
	$(INS.rename)

$(ROOTPROG)/%/Help: tmp/%.help
	$(INS.rename)

TMPFILES=	$(MAILTMPFILES) $(SPELLTMPFILES) $(PROGTMPFILES)

GROUP=		sys
FILEMODE=	755

.KEEP_STATE:

all: $(MAILAPPLFILES) $(SPELLAPPLFILES) $(PROGAPPLFILES)  \
	tmp tmp/mail tmp/spell $(TMPFILES)

install: all $(ROOTMAILDIRS) $(ROOTMAILFILES) $(ROOTSPELLDIRS)  \
	$(ROOTSPELLFILES) $(ROOTPROGDIRS) $(ROOTPROGFILES)

$(ROOTMAILDIRS):
	-$(INS.dir)

$(ROOTSPELLDIRS):
	-$(INS.dir)

$(ROOTPROGDIRS):
	-$(INS.dir)

tmp tmp/mail tmp/spell:
	-$(MKDIR) $@

tmp/mail/add.help: tamail.help
	$(RM) $@; $(CP) tamail.help $@

tmp/mail/mail_services.help: mail_svc.help
	$(RM) $@; $(CP) mail_svc.help $@

tmp/mail/remove.help: tdmail.help
	$(RM) $@; $(CP) tdmail.help $@

tmp/spell/add.help: taspell.help
	$(RM) $@; $(CP) taspell.help $@

tmp/spell/remove.help: tdspell.help
	$(RM) $@; $(CP) tdspell.help $@

tmp/spell/spell_checker.help: spell_ck.help
	$(RM) $@; $(CP) spell_ck.help $@

tmp/add.help: fadd.help
	$(RM) $@; $(CP) fadd.help $@

tmp/list.help: mlist.help
	$(RM) $@; $(CP) mlist.help $@

tmp/modify.help: fmod.help
	$(RM) $@; $(CP) fmod.help $@

tmp/remove.help: fdel.help
	$(RM) $@; $(CP) fdel.help $@

clean clobber:
	$(RM) $(TMPFILES)

lint:
