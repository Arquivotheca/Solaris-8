#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#

#
#ident	"@(#)Makefile	1.5	96/04/18 SMI"
#

PROG= sed

OBJS= sed0.o sed1.o
SRCS= $(OBJS:%.o=%.c)

include ../Makefile.ucbcmd

CPPFLAGS += -D_FILE_OFFSET_BITS=64

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

clean: 
	$(RM) $(OBJS)

lint: lint_SRCS

include ../Makefile.ucbtarg
