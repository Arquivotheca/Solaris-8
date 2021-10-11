#ident	"@(#)Makefile	1.4	92/12/16 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

SHFILES= cc
CLOBBERFILES= $(SHFILES)

SHSRCS= $(SHFILES:%=%.sh)

include ../Makefile.ucbcmd

.KEEP_STATE:

all: $(SHFILES) 

install: all $(ROOTBIN) $(ROOTSHFILES)

clean:

lint:

include ../Makefile.ucbtarg

