#
# ident	"@(#)Makefile	1.10	99/08/16 SMI"
#
# Copyright (c) 1996-1998 by Sun Microsystems, Inc.
# All Rights Reserved
#
# cmd/lp/cmd/adaptor/Makefile
#

LIBRARY =	bsd_lpsched.a
VERS=.1

OBJECTS =	show_queue.o cancel_job.o submit_job.o misc.o

include		../../../../lib/Makefile.lib
include         ../../Makefile.lp

ROOTLIBDIR=	$(ROOT)/usr/lib/print/bsd-adaptor

CPPFLAGS +=	-I$(LPINC)
CPPFLAGS +=	-I$(SRC)/lib
LDLIBS +=	-lprint -lnsl -lsocket -ldl -lc
LDLIBS	+=	-L$(SRC)/cmd/lp/lib/msgs -llpmsg
LDLIBS	+=	-L$(SRC)/cmd/lp/lib/printers -llpprt
LDLIBS	+=	-L$(SRC)/cmd/lp/lib/class -llpcls
LDLIBS	+=	-L$(SRC)/cmd/lp/lib/lp -llp
LDLIBS	+=	-L$(SRC)/cmd/lp/lib/requests -llpreq
LDLIBS	+=	-L$(SRC)/cmd/lp/lib/secure -llpsec


LIBS = $(DYNLIB)	# could be += for static and dynamic
SRCS= $(OBJECTS:%.o=%.c)
 
.KEEP_STATE:
  
all:	$(LIBS)

_msg: 
	@echo "Messages are made in usr/src/cmd/lp"

install:	$(ROOTLIBS) $(ROOTLINKS)

cstyle:
	cstyle $(SRCS)

lint:
	$(LINT.c) $(SRCS)

strip:

include		../../../../lib/Makefile.targ
