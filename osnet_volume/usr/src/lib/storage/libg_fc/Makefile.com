#
#pragma ident	"@(#)Makefile.com	1.10	99/11/22 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libg_fc/Makefile.com
#

LIBRARY=	libg_fc.a
VERS=		.2

OBJECTS=	cmd.o \
		genf.o \
		hot.o \
		io.o \
		map.o \
		errormsgs.o

GENMSG_PROG=	$(LIBRARY).msg
GENMSG_SRCS=	$(OBJECTS:%.o=../common/%.c)

# include library definitions
include $(SRC)/lib/storage/Makefile.storage

# definitions for install_h target
HDRS=           gfc.h

ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)

# install rule for install_h target
$(ROOTHDRDIR)/%: ../common/hdrs/%
	$(INS.file)

install_h: $(ROOTHDRDIR)

check: $(CHECKHDRS)


.KEEP_STATE:

MAPDIR=		$(SRC)/lib/storage/libg_fc/common
MAPFILE=	$(MAPDIR)/mapfile

LIBS =	$(DYNLIB)

LDLIBS += -lc

CPPFLAGS +=	-I$(SRC)/lib/storage/libg_fc/common/hdrs
DYNFLAGS=	-h $(DYNLIB) -ztext -M $(MAPFILE)

LINTOUT=	lint.out
CLEANFILES=	$(LINTLIB) $(GENMSG_PROG)

