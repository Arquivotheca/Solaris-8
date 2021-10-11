#
#ident	"@(#)Makefile	1.7	95/02/07 SMI"       /* SVr4.0 1.3.3.1 */
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/devinfo/Makefile
#

PROG= devinfo

include ../Makefile.cmd

LDLIBS += -ladm 
CFLAGS += -v

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTUSRSBINPROG)

clean:

lint:   lint_PROG

include ../Makefile.targ

