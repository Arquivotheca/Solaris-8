#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#

#
#ident	"@(#)Makefile	1.4	96/04/18 SMI"        /* SVr4.0
#

PROG= touch

include ../Makefile.ucbcmd

FILEMODE=	755
CPPFLAGS += -D_FILE_OFFSET_BITS=64

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.ucbtarg
