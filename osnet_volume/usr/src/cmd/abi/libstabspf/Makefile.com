#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#
#pragma ident	"@(#)Makefile.com	1.1	99/05/14 SMI"
#

SRCS=		$(OBJECTS:%.o=../common/%.c)

MAPFILE=	$(MAPDIR)/mapfile

LINTFLAGS +=	-sF -errtags=yes
LINTFLAGS64 +=	-Xarch=v9 -sF -errtags=yes -erroff=E_BAD_PTR_CAST_ALIGN
CLEANFILES +=	$(LINTOUT)
CLOBBERFILES += $(LINTLIB) $(MAPFILE)

CFLAGS +=	-v
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS=		-lelf -lmapmalloc -lc

pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_SO)

all:	$(DYNLIB)

$(DYNLIB): $(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

lint:	$(LINTLIB)

include	$(SRC)/lib/Makefile.targ

FILEMODE=	755
ROOTABIDIR=	$(ROOTLIBDIR)/abi
ROOTABIDIR64=	$(ROOTABIDIR)/$(MACH64)
ROOTABILIB=	$(ROOTABIDIR)/$(DYNLIB)
ROOTABILIB64=	$(ROOTABIDIR64)/$(DYNLIB)

$(ROOTABILIB): $(DYNLIB) $(ROOTABIDIR)/$(LIBLINKS)
	$(INS.file) $(DYNLIB)

$(ROOTABILIB64): $(DYNLIB) $(ROOTABIDIR64)/$(LIBLINKS)
	$(INS.file) $(DYNLIB)

$(ROOTABIDIR64)/$(LIBLINKS) $(ROOTABIDIR)/$(LIBLINKS): $(DYNLIB)
	-$(RM) $@; $(SYMLINK) $(DYNLIB) $@


#
# Special targets for testing.
# Links directly with objects instead of library.
#

PROG=		typespec
PROGOBJS=	main.o	\
		scan.o


CLEANFILES +=	$(PROGOBJS) ../common/scan.c $(LIBLINKS)
CLOBBERFILES += $(PROG) 


#
# If we build typespec like this we can test against itself.
#
$(PROG) := COPTFLAG = -g -xs
$(PROG) := COPTFLAG64 = -g -xs
$(PROG) := LDLIBS += -ll

PROGSRCS=	$(PROGOBJS:%.o=../common/%.c)

test:	$(DYNLIB) .WAIT $(PROG)

$(PROG): $(PROGOBJS)
	$(LINK.c) -o $@ $(PROGOBJS) $(PICS) $(LDLIBS)

%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
