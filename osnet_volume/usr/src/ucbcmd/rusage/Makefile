#
#ident	"@(#)Makefile	1.7	96/09/10 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= rusage

include ../Makefile.ucbcmd

INC1 = $(ROOT)/usr/ucbinclude

FILEMODE=	755

LDLIBS +=  -L$(ROOT)/usr/ucblib -lucb

#CPPFLAGS.master += -I$(INC1)
CPPFLAGS = -I$(INC1) $(CPPFLAGS.master)

.KEEP_STATE:

all: $(PROG)

$(PROG):	$(PROG).c
	$(SETRUNPATH) $(LINK.c) -o $@ $(PROG).c $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.ucbtarg

