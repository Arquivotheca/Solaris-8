#
#ident  "@(#)Makefile 1.2     99/10/04 SMI"
#
# Copyright (c) 1995, by Sun Microsystems, Inc.
#
# psm/stand/kadb/sparcv9/Makefile
#

SUBDIRS = sun4u

TOPDIR	= ../../../..

all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint
clean.lint	:=	TARGET= clean.lint

.KEEP_STATE:

all install lint clean.lint: $(SUBDIRS)

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

CSDIRS	= $(TOPDIR)/cmd/adb/sparcv9 $(TOPDIR)/cmd/adb/common \
	$(TOPDIR)/stand/lib/common \
	$(TOPDIR)/stand/lib/fs \
	$(TOPDIR)/stand/lib/sparcv9 \
	$(TOPDIR)/stand/sys \
	$(TOPDIR)/psm/promif $(SUBDIRS) common ../common

CSPATHS	= $(CSDIRS:%=$(CSDIR)/%)

CSENVS	= $(ENVCPPFLAGS1) $(ENVCPPFLAGS2) $(ENVCPPFLAGS3) $(ENVCPPFLAGS4)

CSINCS	= $(CSPATHS:%=-I%) \
	  $(SUBDIRS:%=-I$(TOPDIR)/uts/%) \
	  -I$(TOPDIR)/uts/srmmu \
	  -I$(TOPDIR)/uts/sfmmu \
	  -I$(TOPDIR)/uts/sparc \
	  -I$(TOPDIR)/uts/sparc/v7 \
	  -I$(TOPDIR)/uts/sparc/v9 \
	  -I$(TOPDIR)/uts/sun \
	  -I$(TOPDIR)/uts/common \
	  -I$(ROOT)/usr/include \
	  $(CSENVS:%$(MACH)/usr/include=%$(CSMACH)/usr/include)

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
	    -a ! -name '.del*' -print | grep -v '/prep/' >> cscope.files
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

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
