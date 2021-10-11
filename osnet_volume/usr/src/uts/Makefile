#
# Copyright (c) 1993-1997,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.93	99/10/04 SMI"
#
# uts/Makefile
#
# include global definitions
include ../Makefile.master

#
# List of architectures to build as part of the standard build.
#
# Note that sparc must trail the individual platforms so that sparc
# lint targets will have the unix lint libraries to compare against.
#
sparc_ARCHITECTURES = sun4u sun4m sun4d sparc

i386_ARCHITECTURES = i86pc
#$(BUILD64)i386_ARCHITECTURES += ia64plat
i386_ARCHITECTURES += intel

def		:=	TARGET= def
all		:=	TARGET= all
install		:=	TARGET= install
install_h	:=	TARGET= install_h
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint
clean.lint	:=	TARGET= clean.lint
check		:=	TARGET= check

.KEEP_STATE:

def all lint: all_h $($(MACH)_ARCHITECTURES)

install: all_h install_dirs $($(MACH)_ARCHITECTURES)

clean clobber clean.lint: $($(MACH)_ARCHITECTURES)

install_dirs:
	@cd ..; pwd; $(MAKE) rootdirs
	@pwd

$($(MACH)_ARCHITECTURES): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

#
# The following is the list of directories which contain Makefiles with
# targets to install header file. The machine independent headers are
# installed by invoking the Makefile in the directory containing the
# header files. Machine and architecture dependent headers are installed
# by invoking the main makefile for that architecture/machine which,
# in turn, is responsible for invoking the Makefiles which install headers.
# It is done this way so as not to assume that all of the header files in
# the architecture/machine dependent subdirectories are in completely
# isomorphic locations.
#
COMMON_HDRDIRS= common/des common/fs common/gssapi common/inet common/net \
	common/netinet common/nfs common/rpc common/sys common/vm \
	common/c2 common/pcmcia/sys common/rpcsvc common/inet/nca

sparc_HDRDIRS= sun/sys
i386_HDRDIRS= i86pc/vm
#ia64_HDRDIRS= ia64plat/vm
#$(BUILD64)i386_HDRDIRS += $(ia64_HDRDIRS)

HDRDIRS= $(COMMON_HDRDIRS) $($(MACH)_HDRDIRS)
install_h check: $(HDRDIRS) $($(MACH)_ARCHITECTURES)

$(HDRDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

# ensures that headers made by rpcgen are available in uts source
# for kernel builds to reference without building install_h
#
all_h: FRC
	@cd common/rpc; pwd; $(MAKE) $@
	@cd common/rpcsvc; pwd; $(MAKE) $@
	@cd common/gssapi; pwd; $(MAKE) $@

ONC_FILES=	common/io/timod.c \
		common/os/sig.c \
		common/os/flock.c \
		common/os/sysent.c \
		common/os/swapgeneric.c \
		common/syscall/fcntl.c 

# edit onc plus source files. 
ONC_PLUS:	$(ONC_FILES:%=%_onc_plus) 

#
# The cscope.out file is made in the current directory and spans all
# supported architectures.
# this target probably can't be tracked by Nse.
# Things to note:
#	1. We use relative names for cscope and tags.
#	2. We *don't* remove the old cscope.out file, because cscope is
#	   smart enough to only build what has changed.  It can be
#	   confused, however, if files are renamed or removed, so it may
#	   be necessary to manually remove cscope.out if a lot of
#	   reorganization has occured.
#
CSDIR	= .
CSDIRS = $(sparc_ARCHITECTURES) $(i386_ARCHITECTURES) \
	sfmmu srmmu sun common ../psm/promif
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

.PRECIOUS:	cscope.out

cscope.out: cscope.files FRC
	${CSCOPE} ${CSFLAGS}

cscope.files: FRC
	@-$(RM) cscope.files cscope.files.raw
	echo "$(CSINCS)" > cscope.files
	-find $(CSPATHS) -name SCCS -prune -o \
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
#	   sun, and those over those in common.
#
tags: tags.list FRC
	${CTAGS} ${CTFLAGS} `cat tags.list`

# THIS IS A KLUDGE
# We should really only list the files we use to build the kernel; this
# picks up things like mpsas directories which are not used in normal
# kernels.

tags.list: cscope.files
	@$(RM) tags.list
	grep '\.c$$' cscope.files | grep -v '/mpsas/' | grep -v '/sas/' \
		> tags.list
	grep '\.h$$' cscope.files | grep -v '/mpsas/' | grep -v '/sas/' \
		>> tags.list
	grep '\.s$$' cscope.files | grep -v '/mpsas/' | grep -v '/sas/' \
		>> tags.list

FRC:

