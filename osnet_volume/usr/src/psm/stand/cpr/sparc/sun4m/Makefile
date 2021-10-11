#
#ident	"@(#)Makefile	1.16	99/10/19 SMI"
#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/cpr/sparc/sun4m/Makefile
#

TOPDIR=../../../../..

# override global macros as necessary
TARGET_ARCH=	sun4m
ARCH=		sun4m
MACH=		sparc
MMU=		srmmu
ARCHVER =	v7
ARCH_PROMDIR=	sparc/obp/common
PROMTYPE=	obp
PROMLIB=	libprom.a
BOOTLIB=	libboot.a
PLATLIB=
PLATLIBDIR=
PLATLDLIBS=
BOOTLIBDIR=	$(TOPDIR)/psm/stand/lib/boot/$(MACH)/$(ARCH)
BOOTLIB	=	$(BOOTLIBDIR)/libboot.a
BOOTLDLIBS =	-L$(BOOTLIBDIR) -lboot
LDLIBS =	$(BOOTLDLIBS)
CPRBOOTLINT =	cprboot_lint

CPRBOOT_MAPFILE= mapfile
MAP_FLAG=	 -e start -Bstatic

CPRBOOTOBJ=	locore.o cpr.o cpr_sun4m.o module.o fixmmu_sun4m.o \
		module_asm.o sadiv.o util.o samuldiv64.o

ALL = cprboot

include ../../common/Makefile.com

.KEEP_STATE:

STANDDIR= $(TOPDIR)/stand
CSDIR	= .
CSDIRS	=	$(STANDDIR)/lib $(STANDDIR)/sys ../../../../promif \
		$(TOPDIR)/uts/common/os/compress.c \
		$(TOPDIR)/uts/common/sys/cpr.h \
		$(TOPDIR)/uts/sun4m/sys/cpr_impl.h \
		$(TOPDIR)/psm/stand/cpr/common \
		.

CSPATHS	= $(CSDIRS:%=$(CSDIR)/%)
CSINCS	= $(CSPATHS:%=-I%)
CSCOPE	= cscope
CTAGS	= ctags

cscope.out: cscope.files FRC
	${CSCOPE} -b -f `pwd`/$@

cscope.files:   FRC
	@$(RM) -f cscope.files
	$(ECHO) $(CSINCS) > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
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
