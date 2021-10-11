#
#ident	"@(#)Makefile	1.6	96/04/18 SMI"
#
# Copyright (c) 1989, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= bdiff 

include ../Makefile.cmd

FILEMODE=	755

CPPFLAGS +=-D_FILE_OFFSET_BITS=64

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
