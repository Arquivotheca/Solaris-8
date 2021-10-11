#
#ident	"@(#)Makefile	1.1	99/01/11 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#

GKFILES= \
	cshrc \
	login \
	make.machines


include ../Makefile.tools

OWNER=		root
GROUP=		bin
FILEMODE=	644

.KEEP_STATE:

all:	$(GKFILES)

install: all .WAIT $(ROOTONBLDGKFILES)

clean:
	$(RM) $(CLEANFILES)

include ../Makefile.targ
