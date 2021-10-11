#
#ident	"@(#)Makefile	1.5	97/01/29 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# ucbcmd/ls/Makefile

PROG= ls

include ../Makefile.ucbcmd

CFLAGS += -v
CPPFLAGS += -D_FILE_OFFSET_BITS=64

FILEMODE=	755

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.ucbtarg
