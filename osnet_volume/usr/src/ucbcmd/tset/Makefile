#
#ident	"@(#)Makefile	1.7	00/09/14 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= tset

include ../Makefile.ucbcmd

FILEMODE=	755

#LDLIBS +=  -lcurses
LDLIBS =  -L$(ROOT)/usr/ucblib $(LDLIBS.cmd) -lucb -ltermcap

CPPFLAGS = -I$(ROOT)/usr/ucbinclude $(CPPFLAGS.master)

ROOTSYMLINK= $(ROOTBIN)/reset

.KEEP_STATE:

all: $(PROG) 


install: all $(ROOTBIN) $(ROOTPROG) $(ROOTSYMLINK)

$(ROOTSYMLINK):
	$(RM) $@
	$(SYMLINK) $(PROG) $@

clean:

lint:	lint_PROG

include ../Makefile.ucbtarg
