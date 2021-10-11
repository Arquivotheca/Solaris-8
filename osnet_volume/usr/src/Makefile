#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.93	00/10/14 SMI"
#
# Makefile for system source
#
# include global definitions
include Makefile.master
#
# the Targetdirs file is the AT&T target.dirs file in a makefile format.
# it defines TARGETDIRS and ROOTDIRS.
include Targetdirs

COMMON_SUBDIRS=	uts lib stand psm uts/adb cmd ucblib ucbcmd

SUBDIRS= $(COMMON_SUBDIRS) $($(MACH)_SUBDIRS)

HDRSUBDIRS=	uts head lib cmd ucbhead

# UCB headers are bug-for-bug compatible and not checkable against the header
# standards.
#
CHKHDRSUBDIRS=	head uts lib

MSGSUBDIRS=     cmd     ucbcmd     lib
DOMAINS= \
	SUNW_OST_ADMIN \
	SUNW_OST_NETRPC \
	SUNW_OST_OSCMD \
	SUNW_OST_OSLIB \
	SUNW_OST_UCBCMD

MSGDDIRS=       $(DOMAINS:%=$(MSGROOT)/%)
MSGDIRS=        $(MSGROOT) $(MSGDDIRS) $(MSGROOT)/LC_TIME

all all_xmod :=		TARGET= all
install install_xmod :=	TARGET= install
install_h :=		TARGET= install_h
clean :=		TARGET= clean
clobber :=		TARGET= clobber
check :=		TARGET= check
_msg :=			TARGET= _msg


.KEEP_STATE:

#
# Note: install only builds the all target for the pkgdefs
#       directory.  We are not yet ready to have an install
#	build create 'packages' also.  To build packages
#	cd pkgdefs and do a 'make install'
#
all: sgs .WAIT $(SUBDIRS) pkg_all
install: sgs .WAIT $(SUBDIRS) pkg_all .WAIT install_msg

# cannot be done in the source build, since some files are gon
install_msg: FRC
	@if [ -d SCCS ]; then \
		pwd; $(MAKE) _msg; \
		cd pkgdefs/SUNW0on; pwd; $(MAKE) _msg; \
		rm -rf "$(ROOT)/catalog"; \
	fi

clean clobber: $(SUBDIRS) head pkgdefs
_msg: _msgdirs rootdirs _msgheaders $(MSGSUBDIRS)

# for a complete build from scratch
crankturn: sgs uts pkg_all
	@cd lib; pwd; $(MAKE) install
	@cd stand; pwd; $(MAKE) all
	@cd psm; pwd; $(MAKE) all
	@cd uts/adb; pwd; $(MAKE) all
	@cd cmd; pwd; $(MAKE) all
	@cd ucblib; pwd; $(MAKE) install
	@cd ucbcmd; pwd; $(MAKE) all

pkg_all:
	@cd pkgdefs; pwd; $(MAKE) all

#
# target for building a proto area for reference via the ROOT macro
#
protolibs: rootlibs ucblibs

# build all ucb libraries
#
ucblibs:
	@cd ucblib; pwd; $(MAKE) install

# Base subset of rootproto, excluding ucb libraries
#
rootlibs: sgs
	@cd lib; pwd; $(MAKE) install

# create target-variant symlinks
links:
	@cd uts; pwd; $(MAKE) links
	@cd lib/libc; pwd; $(MAKE) links

$(SUBDIRS) head ucbhead pkgdefs: FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

.PARALLEL:	sysheaders userheaders libheaders ucbheaders cmdheaders

# librpcsvc has a dependency on headers installed by
# userheaders, hence the .WAIT before libheaders.
sgs: rootdirs .WAIT sysheaders userheaders .WAIT \
	libheaders ucbheaders cmdheaders

# /var/mail/:saved is a special case because of the colon in the name.
#
rootdirs: $(ROOTDIRS)
	$(INS) -d -m 775 $(ROOT)/var/mail/:saved
	$(CH)$(CHOWN) root $(ROOT)/var/mail/:saved
	$(CH)$(CHGRP) mail $(ROOT)/var/mail/:saved

