#
#ident	"@(#)Makefile	1.20	96/07/31 SMI"
#
# Copyright (c) 1989-93 by Sun Microsystems, Inc.
#
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#
# cmd/localedef/chrtbl/Makefile

include ../../Makefile.cmd

PROG=	chrtbl

SRCS=	chrtbl.c

# remove references to NPROG

ROOTLINK=	$(ROOTBIN)/wchrtbl

CLOBBERFILES=	$(NATIVEPROG)


NATIVEDIR =		native
NATIVEPROG =		$(NATIVEDIR)/$(PROG)
$(NATIVEPROG) :=	CC=$(NATIVECC)
$(NATIVEPROG) :=	CPPFLAGS=
$(NATIVEPROG) :=	LDLIBS=

.KEEP_STATE:
.NO_PARALLEL:

all: $(PROG)

install: all $(ROOTPROG) $(ROOTLINK)

$(ROOTLINK): $(ROOTPROG)
	$(RM) $@; $(LN) $(ROOTPROG) $@

$(NATIVEPROG) :		$(NATIVEDIR) $(SRCS)
			$(LINK.c) -o $@ $(SRCS) $(LDLIBS)

$(NATIVEDIR) :
			-@mkdir -p $@

lint: lint_PROG

clean:

include ../../Makefile.targ
