#
#ident	"@(#)Makefile	1.1	99/01/11 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#

SHFILES=	tokenize
BINARIES=	forth tokenize.exe

include ../Makefile.tools

OWNER=	root
GROUP=	bin

.KEEP_STATE:

all:	$(SHFILES) $(BINARIES)

install: all .WAIT $(ROOTONBLDMACHSHFILES) $(ROOTONBLDMACHBINARIES)

clean:
	$(RM) $(CLEANFILES)

include ../Makefile.targ
