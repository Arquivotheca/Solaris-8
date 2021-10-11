#
#ident	"@(#)Makefile	1.2	98/10/14 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/devfseventd/Makefile
#

PROG= 	devfseventd
OBJS=	devfseventd.o

DEVFSADM = devfsadm
ROOTLIBDEVFSADMDIR = $(ROOTLIB)/$(DEVFSADM)
ROOTLIBDEVFSADMPROG = $(ROOTLIBDEVFSADMDIR)/$(PROG)

include ../Makefile.cmd

LDLIBS += -lthread -ldoor
CFLAGS += -D_REENTRANT -v
LINTFLAGS += -D_REENTRANT
OWNER= root
GROUP= sys
FILEMODE= 0555

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTLIBDEVFSADMDIR) $(ROOTLIBDEVFSADMPROG)

$(PROG):	$(OBJS)
	$(LINK.c) -o $(PROG) $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

$(ROOTLIBDEVFSADMDIR)/%: %
	$(INS.file)

$(ROOTLIBDEVFSADMDIR):
	$(INS.dir)

clean:
	$(RM) $(OBJS)

lint:	lint_PROG 

include ../Makefile.targ
