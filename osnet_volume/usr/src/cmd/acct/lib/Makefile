#
#ident	"@(#)Makefile	1.5	94/12/31 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
#
LIBS=	a.a

OBJS=	pnpsplit.o  copyn.o devtolin.o expand.o lintodev.o \
	namtouid.o tmless.o tmsecs.o uidtonam.o substr.o 
SRCS=	$(OBJS:%.o=%.c)

include	../../Makefile.cmd

CPPFLAGS = -D_LTYPES -I.. $(CPPFLAGS.master)

.KEEP_STATE:

.PARALLEL: $(OBJS)

all: $(LIBS)

$(LIBS): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

clean:
	$(RM) $(OBJS)

clobber: clean
	$(RM) $(LIBS)

lint:
	$(LINT.c) $(SRCS)

install:

.PRECIOUS: $(LIBS)
