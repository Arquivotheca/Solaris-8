#
#ident	"@(#)Makefile.com	1.25	99/10/19 SMI"
#
# Copyright (c) 1992, 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/prtdiag/Makefile.com
#

#
#	Create default so empty rules don't
#	confuse make
#
CLASS		= 32

include $(SRCDIR)/../Makefile.cmd
include $(SRCDIR)/../../Makefile.psm

PROG		= prtdiag

FILEMODE	= 2755
DIRMODE		= 755
OWNER		= root
GROUP		= sys

OBJS=	main.o

# allow additional kernel-architecture dependent objects to be specified.

OBJS		+= $(KARCHOBJS)

SRCS		= $(OBJS:%.o=%.c)

LINT_OBJS	= $(OBJS:%.o=%.ln)


POFILE		= prtdiag_$(PLATFORM).po
POFILES		= $(OBJS:%.o=%.po)


# These names describe the layout on the target machine

IFLAGS = -I$(SRCDIR) -I$(USR_PSM_INCL_DIR) -I./

CPPFLAGS = $(IFLAGS) $(CPPFLAGS.master) -D_SYSCALL32

.PARALLEL: $(OBJS)

# build rules

%.o: $(SRCDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

%.po: $(SRCDIR)/%.c
	$(COMPILE.cpp) $<  > $<.i
	$(BUILD.po)

%.ln: $(SRCDIR)/%.c
	$(LINT) -u -c $(CPPFLAGS) $<
