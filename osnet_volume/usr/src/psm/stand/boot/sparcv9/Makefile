#
#ident	"@(#)Makefile	1.3	99/06/06 SMI"
#
# Copyright (c) 1997, 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/boot/sparcv9/Makefile
#

include ../../../../Makefile.master

# firmware libraries, platform-group, and platform-specific
# subdirectories to visit.

SUBDIRS	= sun4u

TARG_MACH = sparcv9

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

all install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

#
# The cscope.out file is made in the current directory and spans all
# sparc platforms and boot fs types.
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
UTSDIR=		../../../../uts
STANDLIBDIR=	../../../../stand/lib
STANDSYSDIR=	../../../../stand/sys
PROMDIRS=	../../../promif/ieee1275
NAMESDIRS=	../../lib/names/sparcv9 ../../lib/names/sparc/common
CSMACH=		sparcv9
CDIRS=		../common ../sparc/common
CSSUBDIRS=	$(SUBDIRS)
CSDIR=		.
CSDIRS=		\
		$(CDIRS) \
		$(CSSUBDIRS) \
		$(NAMESDIRS) \
		$(PROMDIRS) \
		$(STANDSYSDIR) \
		$(STANDLIBDIR)/$(CSMACH) \
		$(STANDLIBDIR)/common \
		$(STANDLIBDIR)/fs

CSPATHS=	$(CSDIRS:%=$(CSDIR)/%)
CSENVS=		$(ENVCPPFLAGS1) $(ENVCPPFLAGS2) $(ENVCPPFLAGS3) $(ENVCPPFLAGS4)
CSINCS=		$(CSPATHS:%=-I%) -I$(ROOT:%$(MACH)=%$(CSMACH))/usr/include \
		$(CSENVS:%$(MACH)/usr/include=%$(CSMACH)/usr/include)
CSCOPE=		cscope
CSFLAGS=	-b
CTAGS=		ctags

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
