#
# ident	"@(#)Makefile	1.5	99/07/12 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All Rights Reserved
#
# cmd/print/gateway/translators/cascade/Makefile
#

LIBRARY =	bsd_cascade.a
VERS=.1

OBJECTS =	cascade.o

include		../../../../../lib/Makefile.lib
include         ../../../Makefile.sp

ROOTLIBDIR=	$(ROOT)/usr/lib/print/bsd-adaptor

#CPPFLAGS +=	-I$(LPINC)
CPPFLAGS +=	-I$(NPRTINC)
LDLIBS +=	-lprint -lnsl -lsocket -ldl -lc		# with XFN API
#LDLIBS +=	$(LIBCLS) $(LIBPRT) $(LIBMSG)

LIBS = $(DYNLIB)	# could be += for static and dynamic
SRCS= $(OBJECTS:%.o=%.c)

CLOBBERFILES=	llib-lbsd_cascade.a.ln lint.out
 
.KEEP_STATE:
  
all :	$(LIBS)

install:	$(ROOTLIBDIR) $(ROOTLIBS) $(ROOTLINKS)

cstyle:
	cstyle $(SRCS)

lint:
	$(LINT.c) $(SRCS)
strip:

_msg:
	@echo "Messages are made in usr/src/cmd/print"

$(ROOTLIBDIR):
	$(INS.dir)

include		../../../../../lib/Makefile.targ
