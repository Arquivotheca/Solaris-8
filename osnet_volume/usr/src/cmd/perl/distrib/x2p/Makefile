#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	99/08/16 SMI"
#
# cmd/perl/distrib/x2p/Makefile

include ../../../Makefile.cmd
include ../../Makefile.locns
 
CFLAGS += -DPERL_FOR_X2P
LIBS = -lsocket -lnsl -ldl -lm -lc -lcrypt_i
LLPATH= LD_LIBRARY_PATH=..

PL_SRCS = find2perl.PL s2p.PL
PL_TARGS = $(PL_SRCS:%.PL=%)

PODS = a2p.pod

HDRS = EXTERN.h INTERN.h ../config.h ../handy.h hash.h a2p.h str.h util.h
SRCS = hash.c str.c util.c walk.c a2p.c
OBJS = $(SRCS:%.c=%.o)

TARGS = a2p $(PL_TARGS)

CLOBBERFILES= $(TARGS)
CLEANFILES= $(OBJS)

all: $(TARGS)
	$(CP) $(PODS) ../pod

a2p: a2p.o $(OBJS)
	$(LINK.c) -o a2p a2p.o $(OBJS) $(LIBS)

# We don't want to regenerate a2p.c, but it might appear out-of-date
# after a patch is applied or a new distribution is made.
a2p.c: a2p.y
	-@sh -c true

a2p.o: a2p.c a2py.c a2p.h $(HDRS)

$(PL_TARGS):
	$(LLPATH) ../miniperl -I../lib $@.PL

clean:
	$(RM) -f $(CLEANFILES)

include ../../../Makefile.targ
