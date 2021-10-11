#
#ident	"@(#)Makefile.com	1.3	98/10/16 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.

PROG=		ar
XPG4PROG=	ar

include		$(SRC)/cmd/Makefile.cmd
include		$(SRC)/cmd/sgs/Makefile.com

COMOBJS=	main.o		file.o		cmd.o		global.o \
		message.o	sbfocus_enter.o

POFILE=		../ar.po

OBJS=		$(COMOBJS:%=objs/%)
XPG4OBJS=	$(COMOBJS:%=objs.xpg4/%)

LLDFLAGS =	'-R$$ORIGIN/../../lib'
CPPFLAGS=	-I../../include -DBROWSER $(CPPFLAGS.master)
CFLAGS +=	-v
LDLIBS +=	-lelf
LINTFLAGS=	-mx
LINTFLAGS64=	-mx -errchk=longptr64

SED=		sed

$(XPG4) :=	CPPFLAGS += -DXPG4

SRCS=		$(COMOBJS:%.o=../common/%.c)
LINTSRCS=	$(SRCS)

CLEANFILES +=	$(OBJS) $(XPG4OBJS) $(LINTOUTS)


# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6

package :=	LDLIBS += /usr/lib/libintl.so.1
