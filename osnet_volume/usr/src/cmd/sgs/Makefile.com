#
#pragma ident	"@(#)Makefile.com	1.15	99/06/23 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/Makefile.com

.KEEP_STATE:

SRCBASE=	../../../..

i386_ARCH=	intel
sparc_ARCH=	sparc

ARCH=$($(MACH)_ARCH)

ROOTCCSBIN64=		$(ROOTCCSBIN)/$(MACH64)
ROOTCCSBINPROG64=	$(PROG:%=$(ROOTCCSBIN64)/%)

# Establish the local tools, proto and package area.

SGSHOME=	$(SRC)/cmd/sgs
SGSPROTO=	$(SGSHOME)/proto/$(MACH)
SGSTOOLS=	$(SGSHOME)/tools
SGSMSGID=	$(SGSHOME)/messages
SGSMSGDIR=	$(SGSHOME)/messages/$(MACH)
SGSONLD=	$(ROOT)/opt/SUNWonld
SGSRPATH=	/usr/lib
SGSRPATH64=	$(SGSRPATH)/$(MACH64)


# The cmd/Makefile.com and lib/Makefile.com define TEXT_DOMAIN.  We don't need
# this definition as the sgs utilities obtain their domain via sgsmsg(1l).

DTEXTDOM=


# Define any generic sgsmsg(1l) flags.  The default message generation system
# is to use gettext(3i), add the -C flag to switch to catgets(3c).

SGSMSG=		$(SGSTOOLS)/$(MACH)/sgsmsg
CHKMSG=		$(SGSTOOLS)/chkmsg.sh

SGSMSGFLAGS=	-i $(SGSMSGID)/sgs.ident
CHKMSGFLAGS=	$(SGSMSGTARG:%=-m %) $(SGSMSGCHK:%=-m %)


# Native targets should use the minimum of ld(1) flags to allow building on
# previous releases.  We use mapfiles to scope, but don't bother versioning.

native:=	DYNFLAGS   = $(MAPOPTS) -R$(SGSPROTO) -L$(SGSPROTO) -znoversion


.KEEP_STATE_FILE: .make.state.$(MACH)

#
# lint-related stuff
#

DASHES=		"------------------------------------------------------------"

LIBNAME32 =	$(LIBNAME:%=%32)
LIBNAME64 =	$(LIBNAME:%=%64)
LIBNAMES =	$(LIBNAME32) $(LIBNAME64)

SGSLINTOUT =	lint.out
LINTOUT1 =	lint.out.1
LINTOUT32 =	lint.out.32
LINTOUT64 =	lint.out.64
LINTOUTS =	$(SGSLINTOUT) $(LINTOUT1) $(LINTOUT32) $(LINTOUT64)

LINTLIBSRC =	$(LINTLIB:%.ln=%)
LINTLIB32 =	$(LINTLIB:%.ln=%32.ln)
LINTLIB64 =	$(LINTLIB:%.ln=%64.ln)
LINTLIBS =	$(LINTLIB32) $(LINTLIB64)

#
# These libraries have two resulting lint libraries.
# If a dependency is declared using these variables,
# the substitution for the 32/64 versions at lint time
# will happen automatically (see Makefile.targ).
#
LDDBG_LIB=	-llddbg
LDDBG_LIB32=	-llddbg32
LDDBG_LIB64=	-llddbg64

LD_LIB=		-lld
LD_LIB32=	-lld32
LD_LIB64=	-lld64
