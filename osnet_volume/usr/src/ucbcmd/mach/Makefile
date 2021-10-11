#ident	"@(#)Makefile	1.3	92/12/16 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

SHFILES= mach
CLOBBERFILES= $(SHFILES)

SHSRCS= $(SHFILES:%=%.sh)

include ../Makefile.ucbcmd

.KEEP_STATE:

all: $(SHFILES) 

%: %.sh
	$(CP) $< $@

# Don't re-install directories already installed by Targetdirs 
#install: all $(ROOTBIN) $(ROOTSHFILES)
install: all $(ROOTSHFILES)

clean:

lint:

include ../Makefile.ucbtarg

