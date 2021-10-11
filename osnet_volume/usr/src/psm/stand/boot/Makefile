#
#pragma ident	"@(#)Makefile	1.23	99/10/04 SMI"
#
# Copyright (c) 1991-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/boot/Makefile
#

include		../../../Makefile.master

sparcv7_ARCHITECTURES = sparc
sparcv9_ARCHITECTURES = sparcv9
sparc_ARCHITECTURES = $(sparcv9_ARCHITECTURES) $(sparcv7_ARCHITECTURES)
i386_ARCHITECTURES = i386
ia64_ARCHITECTURES = ia64
$(BUILD64)i386_ARCHITECTURES += $(ia64_ARCHITECTURES)

SUBDIRS = $($(MACH)_ARCHITECTURES)

all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint

.KEEP_STATE:

all install lint clean: $(SUBDIRS)

clobber: $(SUBDIRS)
	$(RM) make.out lint.out

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

#
# The cscope.out file is made in the current directory and spans all
# architectures and boot fs types.
# Things to note:
#	1. We use relative names for cscope and tags.
#	2. We *don't* remove the old cscope.out file, because cscope is
#	   smart enough to only build what has changed.  It can be
#	   confused, however, if files are renamed or removed, so it may
#	   be necessary to manually remove cscope.out if a lot of
#	   reorganization has occured.
#	3. We deliberately avoid names that include '.del' in their names
#	   as these are usually files that have been 'deleted' by nsefile.
#
UTSDIR=		../../../uts
STANDLIBDIR=	../../../stand/lib
STANDSYSDIR=	../../../stand/sys ../../../stand/i386/sys
PROMDIRS=	../../promif $(UTSDIR)/i86/promif
NAMESDIRS=	../lib/names
CSSUBDIRS=	common sparcv9 sparc i386
CSDIR	= .
CSDIRS	= $(STANDLIBDIR) $(STANDSYSDIR) $(PROMDIRS) $(CSSUBDIRS) $(NAMESDIRS)
CSPATHS	= $(CSDIRS:%=$(CSDIR)/%)
CSINCS	= $(CSPATHS:%=-I%) \
	  -I$(UTSDIR)/sun4u \
	  -I$(UTSDIR)/sfmmu \
	  -I$(UTSDIR)/sparc/v7 \
 	  -I$(UTSDIR)/sparc/v9 \
	  -I$(UTSDIR)/sparc \
 	  -I$(UTSDIR)/sun \
 	  -I$(UTSDIR)/common \
 	  -I$(UTSDIR)/intel \
 	  -I$(UTSDIR)/i86pc \
 	  -I$(ROOT)/usr/include \
 	  $(ENVCPPFLAGS1) $(ENVCPPFLAGS2) $(ENVCPPFLAGS3) $(ENVCPPFLAGS4)
CSCOPE	= cscope
CSFLAGS= -b
CTAGS	= ctags

cscope.out: cscope.files FRC
	${CSCOPE} ${CSFLAGS}

cscope.files:   FRC
	@-$(RM) cscope.files
	echo $(CSINCS) > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
	    \( -type d -name '.del*' \) -prune -o -type f \
	    \( -name '*.[chs]' -o -name 'Makefile*' -o \
	    -name 'mapfile*' -o -name '*.il' \) \
	    -a ! -name '.del*' -print >> cscope.files
	@wc -l cscope.files

#
# Create a tags data base, similar to above.
# Since many assembler files now contain C fragments for lint,
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
	${CTAGS} -wt `sed 1d cscope.files`

FRC:
