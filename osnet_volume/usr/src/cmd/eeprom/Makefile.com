#
#ident	"@(#)Makefile.com	1.13	99/03/30 SMI"
#
# Copyright (c) 1993,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/eeprom/Makefile.com
#

#
#	Create default so empty rules don't
#	confuse make
#
CLASS		= 32

include $(SRCDIR)/../Makefile.cmd
include $(SRCDIR)/../../Makefile.psm

PROG		= eeprom

FILEMODE	= 02555
DIRMODE		= 755
GROUP		= sys

#
# Sparc program implementation supports openprom machines.  identical versions
# are installed in /usr/platform for each machine type
# because (at this point in time) we have no guarantee that a common version
# will be available for all potential sparc machines (eg: ICL, solbourne ,...).
#
# The identical binary is installed several times (rather than linking them
# together) because they will be in separate packages.
#
# Now that it should be obvious that little (if anything) was gained from
# this `fix-impl' implementation style, maybe somebody will unroll this in
# distinct, small and simpler versions for each PROM type.
#
IMPL = $(PLATFORM:sun%=sun)

prep_OBJS = openprom.o loadlogo.o
sun_OBJS = openprom.o loadlogo.o
i86pc_OBJS = benv.o benv_kvm.o benv_sync.o
OBJS = error.o
OBJS += $($(IMPL)_OBJS)
LINT_OBJS = $(OBJS:%.o=%.ln)

prep_SOURCES = openprom.c loadlogo.c
sun_SOURCES = openprom.c loadlogo.c
i86pc_SOURCES = benv.c benv_kvm.c benv_syn.c
SOURCES	= error.c
SOURCES	+= $($(IMPL)_SOURCES)

.PARALLEL: $(OBJS)

%.o:	../common/%.c
	$(COMPILE.c) -o $@ $<

%.o:	$(SRCDIR)/common/%.c
	$(COMPILE.c) -o $@ $<

%.ln:	../common/%.c
	$(LINT.c) -c $@ $<

%.ln:	$(SRCDIR)/common/%.c
	$(LINT.c) -c $@ $<
