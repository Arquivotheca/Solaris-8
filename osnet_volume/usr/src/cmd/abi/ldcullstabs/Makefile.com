#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.1	99/05/14 SMI"
#
# cmd/abi/ldcullstabs/Makefile.com
#

SRCS=		$(OBJECTS:%.o=../common/%.c)

LINTFLAGS +=	-sF -errtags=yes
LINTFLAGS64 +=	-Xarch=v9 -sF -errtags=yes
CLEANFILES +=	$(LINTOUT)
CLOBBERFILES += $(LINTLIB)

LDLIBS +=	-lelf -lc
CFLAGS +=	-v

pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_SO)

all:	$(DYNLIB)

lint:	$(LINTLIB)

include	$(SRC)/lib/Makefile.targ

FILEMODE=	755
ROOTABIDIR=	$(ROOTLIBDIR)/abi
ROOTABIDIR64=	$(ROOTABIDIR)/$(MACH64)
ROOTABILIB=	$(ROOTABIDIR)/$(DYNLIB)
ROOTABILIB64=	$(ROOTABIDIR64)/$(DYNLIB)

$(ROOTABILIB): $(DYNLIB)
	$(INS.file) $(DYNLIB)

$(ROOTABILIB64): $(DYNLIB)
	$(INS.file) $(DYNLIB)
