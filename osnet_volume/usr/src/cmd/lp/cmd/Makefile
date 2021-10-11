#
# ident	"@(#)Makefile	1.28	99/08/16 SMI"
#
# Copyright (c) 1994,1996,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/lp/cmd/Makefile
#

include 	../Makefile.lp

OWNER = root

SUBDIRS =	lpc lptest lpadmin lpsched lpstat adaptor

LOCALPROG1 =	lp lpsystem lpmove
LOCALPROG2 =	accept
LOCALPROG =	$(LOCALPROG1) $(LOCALPROG2)
LOCALLINKS =	disable enable reject

SBINPROG =	lpfilter lpforms lpshut lpusers 
LIBLINKS =	$(SBINPROG)

PROG =		$(LOCALPROG) $(SBINPROG)

COMMONOBJ=	comb.o
OTHEROBJS=	cancel.o lp.o $(SBINPROG:=.o)

OBJS=		$(COMMONOBJ) $(OTHEROBJS)

SRCS=          $(OBJS:.o=.c)

ROOTLIBLPLOCLPROG=	$(LOCALPROG:%=$(ROOTLIBLPLOCL)/%)
ROOTSBINPROG=	$(SBINPROG:%=$(ROOTUSRSBIN)/%)
ROOTSYMLINKS=	$(LIBLINKS:%=$(ROOTLIB)/%)
ROOTSYMLINKS2=	$(LOCALLINKS:%=$(ROOTLIBLPLOCL)/%)


CPPFLAGS =	-I$(LPINC) $(CPPFLAGS.master)

# conditional assignments
#
accept:=	LDLIBS += $(LIBMSG) $(LIBACC) $(LIBOAM) $(LIBLP)
cancel:=	LDLIBS += $(LIBREQ) $(LIBMSG) $(LIBOAM) $(LIBLP)
lp:=		LDLIBS += $(LIBPRT) $(LIBREQ) $(LIBMSG) $(LIBOAM) $(LIBLP)
lpfilter:=	LDLIBS += $(LIBFLT) $(LIBMSG) $(LIBACC) $(LIBOAM) $(LIBLP) \
			-lgen
lpforms:=	LDLIBS += $(LIBFRM) $(LIBMSG) $(LIBREQ) $(LIBOAM) \
		$(LIBACC) $(LIBLP)
lpmove:=	LDLIBS += $(LIBMSG) $(LIBACC) $(LIBOAM) $(LIBLP)
lpshut:=	LDLIBS += $(LIBMSG) $(LIBOAM) $(LIBLP)
lpsystem:=	LDLIBS += $(LIBSYS) $(LIBMSG) $(LIBOAM) $(LIBLP) -lnsl
lpusers:=	LDLIBS += $(LIBMSG) $(LIBACC) $(LIBOAM) $(LIBUSR) $(LIBLP)

.KEEP_STATE:

all:		$(PROG) $(SUBDIRS)

accept:		$(COMMONOBJ)
		$(LINK.c) -o $@ $(COMMONOBJ) $(LDFLAGS) $(LDLIBS)
		$(POST_PROCESS)

install:	$(PROG) $(ROOTLIBLPLOCLPROG) $(ROOTSBINPROG) \
		$(ROOTSYMLINKS) $(ROOTSYMLINKS2) $(SUBDIRS)

_msg:
		@echo "Messages are made in usr/src/cmd/lp"

clean:		$(SUBDIRS)
		$(RM) $(OBJS)

clobber:	$(SUBDIRS) local_clobber

local_clobber:
		$(RM) $(OBJS) $(PROG) lpmove.o lpsystem.o

strip:		$(SUBDIRS)
		$(STRIP) $(PROG)

lint:
		$(LINT.c) $(SRCS) $(LDLIBS)

$(LOCALPROG1) $(SBINPROG): $$@.o
		$(LINK.c) -o $@ $@.o $(LDFLAGS) $(LDLIBS)
		$(POST_PROCESS)

$(ROOTSYMLINKS):
		$(RM) $@; $(SYMLINK) ../sbin/$(@F) $@

$(ROOTSYMLINKS2):
		$(RM) $@; $(SYMLINK) ./accept $@

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
