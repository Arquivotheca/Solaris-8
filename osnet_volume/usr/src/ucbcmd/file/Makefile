#ident	"@(#)Makefile	1.1	91/05/04 SMI"
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
#

SHFILES= file
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
