#ident	"@(#)Makefile	1.4	92/12/16 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

SHFILES= lintcmd

SHFILES2= lint

CLOBBERFILES= $(SHFILES) $(SHFILES2)

SHSRCS= $(SHFILES2:%=%.sh)

include ../Makefile.ucbcmd

.KEEP_STATE:

all: $(SHFILES) 

$(SHFILES):	$(SHSRCS)
	$(RM) $@
	$(CP) $(SHSRCS) $@

install: all $(ROOTBIN) $(ROOTSHFILES)
	rm -f $(ROOTBIN)/$(SHFILES2)
	mv $(ROOTBIN)/$(SHFILES) $(ROOTBIN)/$(SHFILES2)

clean:

lint:

include ../Makefile.ucbtarg

