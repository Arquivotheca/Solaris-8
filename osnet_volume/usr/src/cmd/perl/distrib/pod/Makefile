#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	99/08/16 SMI"
#
# cmd/perl/distrib/pod/Makefile

include ../../../Makefile.cmd
include ../../Makefile.locns
 
LLPATH= LD_LIBRARY_PATH=..

PL_SRCS = \
	checkpods.PL \
	pod2html.PL \
	pod2latex.PL \
	pod2man.PL \
	pod2text.PL

PL_TARGS = $(PL_SRCS:%.PL=%)

TARGS = $(PL_TARGS)
CLOBBERFILES = $(TARGS)

all: $(TARGS)

$(PL_TARGS):
	$(LLPATH) ../miniperl -I../lib $@.PL

$(PL_SRCS): ../lib/Config.pm

clean:

include ../../../Makefile.targ
