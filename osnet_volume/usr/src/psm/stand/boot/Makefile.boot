#
#pragma ident	"@(#)Makefile.boot	1.10	99/05/04 SMI"
#
# Copyright (c) 1994-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/boot/Makefile.boot

#
# Hack until stand makefiles are fixed
#
CLASS	= 32

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/Makefile.psm

STANDDIR	= $(TOPDIR)/stand
PSMSTANDDIR	= $(TOPDIR)/psm/stand

SYSHDRDIR	= $(STANDDIR)
SYSLIBDIR	= $(STANDDIR)/lib/$(MACH)
SYSLIBDIR64	= $(STANDDIR)/lib/$(MACH64)

PSMSYSHDRDIR	= $(PSMSTANDDIR)
PSMNAMELIBDIR	= $(PSMSTANDDIR)/lib/names/$(MACH)
PSMNAMELIBDIR64	= $(PSMSTANDDIR)/lib/names/$(MACH64)
PSMPROMLIBDIR	= $(PSMSTANDDIR)/lib/promif/$(MACH)
PSMPROMLIBDIR64	= $(PSMSTANDDIR)/lib/promif/$(MACH64)

#
# XXX	one day we should just be able to set PROG to 'cfsboot'..
#	and everything will become a lot easier.
#
# XXX	note that we build but -don't- install the HSFS boot
#	program - it's unused and untested, and until it is we
#	shouldn't ship it!
#
UNIBOOT		= boot.bin
UFSBOOT		= ufsboot
NFSBOOT		= inetboot
HSFSBOOT	= hsfsboot

#
# Common install modes and owners
#
FILEMODE	= 644
DIRMODE		= 755
OWNER		= root
GROUP		= sys

#
# Install locations
#
ROOT_PSM_UNIBOOT= $(ROOT_BOOT_SOL_DIR)/$(UNIBOOT)
ROOT_PSM_UFSBOOT= $(ROOT_PSM_DIR)/$(UFSBOOT)
USR_PSM_NFSBOOT	= $(USR_PSM_LIB_NFS_DIR)/$(NFSBOOT)
USR_PSM_HSFSBOOT= $(USR_PSM_LIB_HSFS_DIR)/$(HSFSBOOT)

#
# Lint rules (adapted from Makefile.uts)
#
LHEAD		= ( $(ECHO) "\n$@";
LGREP		= grep -v "pointer cast may result in improper alignment"
LTAIL		= ) 2>&1 | $(LGREP)

LINT.c		= $(LINT) $(LINTFLAGS) $(LINT_DEFS) $(CPPFLAGS) -c
LINT.s		= $(LINT.c)
LINT.2		= $(LINT) $(LINTFLAGS.2) $(LINT_DEFS) $(CPPFLAGS)

LINTFLAGS	= -nsxmu
LINTFLAGS64	+= -nsxmu
LINTFLAGS.2	= -nsxm
