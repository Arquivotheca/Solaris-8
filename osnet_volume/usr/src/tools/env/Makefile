#
#ident	"@(#)Makefile	1.1	99/01/11 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#

ENVFILES= \
	developer \
	gatekeeper

include ../Makefile.tools

OWNER=	root
GROUP=	bin
FILEMODE= 644

.KEEP_STATE:

all:	$(ENVFILES)

install: all .WAIT $(ROOTONBLDENVFILES)

clean:
	$(RM) $(CLEANFILES)

include ../Makefile.targ

