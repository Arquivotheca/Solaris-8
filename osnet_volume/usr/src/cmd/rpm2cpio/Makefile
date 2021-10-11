#
#ident	"@(#)Makefile	1.1	99/10/08 SMI"

SCRIPTS= rpm2cpio

include ../Makefile.cmd

CLOBBERFILES += $(SCRIPTS)
ROOTSCRIPTS=  $(SCRIPTS:%=$(ROOTBIN)/%)

.KEEP_STATE:

all: $(SCRIPTS)

install: all $(ROOTSCRIPTS)

clean:

$(SCRIPTS):	$(SCRIPTS).pl
	$(RM) $(SCRIPTS)
	cat $(SCRIPTS).pl > $(SCRIPTS)
	chmod +x $(SCRIPTS)

include ../Makefile.targ
