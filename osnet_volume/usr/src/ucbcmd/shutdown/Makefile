#
#ident	"@(#)Makefile	1.11	97/03/18 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG= shutdown

PROGSRC= shutdown.c
PROGOBJS= shutdown.o

include ../Makefile.ucbcmd

INC1 = $(ROOT)/usr/ucbinclude

FILEMODE=	755

LDLIBS +=  -l socket -l rpcsvc -l nsl -L$(ROOT)/usr/ucblib -l ucb -l elf -lbsm

#CPPFLAGS.master += -I$(INC1)
CPPFLAGS = -I$(INC1) $(CPPFLAGS.master)

XGETFLAGS += -a -x shutdown.xcl

.KEEP_STATE:

all: $(PROG) 

$(PROG):	$(PROGOBJS)
		$(SETRUNPATH) $(LINK.c) $(PROGOBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

install: all $(ROOTPROG)

clean:
	$(RM) $(PROGOBJS)

lint:	lint_PROG

include ../Makefile.ucbtarg
