#ident	"@(#)Makefile	1.2	92/12/16 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

SHFILES= fasthalt
CLOBBERFILES= $(SHFILES)

SHSRCS= $(SHFILES:%=%.sh)

include ../Makefile.ucbcmd

.KEEP_STATE:

all: $(SHFILES) 

%: %.sh
	$(CP) $< $@

install: all $(ROOTBIN) $(ROOTSHFILES)

clean:

lint:

include ../Makefile.ucbtarg

