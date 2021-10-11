#
#ident	"@(#)Makefile	1.5	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= stty

PROGSRC= stty.c sttyparse.c sttytable.c

include ../Makefile.ucbcmd

FILEMODE=	755

.KEEP_STATE:

all: $(PROG) 

$(PROG):	$(PROGSRC)
		$(LINK.c) $(PROGSRC) -o $@ $(LDLIBS)
		$(POST_PROCESS)

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.ucbtarg
