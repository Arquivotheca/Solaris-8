#ident	"@(#)Makefile	1.3	94/10/28 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

SHFILES= ld
CLOBBERFILES= $(SHFILES)

SHSRCS= $(SHFILES:%=%.sh)

include ../Makefile.ucbcmd

.KEEP_STATE:

all: $(SHFILES) 

%: %.sh
	$(RM) $@
	$(CP) $< $@

install: all $(ROOTBIN) $(ROOTSHFILES)

clean:

lint:

include ../Makefile.ucbtarg

