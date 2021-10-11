# Copyright 1992, Sun Microsystems
#
# Makefile for UFS ident function.
#
# "@(#)Makefile	1.2	94/12/22 SMI"
#


IDENT_VERS=1
FSTYPE=		ufs
LIBPROG=	ident_ufs.so.${IDENT_VERS} 

include ../../Makefile.fstype
#
# Rules for making shared objects out of .c files.  Works well if
# we have a one-to-one mapping.  Applies in all cases so far.
#
.SUFFIXES: .so.${IDENT_VERS} ${SUFFIXES}

.c.so.${IDENT_VERS}:
	${CC} ${CPPFLAGS} ${CFLAGS} -G -o $@ $<
	$(POST_PROCESS_SO)

SRCS=	${LIBPROG:%.so.$(IDENT_VERS)=%.c}

CPPFLAGS += 	-DIDENT_VERS=${IDENT_VERS}

.KEEP_STATE:

all: ${LIBPROG}

lint:
	$(LINT.c) ${CFLAGS} ${SRCS}

cstyle:
	${CSTYLE} ${SRCS}

clean:     
	${RM} ${IDENTS} core a.out

clobber: clean