#
# As pieces are made lint-clean, add them here so the nightly build
# can be used to keep them that way.
#
lint: FRC
	@cd uts; pwd; $(MAKE) lint
	@cd cmd/cmd-inet/sbin; pwd; $(MAKE) lint
	@cd cmd/coreadm; pwd; $(MAKE) lint
	@cd cmd/cpc; pwd; $(MAKE) lint
	@cd cmd/dumpadm; pwd; $(MAKE) lint
	@cd cmd/iostat; pwd; $(MAKE) lint
	@cd cmd/lofiadm; pwd; $(MAKE) lint
	@cd cmd/mdb; pwd; $(MAKE) lint
	@cd cmd/mpstat; pwd; $(MAKE) lint
	@cd cmd/halt; pwd; $(MAKE) lint
	@cd cmd/pgrep; pwd; $(MAKE) lint
	@cd cmd/prstat; pwd; $(MAKE) lint
	@cd cmd/prtconf; pwd; $(MAKE) lint
	@cd cmd/psradm; pwd; $(MAKE) lint
	@cd cmd/psrinfo; pwd; $(MAKE) lint
	@cd cmd/psrset; pwd; $(MAKE) lint
	@cd cmd/sort; pwd; $(MAKE) lint
	@cd cmd/uadmin; pwd; $(MAKE) lint
	@cd cmd/vmstat; pwd; $(MAKE) lint

_msgdirs:       $(MSGDIRS)

$(ROOTDIRS) $(MSGDIRS):
	$(INS.dir)

_msgheaders: userheaders sysheaders
	@cd lib/libintl; pwd; $(MAKE) install_h
	@cd lib/libtnf; pwd; $(MAKE) install_h
	@cd lib/libtnfprobe; pwd; $(MAKE) install_h

userheaders: FRC
	@cd head; pwd; $(MAKE) install_h

libheaders: FRC
	@cd lib; pwd; $(MAKE) install_h

sysheaders: FRC
	@cd uts; pwd; $(MAKE) install_h

ucbheaders: FRC
	@cd ucbhead; pwd; $(MAKE) install_h

cmdheaders: FRC
	@cd cmd; pwd; $(MAKE) install_h

# each xmod target depends on a corresponding MACH-specific pseudotarget
# before doing common xmod work
#
all_xmod install_xmod: $$@_$(MACH)
	@cd uts/common/sys; pwd; $(MAKE) svvs_h

all_xmod_sparc install_xmod_sparc: FRC
	@cd uts/sparc; pwd; \
	  $(MAKE) TARGET=$(TARGET) svvs pm wsdrv
	@cd uts/sun4m; pwd; $(MAKE) TARGET=$(TARGET) cpr pmc

all_xmod_i386 install_xmod_i386: FRC
	@cd uts/i86; pwd; $(MAKE) TARGET=$(TARGET) svvs

check:	$(CHKHDRSUBDIRS)

#
# The cscope.out file is made in the current directory and spans the
# entire source tree.
#
# Things to note:
#	1. We use relative names for cscope.
#	2. We *don't* remove the old cscope.out file, because cscope is
#	   smart enough to only build what has changed.  It can be
#	   confused, however, if files are renamed or removed, so it may
#	   be necessary to manually remove cscope.out if a lot of
#	   reorganization has occured.
#
CSDIR	= .
CSDIRS	= cmd common lib head uts psm stand ucbcmd ucbhead ucblib
CSPATHS	= $(CSDIRS:%=$(CSDIR)/%)
CSINCS	= $(CSPATHS:%=-I%)
CSCOPE	= cscope

#
# Set CSFLAGS env variable to -bq when using fast cscope to
# build the fast (but large) cscope data bases.
#
CSFLAGS	= -b

.PRECIOUS:	cscope.out

cscope.out: cscope.files FRC
	${CSCOPE} ${CSFLAGS}

cscope.files: FRC
	@-$(RM) cscope.files cscope.files.raw
	echo "$(CSINCS)" > cscope.files
	-find $(CSPATHS) -name SCCS -prune -o \
	    -type d -name '.del-*' -prune -o -type f \
	    \( -name '*.[Ccshlxy]' -o -name 'Makefile*' -o -name '*.il' \
	    -o -name '*.cc' -o -name '*.adb' \) \
	    -print > cscope.files.raw
	grep -v Makefile cscope.files.raw >> cscope.files
	grep Makefile cscope.files.raw >> cscope.files
	-$(RM) cscope.files.raw
	@wc -l cscope.files
cscope.out:

FRC:


ONC_PLUS:
	@cd cmd/login; pwd; $(MAKE) ONC_PLUS
	@cd uts; pwd; $(MAKE) ONC_PLUS
