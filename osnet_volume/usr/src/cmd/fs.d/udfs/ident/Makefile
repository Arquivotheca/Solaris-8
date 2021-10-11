#
#pragma ident	"@(#)Makefile	1.5	99/02/13 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#


IDENT_VERS=1
FSTYPE=		udfs
LIBPROG=	ident_udfs.so.${IDENT_VERS} 

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

# for messaging catalog
#
POFILE= ident.po

# for messaging catalog
#
catalog:	$(POFILE)

$(POFILE):	$(SRCS)
	$(RM) $@
	$(COMPILE.cpp) $(SRCS) > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS) $(POFILE).i
	sed "/^domain/d" messages.po > $@

lint:
	$(LINT.c) -u ${SRCS}

putback:
	-cstyle -p ident_udfs.c
	-keywords ident_udfs.c
	-keywords Makefile

cstyle:
	${CSTYLE} ${SRCS}

clean:     
	${RM} ${IDENTS} core a.out

clobber: clean

