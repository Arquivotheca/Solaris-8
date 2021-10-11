#
#pragma ident	"@(#)Makefile	1.35	99/10/04 SMI"
#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/kadb/Makefile
#

# All the real work gets done in kernel-architecture directories,
# and that's where the targets wind up. This makefile just gets us there.

include	../../../Makefile.master

sparcv7_ARCHITECTURES = sparc
sparcv9_ARCHITECTURES = sparcv9
sparc_ARCHITECTURES = $(sparcv9_ARCHITECTURES) $(sparcv7_ARCHITECTURES)
i386_ARCHITECTURES = i386
ia64_ARCHITECTURES = ia64
$(BUILD64)i386_ARCHITECTURES += $(ia64_ARCHITECTURES)

SUBDIRS = $($(MACH)_ARCHITECTURES) 

TOPDIR= ../../..

all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint
clean.lint	:=	TARGET= clean

.KEEP_STATE:

all install lint clean.lint: all_h $(SUBDIRS)

#
# Since the subdirs only clobber the platform specific macros, clobber the
# platform independent ones here.
#
clean clobber: $(SUBDIRS)
	@cd $(TOPDIR)/uts/adb/$(MACH); pwd; $(MAKE) $(TARGET)

#
# The cscope.out file is made in the current directory and spans
# all architectures.
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
CSDIR	= .

CSDIRS	= $(TOPDIR)/cmd/adb/sparc $(TOPDIR)/cmd/adb/common \
	$(TOPDIR)/stand/lib $(TOPDIR)/stand/sys \
	$(TOPDIR)/psm/promif $(TOPDIR)/uts/intel/ia32/promif \
	$(TOPDIR)/uts/intel/promif \
	common sparc i386 ia64

CSPATHS	= $(CSDIRS:%=$(CSDIR)/%)

CSENVS	= $(ENVCPPFLAGS1) $(ENVCPPFLAGS2) $(ENVCPPFLAGS3) $(ENVCPPFLAGS4)

CSINCS	= $(CSPATHS:%=-I%) \
	  -I$(TOPDIR)/uts/sun4d \
	  -I$(TOPDIR)/uts/sun4m \
	  -I$(TOPDIR)/uts/sun4u \
	  -I$(TOPDIR)/uts/sparc \
	  -I$(TOPDIR)/uts/sparc/v7 \
	  -I$(TOPDIR)/uts/sparc/v9 \
	  -I$(TOPDIR)/uts/sun \
	  -I$(TOPDIR)/uts/srmmu \
	  -I$(TOPDIR)/uts/i86pc \
	  -I$(TOPDIR)/uts/ia64plat \
	  -I$(TOPDIR)/uts/intel \
	  -I$(TOPDIR)/uts/common \
	  -I$(ROOT)/usr/include \
	  $(CSENVS)

CSCOPE	= cscope
CSFLAGS	= -b
CTAGS	= ctags

cscope.out: cscope.files FRC
	${CSCOPE} ${CSFLAGS}

cscope.files:   FRC
	@-$(RM) cscope.files
	echo $(CSINCS) > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
	    \( -type d -name '.del*' \) -prune -o -type f \
	    \( -name '*.[chs]' -o -name 'Makefile*' -o -name '*.il' \) \
	    -a ! -name '.del*' -print >> cscope.files
	@wc -l cscope.files

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

# ensures that headers made by rpcgen are available in uts source for
# kadb builds to reference without having to run make install_h in uts
#
all_h:	FRC
	@cd ../../../uts/common/rpc; pwd; $(MAKE) $@
	@cd ../../../uts/common/rpcsvc; pwd; $(MAKE) $@

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
