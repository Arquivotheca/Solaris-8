#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	99/08/16 SMI"
#
# cmd/perl/distrib/utils/Makefile

include ../../../Makefile.cmd
include ../../Makefile.locns
 
LLPATH= LD_LIBRARY_PATH=..

PL_SRCS = \
	c2ph.PL \
	h2ph.PL \
	h2xs.PL \
	perlbug.PL \
	perlcc.PL \
	perldoc.PL \
	pl2pm.PL \
	splain.PL

PL_TARGS = $(PL_SRCS:%.PL=%)

TARGS = $(PL_TARGS)
CLOBBERFILES = $(TARGS) pstruct

all: $(TARGS)

$(PL_TARGS):
	$(LLPATH) ../miniperl -I../lib $@.PL

$(PL_TARGS): ../config.sh

perlbug: ../patchlevel.h

splain: ../lib/diagnostics.pm

clean:

include ../../../Makefile.targ
