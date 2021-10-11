#
# ident	"@(#)Makefile	1.14	99/08/16 SMI"	/* SVr4.0 1.13	*/
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/lp/lib/oam/Makefile
#
include         ../../Makefile.lp

LIBRARY =		liblpoam.a

TEXTOBJS =		e_adm__msgs.o	\
			e_can__msgs.o	\
			e_cmb__msgs.o	\
			e_fl__msgs.o	\
			e_for__msgs.o	\
			e_lp__msgs.o	\
			e_lpp__msgs.o	\
			e_lpu__msgs.o	\
			e_mov__msgs.o	\
			e_sht__msgs.o	\
			e_stat__msgs.o	\
			e_sys__msgs.o

OBJECTS =		agettxt.o	\
			buffers.o	\
			fmtmsg.o	\
			$(TEXTOBJS)

# see the comment below about the '+' signs in TEXTTARG
#
TEXTSRCS =		$(TEXTOBJS:%.o=%.c)
TEXTTARG =		$(TEXTSRCS:%=+ %)

TFILES =		msg.source gen-defs gen-text

TXTFILES=		$(TFILES)

LPINC =			../../include
OAMDEF_H =		$(LPINC)/oam_def.h

CLEANFILES =		$(TEXTSRCS) $(OAMDEF_H) xx??

include			../../../../lib/Makefile.lib

CPPFLAGS =		-I$(LPINC) $(CPPFLAGS.master)

.KEEP_STATE:

all install :		$(TXTFILES) $(LIBS)

_msg :
	@echo "Messages are made in usr/src/cmd/lp"

# derived source files
# and their dependencies
#
# beware of the hidden ordering requirement in the target source
# macros.  TEXTTARG contains '+' signs to tell make that this is a
# target group.  Without this syntax, an infinite build loop occurs.
#
$(OAMDEF_H) $(TEXTTARG) :$(TFILES)
			sh gen-defs > $(OAMDEF_H)
			sh gen-text

# dependencies slightly overstated but necessarily explicit
$(OBJS) \
$(PICS) \
$(PROFS) :		$(OAMDEF_H) $$(@F:.o=.c)

include			../../../../lib/Makefile.targ

CLEANFILES	+= llib-llpoam.ln
LINTFLAGS = -nvx
SRCS= $(OBJECTS:%.o=%.c)

lint:	lintlib
	$(LINT.c) $(LINTFLAGS) $(SRCS)

lintlib:
	$(LINT.c) $(LINTFLAGS) -o lpoam llib-llpoam
