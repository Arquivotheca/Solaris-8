#
#ident	"@(#)Makefile	1.1	98/07/14 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libdevfsevent/Makefile
#
#
LIBRARY=	libdevfsevent.a
VERS=		.1
OBJECTS=	libdevfsevent.o

# include library definitions
include ../Makefile.lib

MAPFILE=	mapfile-vers
ROOTLIBDIR=	$(ROOT)/usr/lib
LIBS=		$(DYNLIB)
DYNFLAGS	+= -M $(MAPFILE)
LDLIBS +=	-ldoor -lc

# definitions for install_h target
HDRS=		libdevfsevent.h
ROOTHDRDIR=	$(ROOT)/usr/include
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)

$(ROOTHDRS) := FILEMODE = 644

CPPFLAGS +=	-v -D_REENTRANT
LINTOUT=	lint.out
CLEANFILES=	$(LINTOUT) $(LINTLIB)

$(DYNLIB): $(MAPFILE)

.KEEP_STATE:

all: 	$(LIBS)

# install rule for install_h target
$(ROOTHDRDIR)/%: %
	$(INS.file)

install: all $(ROOTLIBS) $(ROOTLINKS)

install_h: $(ROOTHDRS)

check: $(CHECKHDRS)

lint:		$(LINTLIB)

# include library targets
include ../Makefile.targ
