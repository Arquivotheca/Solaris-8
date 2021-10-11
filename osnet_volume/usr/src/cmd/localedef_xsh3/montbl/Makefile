#
#ident	"@(#)Makefile	1.15	96/07/26 SMI"
#
# Copyright (c) 1990-93 by Sun Microsystems, Inc.
#
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#
# cmd/localedef/montbl/Makefile

include ../../Makefile.cmd

PROG=	montbl

SRCS=	montbl.c

CLOBBERFILES=	$(NATIVEPROG)

NATIVEDIR =		native
NATIVEPROG =		$(NATIVEDIR)/$(PROG)
$(NATIVEPROG) :=	CC=$(NATIVECC)
$(NATIVEPROG) :=	CPPFLAGS=
$(NATIVEPROG) :=	LDLIBS=

.KEEP_STATE:
.NO_PARALLEL:

all: $(PROG)

install: all $(ROOTPROG)

$(NATIVEPROG) :		$(NATIVEDIR) $(SRCS)
			$(LINK.c) -o $@ $(SRCS) $(LDLIBS)

$(NATIVEDIR) :
			-@mkdir -p $@

lint: lint_PROG

clean:

include ../../Makefile.targ
