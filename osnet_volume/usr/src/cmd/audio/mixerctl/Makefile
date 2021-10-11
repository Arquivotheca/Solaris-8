#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/05/26 SMI"
#
# Makefile for cmd/audio/mixerctl

PROG= mixerctl

include ../../Makefile.cmd

CFLAGS += -v

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTUSRSBINPROG)

clean:

lint:	lint_PROG

include ../../Makefile.targ
