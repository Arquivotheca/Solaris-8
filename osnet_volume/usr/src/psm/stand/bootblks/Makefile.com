#
#ident	"@(#)Makefile.com	1.5	97/11/12 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
#
# psm/stand/bootblks/Makefile.com
#
TOPDIR = ../../../$(BASEDIR)

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

PSMSYSHDRDIR	= $(PSMSTANDDIR)
PSMNAMELIBDIR	= $(PSMSTANDDIR)/lib/names/$(MACH)
PSMPROMLIBDIR	= $(PSMSTANDDIR)/lib/promif/$(MACH)

#
# 'bootblk' is the basic target we build - in many flavours
#
PROG		= bootblk

#
# Used to convert Forth source to isa-independent FCode.
#
TOKENIZE	= tokenize

#
# Common install modes and owners
#
FILEMODE	= 444
DIRMODE		= 755
OWNER		= root
GROUP		= sys

#
# Lint rules (adapted from Makefile.uts)
#
LHEAD		= ( $(ECHO) "\n$@";
LGREP		= grep -v "pointer cast may result in improper alignment"
LTAIL		= ) 2>&1 | $(LGREP)
LINT_DEFS	+= -Dlint

#
# For building lint objects
#
LINTFLAGS.c	= -nsxum
LINT.c		= $(LINT) $(LINTFLAGS.c) $(LINT_DEFS) $(CPPFLAGS) -c
LINT.s		= $(LINT) $(LINTFLAGS.s) $(LINT_DEFS) $(CPPFLAGS) -c

#
# For building lint libraries
#
LINTFLAGS.lib	= -nsxum
LINT.lib	= $(LINT) $(LINTFLAGS.lib) $(LINT_DEFS) $(CPPFLAGS)

#
# For complete pass 2 cross-checks
# XXX: lint flags should exclude -u, but the standalone libs confuse lint.
#
LINTFLAGS.2	= -nsxum
LINT.2		= $(LINT) $(LINTFLAGS.2) $(LINT_DEFS) $(CPPFLAGS)
