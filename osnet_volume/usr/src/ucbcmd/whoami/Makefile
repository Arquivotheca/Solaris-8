#
#ident	"@(#)Makefile	1.4	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= whoami

include ../Makefile.ucbcmd

CFLAGS += -v
 
XGETFLAGS += -a -x rmdir.xcl


FILEMODE=	755

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.ucbtarg
