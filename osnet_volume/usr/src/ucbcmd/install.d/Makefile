#
#ident	"@(#)Makefile	1.8	00/09/14 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= installcmd

PROG2= install

PROGSRC= install.c

include ../Makefile.ucbcmd

CPPFLAGS += -D_FILE_OFFSET_BITS=64

FILEMODE=	755

# for messaging catalog
#
POFILE= install.po


.KEEP_STATE:

all: $(PROG) 


install: all $(ROOTPROG)
	rm -f $(ROOTBIN)/$(PROG2)
	mv $(ROOTBIN)/$(PROG) $(ROOTBIN)/$(PROG2)

clean:

lint:	lint_PROG

include ../Makefile.ucbtarg
