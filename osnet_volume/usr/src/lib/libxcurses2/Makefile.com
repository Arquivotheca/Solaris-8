#
#pragma ident	"@(#)Makefile.com	1.5	99/04/19 SMI"
#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libxcurses/Makefile.com
#
LIBRARY=	libcurses.a
VERS=	.2

# objects are grouped by source directory
# all of the libxcurses source files are in src/libc
OBJECTS= $(XCURSES)	$(MKS)	$(WIDE)

# XCURSES source files are in src/libc/xcurses
XCURSES= \
add_wch.o	dupwin.o	longname.o	strname.o	wbrdr.o \
addch.o		echo_wch.o	m_cc.o		termattr.o	wbrdr_st.o \
addchn.o	echochar.o	meta.o		tgetent.o	wchgat.o \
addnstr.o	endwin.o	move.o		tgetflag.o	wclear.o \
addnws.o	flushinp.o	mvcur.o		tgetnum.o	wclrbot.o \
addwchn.o	get_wch.o	mvwin.o		tgetstr.o	wclreol.o \
attr_on.o	getcchar.o	napms.o		tgoto.o		wdelch.o \
attron.o	getch.o		newpad.o	tigetfla.o	wget_wch.o \
baudrate.o	getn_ws.o	newterm.o	tigetnum.o	wgetch.o \
beep.o		getnstr.o	newwin.o	tigetstr.o	wgetn_ws.o \
bkgd.o		getwin.o	noecho.o	timeout.o	whln.o \
bkgdset.o	has.o		nonl.o		touched.o	whln_st.o \
bkgrnd.o	hln.o		numcode.o	touchwin.o	win_wch.o \
bkgrndst.o	hln_st.o	numfnam.o	tparm.o		win_wchn.o \
boolcode.o	in_wch.o	numname.o	tputs.o		winch.o \
boolfnam.o	in_wchn.o	overlay.o	winchn.o \
boolname.o	inch.o		pecho_wc.o	unctrl.o	winnstr.o \
box.o		inchn.o		pechoch.o	vid_attr.o	winnwstr.o \
box_set.o	initscr.o	prefresh.o	vid_puts.o	wins_nws.o \
brdr.o		innstr.o	printw.o	vidattr.o	wins_wch.o \
brdr_st.o	innwstr.o	ptrmove.o	vw_print.o	winsch.o \
cbreak.o	ins_nws.o	qiflush.o	vw_scanw.o	winsdel.o \
chgat.o		ins_wch.o	redraw.o	vwprintw.o	winsnstr.o \
clear.o		insch.o		refresh.o	vwscanw.o	wmove.o \
clearok.o	insnstr.o	savetty.o	wacs.o		wredraw.o \
clrbot.o	intrflsh.o	scanw.o		wadd_wch.o	wrefresh.o \
clreol.o	scr_dump.o	waddch.o	wscrl.o \
color.o		isendwin.o	scrl.o		waddchn.o	wscrreg.o \
copywin.o	key_name.o	scrreg.o	waddnstr.o	wsyncdn.o \
curs_set.o	keyindex.o	setcchar.o	waddnws.o	wsyncup.o \
delay.o		keyname.o	setup.o		waddwchn.o	wtimeout.o \
delch.o		keypad.o	slk.o		wattr_on.o	wtouchln.o \
deleteln.o	killchar.o	strcode.o	wattron.o	wunctrl.o \
doupdate.o	killwch.o	strfnam.o	wbkgrnd.o

# MKS source files are in src/libc/mks
MKS= m_crcpos.o	m_vsscan.o

# WIDE source files are in src/libc/wide
WIDE= wio_get.o	wio_put.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
SRCS=		$(OBJECTS:%.o=../src/libc/xcurses/%.c)

LIBS =		$(DYNLIB) $(LINTLIB)

# definitions for install target
ROOTLIBDIR=	$(ROOT)/usr/xpg4/lib
ROOTLIBDIR64=	$(ROOT)/usr/xpg4/lib/sparcv9
ROOTLIBS=	$(LIBS:%=$(ROOTLIBDIR)/%)

# definitions for lint
$(LINTLIB):= SRCS=../src/libc/llib-lcurses
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= CPPFLAGS +=

LINTOUT=	lint.out
LINTSRC=	$(LINTLIB:%.ln=%)

ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)
ROOTLINTDIR64=	$(ROOTLIBDIR64)

CLEANFILES +=	$(LINTOUT) $(LINTLIB)

DYNFLAGS +=	-Wl,-M,$(MAPFILE)
LDLIBS += -ldl -lc

CPPFLAGS = -I../h $(CPPFLAGS.master)

#
# If and when somebody gets around to messaging this, CLOBBERFILE should not
# be cleared (so that any .po file will be clobbered.
#
CLOBBERFILES=	libcurses.so libcurses.so$(VERS) $(MAPFILE)

.KEEP_STATE:

all: $(LIBS)

lint: $(LINTLIB)

$(DYNLIB): 	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

#
# Include library targets
#
include ../../Makefile.targ

objs/%.o pics/%.o:	../src/libc/xcurses/%.c ../h/term.h \
			../h/m_ord.h ../h/m_wio.h ../h/mks.h \
			../src/libc/xcurses/private.h
	$(COMPILE.c) -I../src/libc/xcurses -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o:	../src/libc/mks/%.c ../h/mks.h
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o:	../src/libc/wide/%.c ../h/mks.h \
			../h/m_wio.h ../src/libc/xcurses/private.h
	$(COMPILE.c) -o $@ $<

# install rule for lint library target
$(ROOTLINTDIR)/%: ../src/libc/%
	$(INS.file)

# install rule for 64 bit lint library target
$(ROOTLINTDIR64)/%: ../src/libc/%
	$(INS.file)
