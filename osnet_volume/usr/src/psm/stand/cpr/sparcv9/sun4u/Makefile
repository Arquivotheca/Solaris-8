#
#ident	"@(#)Makefile	1.15	99/10/19 SMI"
#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/cpr/sparc/sun4u/Makefile
#

TOPDIR =	../../../../..

# override global macros as necessary
TARGET_ARCH =	sun4u
TARG_MACH =     sparcv9
ARCH =		sun4u
MACH =		sparc
MMU =		sfmmu
ARCHVER =	v9
PROMTYPE =	ieee1275
ARCH_PROMDIR =	$(TARG_MACH)/$(PROMTYPE)/common
PROMIFDIR =	$(TOPDIR)/psm/stand/lib/promif
PLATLIBDIR =	$(PROMIFDIR)/$(TARG_MACH)/$(PROMTYPE)/$(TARGET_ARCH)
PLATLIB =	$(PLATLIBDIR)/libplat.a
PLATLDLIBS =	-L$(PLATLIBDIR) -lplat
PROMLIB =	libprom.a
CPRBOOTLINT =	cprboot_lint

CPRBOOT_MAPFILE = mapfile

ARCHOPTS = -D__sparcv9cpu

CPRBOOTOBJ = cb_srt0.o cprboot.o machdep.o pages.o bitmap.o util.o

ALL = cprboot

include ../../common/Makefile.com
include $(TOPDIR)/psm/Makefile.psm.64
CFLAGS64 += -xchip=ultra $(CCABS32)


.KEEP_STATE:

STANDDIR= $(TOPDIR)/stand
CSDIRS	=	. ../../common \
		$(STANDDIR)/lib $(STANDDIR)/sys \
		../../../../promif/ieee1275 \
		$(TOPDIR)/uts/common/os/compress.c \
		$(TOPDIR)/uts/common/sys/cpr.h \
		$(TOPDIR)/uts/sun4u/sys/cpr_impl.h

CPPINCS	+= -I${STANDDIR}

CSINCS	= $(CSPATHS:%=-I%)
CSCOPE	= cscope
CTAGS	= ctags

cscope.out: cscope.files FRC
	${CSCOPE} -b -f `pwd`/$@

cscope.files:   FRC
	@$(RM) -f cscope.files
	$(ECHO) $(CSINCS) > cscope.files
	find $(CSDIRS) -name SCCS -prune -o \
	    \( -type d -name '.del*' \) -prune -o -type f \
	    \( -name '*.[chs]' -o -name 'Makefile*' -o -name '*.il' \) \
	    -a ! -name '.del*' -print >> cscope.files
	@$(WC) -l cscope.files

#
# Create a tags data base, similar to above.
# Since assembler files now contain C fragments for lint,
# the lint fragments will allow ctags to "work" on assembler.
#
# Things to note:
#	1. We put plain files before headers, and .c before .s (because
#	   ctags doesn't understand assembly comments).
#	2. We *don't* sort the output of find, as we want functions in
#	   architecture directories to take precedence over those in
#	   sun, and those over those in common.
#
tags: cscope.files
	$(CTAGS) -wt `sed 1d cscope.files`
