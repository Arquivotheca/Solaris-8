#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/07/18 SMI"
#

MAPDIR= ../spec/ia64

include ../Makefile.com
include ../../../../Makefile.lib.64

ROOTLIBDIR=	$(ROOT)$(KERBLIBDIR64)
ROOTLIBS=	$(LIBS:%=$(ROOTLIBDIR)/%)
ROOTLINKS=	$(ROOTLIBDIR)/$(LIBLINKS)

FILEMODE=	755

LDLIBS += -L $(ROOTLIBDIR)
LDLIBS += -R $(KRUNPATH64) $(KMECHLIB64) -lgss -lnsl -ldl -lsocket \
	-lc -lxfn

.KEEP_STATE:

all: $(LIBS)

$(ROOTLIBDIR):
	$(INS.dir)

$(ROOTLINKS):
	$(INS.liblink)

$(ROOTLIBDIR)/%: %
	$(INS.file)

install: all $(ROOTLIBDIR) $(ROOTLIBS) $(ROOTLINKS)
