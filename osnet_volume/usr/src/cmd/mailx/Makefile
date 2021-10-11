#
#ident	"@(#)Makefile	1.25	96/03/19 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/mailx/Makefile

PROG= mailx

OBJS=	myfopen.o aux.o cmd1.o cmd2.o cmd3.o \
	cmd4.o cmdtab.o collect.o config.o edit.o \
	init.o fio.o getname.o head.o \
	hostname.o lex.o list.o lock.o lpaths.o \
	main.o names.o optim.o popen.o quit.o \
	receipt.o send.o sigretro.o stralloc.o temp.o \
	translate.o tty.o $(UNIX).local.o vars.o

SRCS=	$(OBJS:.o=.c)

VEROBJ=	version.o
VERSRC= $(VEROBJ:.o=.c)
MAILXVERSION= 5.0
MISC= misc
GREP= grep

include ../Makefile.cmd
#
# for messaging catalogue file
#
POFILE= mailx.po
POFILES= $(SRCS:%.c=%.po)

ROOTMAILXD=	$(ROOTSHLIB)/mailx
ROOTETCMAILD=	$(ROOTETC)/mail
MAILXHELP=	mailx.help mailx.help.~
ROOTMAILXHELP=	$(MAILXHELP:%=$(ROOTMAILXD)/%)
MAILXRC=	$(MISC)/mailx.rc
UCBMAILRC=	$(MISC)/Mail.rc
ROOTMAILXRC=	$(MAILXRC:$(MISC)/%=$(ROOTETCMAILD)/%)
ROOTUCBMAILRC=	$(UCBMAILRC:$(MISC)/%=$(ROOTETCMAILD)/%)
ROOTUCBMAIL=	$(ROOT)/usr/ucb/mail
ROOTUCBCAPMAIL=	$(ROOT)/usr/ucb/Mail
TXTS=		usg.local.c v7.local.c

$(ROOTPROG) :=		FILEMODE =	02511
$(ROOTPROG) :=		GROUP =		mail
$(ROOTMAILXHELP) :=	FILEMODE =	0644
$(ROOTMAILXRC) :=	FILEMODE =	0644
$(ROOTUCBMAILRC) :=	FILEMODE =	0644

CPPFLAGS=	-Ihdr -DUSG -DOPTIM $(CPPFLAGS.master)
# uncomment the following two lines to compile with C++
#CC=CC
#CFLAGS=		-O
# comment out the following line when compiling with C++
CCMODE=		-Xa
UNIX=usg
LINTFLAGS= 	-hb
LDLIBS +=	-lmail

# install rules
$(ROOTMAILXD)/% : %
	$(INS.file)

$(ROOTETCMAILD)/% : $(MISC)/%
	$(INS.file)

.KEEP_STATE:

all: $(PROG) $(MAILXHELP) $(MAILXRC) $(UCBMAILRC) $(TXTS)

$(PROG): $(OBJS) $(VEROBJ) $(LIBMAIL)
	$(LINK.c) $(OBJS) $(VEROBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(VEROBJ): $(VERSRC)
	$(COMPILE.c) $(VERSRC)

# want to recreate version.c whenever (and only when) mailx needs to be rebuilt
$(VERSRC): $(OBJS) $(LIBMAIL)
	$(ECHO) "extern const char *const version=\"mailx version $(MAILXVERSION) `date`\";" > $@

install: all $(ROOTMAILXD) $(ROOTPROG) $(ROOTMAILXHELP) \
	$(ROOTMAILXRC) $(ROOTUCBMAIL) $(ROOTUCBCAPMAIL) $(ROOTUCBMAILRC)

$(ROOTUCBMAIL) $(ROOTUCBCAPMAIL):
	$(RM) $@; $(SYMLINK) ../bin/mailx $@

$(ROOTMAILXD):
	$(INS.dir)

$(MAILXHELP): $(MISC)/$$@
	$(GREP) -v '^#.*@(' $(MISC)/$@ > $@

$(POFILE):      $(POFILES)
	$(RM)	$@
	cat     $(POFILES)      > $@

clean:
	$(RM) $(OBJS) $(VEROBJS) version.c

lint: lint_SRCS

include ../Makefile.targ

FRC:
