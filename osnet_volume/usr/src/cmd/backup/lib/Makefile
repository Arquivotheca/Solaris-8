#
# "@(#)Makefile	1.30	99/01/22 SMI"
#
# Copyright (c) 1993,1998 by Sun Microsystems, Inc.
#
# cmd/backup/lib/Makefile
#
LIBRARY=	libdump.a

# Has to be before include of Makefile.backup
# This should be POFILE=libdump.po, but that causes make to
# fall over due to some seriously weird interactions in the
# various indirectly-included makefiles.  So, since this works
# and is otherwise harmless, we fake it.
PROG=		libdump

# Include library definitions, then backup definitions, as in general
# we want the flags and such from our tree.

include ../../../lib/Makefile.lib
include ../Makefile.backup

HDRS=		../include/byteorder.h \
	../include/memutils.h ../include/myrcmd.h \
	../../../head/protocols/dumprestore.h \
	../include/rmt.h

YFILE=		getdate.y
YSRC=		getdate.c

CLOBBERFILES=	$(YSRC) $(GLIB) *.ln

LOBJS=		rmtlib.o myrcmd.o \
		$(YSRC:%.c=%.o) \
		byteorder.o memutils.o $(RPC_CLNT:%.c=%.o) $(RPC_XDR:%.c=%.o)

OBJECTS=	$(LOBJS)
POFILES=	$(OBJECTS:.o=.po)
GENERAL=	../include
GLOBAL=		../../../head
CPPFLAGS=	-I$(GENERAL) -I$(GLOBAL) $(CPPFLAGS.master)
lint:= \
	LINTFLAGS +=	-y
LINTOUT=	lint.out
CLEANFILES=	$(OBJECTS) $(LINTOUT) $(LINTLIB) $(DEBUGS) *.ln \
		$(YSRC) $(LIBRARY)
LINTFLAGS+=	-y

# support for -g library
GLIB=		libdump_g.a
DEBUGS=		$(OBJECTS:%=.debug/%)
$(GLIB):=	AROBJS = $(DEBUGS)
$(GLIB):=	DIR = .debug
$(GLIB):=	CFLAGS= -g -xs -DDEBUG -DYYDEBUG ${SBFLAGS}

.KEEP_STATE:

all:	$(LIBS)

debug:	$(LIBS) $(GLIB)

.debug:
	-@mkdir -p $@

.debug/%.o: %.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(GLIB): .debug $$(DEBUGS)
	$(BUILD.AR)
	$(POST_PROCESS_A)

$(OBJECTS):	$(HDRS)

install:	all

$(POFILE):	$(POFILES)
	$(RM) $@; cat $(POFILES) > $@

# rpcgen produces unused local variables that we can't easily suppress.
# It is also stupid about 32/64 bit integers.  Since we don't support
# the RPC subsystem any more, just ignore complaints about it all.
# We have no control over yaccpar, and it has lots of 32/64 complaints.
# Assumes lint run with -s argument
lint:		lint.out
	sed -f lint.sed lint.out

lint.out:	$(LINTLIB)

check:	$(HDRS)
	$(CSTYLE) $(CSTYLEFLAGS) `echo $(SRCS) | sed -e s/getdate.c//` $(HDRS)
	$(HDRCHK) $(HDRCHKFLAGS) $(HDRS)

# include library targets
include ../../../lib/Makefile.targ

_msg: 		$(POFILE)
