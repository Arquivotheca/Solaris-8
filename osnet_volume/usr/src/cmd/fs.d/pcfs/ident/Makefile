# Copyright 1992, Sun Microsystems
#
# Makefile for PCFS ident function.
#
# "@(#)Makefile	1.4	94/11/14 SMI"
#


IDENT_VERS=1
FSTYPE=		pcfs
LIBPROG=	ident_pcfs.so.${IDENT_VERS} 

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

LINTFLAGS=	-avx

.KEEP_STATE:

all: ${LIBPROG}

lint:
	$(LINT.c) ${CPPLAGS} ${SRCS}

cstyle:
	${CSTYLE} ${SRCS}

clean:     
	${RM} ${IDENTS} core a.out

clobber: clean
