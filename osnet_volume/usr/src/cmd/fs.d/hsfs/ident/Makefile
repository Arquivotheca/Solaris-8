#
# Copyright 1996, Sun Microsystems
# All rights reserved.
#
# Makefile for HSFS ident function.
#
#ident	"@(#)Makefile	1.4	96/04/17 SMI"
#
# cmd/fs.d/hsfs/ident/Makefile
#

IDENT_VERS=1
FSTYPE=		hsfs
LIBPROG=	ident_hsfs.so.${IDENT_VERS} 

include ../../Makefile.fstype
#
# Rules for making shared objects out of .c files.  Works well if
# we have a one-to-one mapping.  Applies in all cases so far.
#
.SUFFIXES: .so.${IDENT_VERS} ${SUFFIXES}

.c.so.${IDENT_VERS}:
	${CC} ${CPPFLAGS} ${CFLAGS} ${COPTFLAG} ${LDFLAGS} -G -K pic \
		-o $@ $< ${LIBS}
	$(POST_PROCESS_SO)

SRCS=	${LIBPROG:%.so.$(IDENT_VERS)=%.c}

CPPFLAGS += 	-DIDENT_VERS=${IDENT_VERS}

LDFLAGS +=	-zdefs

LIBS=		-lc

.KEEP_STATE:

all: ${LIBPROG}

lint:
	$(LINT.c) ${CFLAGS} ${SRCS}

cstyle:
	${CSTYLE} ${SRCS}

clean:     
	${RM} ${IDENTS} core a.out

clobber: clean

