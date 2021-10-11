#
# uts/i86pc/Makefile
# Copyright (c) 1991-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.22	99/07/26 SMI"
#
#	This makefile drives the production of all implementation architecture
#	dependent modules for the i86pc architecture.
#

UTSBASE = ..

include Makefile.i86pc

#
#	The following are ia32 specific (rather than i86pc) specific modules
#	which are required for the i86pc kernel to completely lint. They are
#	not involved in the build in any other way. In order to minimize
#	build time, it is assumed that they are up to date.
#
IA32_LIB_DIR	 = $(UTSBASE)/intel/ia32/lint-libs/$(OBJS_DIR)

IA32_LINTS	 = genunix

KMODS_BUTFOR_MMU36	= $(KMODS:%mmu36=)
KMODS_BUTFOR_MMU32	= $(KMODS:%mmu32=)
LINT_LIBS_MMU32	 = $(LINT_LIB) \
		   $(KMODS_BUTFOR_MMU36:%=$(LINT_LIB_DIR)/llib-l%.ln) \
		   $(IA32_LINTS:%=$(IA32_LIB_DIR)/llib-l%.ln)

LINT_LIBS_MMU36	 = $(LINT_LIB) \
		   $(KMODS_BUTFOR_MMU32:%=$(LINT_LIB_DIR)/llib-l%.ln) \
		   $(IA32_LINTS:%=$(IA32_LIB_DIR)/llib-l%.ln)

#
#
#
def		:=	TARGET= def
all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint
lintlib		:=	TARGET= lintlib
machmodlintlib	:=	TARGET= modlintlib
clean.lint	:=	TARGET= clean.lint
check		:=	TARGET= check

.KEEP_STATE:

.PARALLEL:	$(KMODS) $(XMODS)

def all clean clobber clean.lint: genassym unix .WAIT $(KMODS) $(XMODS)

install: install_platforms genassym unix .WAIT $(KMODS) $(XMODS)

lintlib:	unix

modlintlib:	$(KMODS)

genassym unix $(KMODS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(XMODS):	FRC
	@if [ -f $@/Makefile  ]; then \
		cd $@; pwd; $(MAKE) $(TARGET); \
	else \
		true; \
	fi

install_h check:	FRC
	@cd sys; pwd; $(MAKE) $(TARGET)

#
# For some ia32 OEMs that deliver their own platform kernel modules but
# will not deliver anything in usr/platform, they should add their platforms 
# here so a symlink will be created to the standard usr/platform/i86pc.
# 
OEM_LINKS	= ncri86pc

OEM_USR_PLAT_LINKS	= $(OEM_LINKS:%=$(USR_PLAT_DIR)/%)

$(OEM_USR_PLAT_LINKS):
	$(INS.slink1)

#
# Definitions for the /platform directory aliases.
# Currently none for i86pc.
#
PLAT_LINKS	=

#
# Make the /platform directories.  This is hardwired here because
# the first stage of the project (KBI) only implements the userland
# changes, but the only reasonable place to record the aliases is
# here in kernel land.
#
install_platforms:	$(ROOT_PSM_DIR) $(USR_PSM_DIR) \
			$(ROOT_PLAT_LINKS) $(USR_PLAT_LINKS) \
			$(OEM_USR_PLAT_LINKS)

#
#	Full kernel lint target.
#
LINT_TARGET	= globallint

# workaround for multiply defined errors
globallint := LINTFLAGS += -erroff=E_NAME_MULTIPLY_DEF2

globallint:
	@-$(ECHO) "\nFULL KERNEL: global crosschecks with mmu32:"
	@-$(LINT) $(LINTFLAGS) $(LINT_LIBS_MMU32) 2>&1 | $(LGREP.2)
	@-$(ECHO) "\nFULL KERNEL: global crosschecks with mmu36:"
	@-$(LINT) $(LINTFLAGS) $(LINT_LIBS_MMU36) 2>&1 | $(LGREP.2)

lint:	lintlib .WAIT modlintlib .WAIT $(IA32_LINTS) $(LINT_DEPS)

$(IA32_LINTS):	FRC
	@cd $(UTSBASE)/intel/ia32/$@; pwd; $(MAKE) modlintlib

#
# The cscope.out file is made in the current directory and spans only
# its architecture.
# Things to note:
#	1. We use relative names for cscope and tags.
#	2. We *don't* remove the old cscope.out file, because cscope is
#	   smart enough to only build what has changed.  It can be
#	   confused, however, if files are renamed or removed, so it may
#	   be necessary to manually remove cscope.out if a lot of
#	   reorganization has occured.
#
CSDIR	= ..
CSDIRS	= i86pc intel common
CSPATHS	= $(CSDIRS:%=$(CSDIR)/%)
CSINCS	= $(CSPATHS:%=-I%)
CSCOPE	= cscope
CTAGS	= ctags
#
# Set CSFLAGS env variable to -bq when using fast cscope to
# build the fast (but large) cscope data bases.
#
CSFLAGS	= -b
CTFLAGS = -wt

.PRECIOUS: cscope.out

cscope.out: cscope.files FRC
	${CSCOPE} ${CSFLAGS}

cscope.files: FRC
	@-$(RM) cscope.files cscope.files.raw
	echo "$(CSINCS)" > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
	    -type d -name '.del-*' -prune -o -type f \
	    \( -name '*.[csh]' -o -name 'Makefile*' -o -name '*.il' \) \
	    -print > cscope.files.raw
	grep -v Makefile cscope.files.raw >> cscope.files
	grep Makefile cscope.files.raw >> cscope.files
	-$(RM) cscope.files.raw
	@wc -l cscope.files

#
# Create a tags data base, similar to above.
# Since assembler files now contain C fragments for lint,
# the lint fragments will allow ctags to "work" on assembler.
# Things to note:
#	1. We put plain files before headers, and .c before .s (because
#	   ctags doesn't understand assembly comments).
#	2. We *don't* sort the output of find, as we want functions in
#	   architecture directories to take precedence over those in
#	   common.
#
tags: tags.list FRC
	${CTAGS} ${CTFLAGS} `cat tags.list`

# THIS IS A KLUDGE
# We should really only list the files we use to build the kernel; this
# picks up things like mpsas directories which are not used in normal
# kernels.

tags.list: cscope.files
	@-$(RM) tags.list
	grep '\.c$$' cscope.files | grep -v '/mpsas/' | grep -v '/sas/' \
		> tags.list
	grep '\.h$$' cscope.files | grep -v '/mpsas/' | grep -v '/sas/' \
		>> tags.list
	grep '\.s$$' cscope.files | grep -v '/mpsas/' | grep -v '/sas/' \
		>> tags.list

FRC:

include ../Makefile.targ
