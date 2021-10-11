#
#ident	"@(#)Makefile	1.6	92/12/15 SMI"        /* SVr4.0 1.2  */
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= diffmk
POFILE= diffmk.po

include ../Makefile.cmd

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(PROG).sh
	$(RM) $@
	sed -e "s/TEXT_DOMAIN/${TEXT_DOMAIN}/g" $(PROG).sh > $@
	chmod +x $@
	
#
# For message catalogue file.
#	
$(POFILE):
	grep gettext $(PROG).sh | tr '`' ' ' | sed -e "s/TEXT_DOMAIN/\(/" | sed -e "s/$$/);/"  > $(PROG).i
	$(XGETTEXT) $(PROG).i
	$(RM) $@ $(PROG).i
	mv messages.po diffmk.po

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
