#
#pragma ident   "@(#)Makefile 1.10     98/11/10 SMI"
#
# Copyright (c) 1994-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/cachefs/cfsd
#

FSTYPE=		cachefs
LIBPROG_CC=	cachefsd
ATTMK=		$(LIBPROG_CC)
LINKVALUE=      ../lib/fs/$(FSTYPE)/$(LIBPROG_CC)

include		../../Makefile.fstype

PROGOBJS =	cfsd_main.o cfsd_svc.o cfsd_kmod.o cfsd_maptbl.o \
		cfsd_logelem.o cfsd_cache.o cfsd_fscache.o cfsd_all.o \
		cfsd_logfile.o cfsd_subr.o

LDLIBS +=	$(MDBUGLIB) -lrwtool -lthread
include		../Makefile.cachefs

MDBUGLIB=	../mdbug-cc/libdbug.a
CPPFLAGS +=	-I..
CCFLAGS +=	-mt
RPCGENFLAGS=	-M -C -T
CLEANFILES +=	cachefsd_tbl.i tcfs.o
CLOBBERFILES +=	$(LIBPROG_CC)

all : $(LIBPROG_CC)

install : $(ROOTLIBFSTYPE)/$(LIBPROG_CC)

$(LIBPROG_CC):	$(OBJS)
		$(CCC) $(LDFLAGS) -o $@ -R /usr/lib $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

testmap : cfsd_maptbl.o testmap.o
		$(LINK.cc) -o $@ cfsd_maptbl.o testmap.o $(LDLIBS)

cachefsd_tbl.i :	$(CACHEFSDIR)/cachefsd.x
		$(RPCGEN) $(RPCGENFLAGS) -t -o cachefsd_tbl.i \
			$(CACHEFSDIR)/cachefsd.x

cfsd_main.o :	cfsd_main.cc cachefsd_tbl.i

clobber :	templates

templates :
		rm -rf SunWS_cache

$(LIBPROG_CC) :	$(CFSLIB) $(MDBUGLIB)

$(MDBUGLIB) :
		cd $(@D); pwd; $(MAKE) $(TARGET);
		@pwd

$(PROGOBJS) :	$(CACHEFSDIR)/cachefsd.h ../mdbug-cc/mdbug.h
