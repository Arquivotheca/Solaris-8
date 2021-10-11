#
# Copyright (c) 1990-1992,1994,1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.20	99/01/25 SMI"
#
# lib/libcurses/Makefile
#
# The builds occur in isa subdirectories.
#

include ../../Makefile.master
include	../Makefile.lib

SUBDIRS = spec .WAIT $(MACH) $(BUILD64) $(MACH64)

# conditional assignments
all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
test :=		TARGET= test
_msg :=		TARGET= _msg

SCRFILES = screen/keycaps \
	screen/curses.h \
	screen/keyname.c \
	screen/termcap.c \
	screen/tifget.c \
	screen/tiget.c \
	screen/tifnames.c \
	screen/tnames.c \
	screen/tinames.c \
	screen/term.h

# definitions for install_h target
#
#
HDRS= 		curses.h term.h unctrl.h
ROOTHDRDIR= 	$(ROOT)/usr/include
ROOTHDRS= 	$(HDRS:%=$(ROOTHDRDIR)/%)

LIBRARY=	libcurses.a
TEXT_DOMAIN=	SUNW_OST_OSLIB
XGETFLAGS=	-a
MSGDIRS=	screen
POFILE=		$(LIBRARY:.a=.po)
POFILES=	generic.po

SED=	sed
GREP=	grep
CP=	cp

# term.h doesn't pass cstyle due to numerous lines exceeding the 80 character
# limit.  maketerm.ed can probably be fixed to prevent this, but it is arguable
# if this work would ever be worth it.
#
screen/term.check      := CSTYLE_TAIL = | grep -v "line > 80 characters" | true

CHECKHDRS= 	$(HDRS:%.h=screen/%.check)

# install rule for install_h target
$(ROOTHDRDIR)/%: screen/%
	$(INS.file)

.KEEP_STATE:

all: $(SCRFILES) .WAIT $(SUBDIRS)

install: all .WAIT $(SUBDIRS)

clean clobber: $(SUBDIRS)
	$(RM) $(SCRFILES)

lint test: $(SUBDIRS)

install_h: $(ROOTHDRS)

check:	$(CHECKHDRS)

scrfiles:	$(SCRFILES)

$(MACH) $(MACH64) spec: FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

ED = ed

screen/curses.h:	screen/curses.ed screen/keycaps screen/tmp
	(cd screen; $(ED) - < curses.ed)

screen/keycaps:		screen/caps
	grep "KEY_" screen/caps > screen/keycaps
	@if tail -1 screen/keycaps | grep 'KEY_MOUSE' > /dev/null;then :; \
		else echo new keys!! must change setkeymap.c; exit 1;fi

screen/keyname.c:    screen/keyname.sh screen/keycaps
	(cd screen; sh keyname.sh)

screen/termcap.c:    screen/termcap.ed
	(cd screen; $(ED) - < termcap.ed)
	(cd screen; $(RM) bool num str)

screen/tifget.c:     screen/tifget.ed
	(cd screen; $(ED) - < tifget.ed)

screen/tiget.c:      screen/tiget.ed
	(cd screen; $(ED) - < tiget.ed)

screen/tnames.c screen/tinames.c screen/tifnames.c \
screen/term.h:   screen/maketerm.ed
	(cd screen; $(ED) - < maketerm.ed)

screen/tmp:
	rm -rf $@
	mkdir $@
#
# The cscope.out file is made in the current directory and spans all
# supported architectures.
#
# Things to note:
#	1. We use relative names for cscope and tags.
#	2. We *don't* remove the old cscope.out file, because cscope is
#	   smart enough to only build what has changed.  It can be
#	   confused, however, if files are renamed or removed, so it may
#	   be necessary to manually remove cscope.out if a lot of
#	   reorganization has occured.
#
CSCOPE			= cscope
CSDIR			= screen
CSINCS			= -I$(CSDIR) -I$(CSDIR)/screen
CSPATHS			= $(CSDIR)

.PRECIOUS:	cscope.out

cscope.out: cscope.files FRC
	${CSCOPE} -b -f `pwd`/cscope.out

cscope.files: FRC
	@-$(RM) cscope.files
	echo "$(CSINCS)" > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
	    -type d -name '.del-*' -prune -o -type f \
	    \( -name '*.[csh]' -o -name 'Makefile*' \) \
	    -print >> cscope.files
	@wc -l cscope.files

_msg:	$(MSGDOMAIN) $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(POFILE): .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

generic.po:
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext *.[ch]* */*.[ch]*`
	$(SED) -e '/^# msg/d' -e '/^domain/d' messages.po > $@
	$(RM) messages.po

FRC:
