#
#pragma ident	"@(#)Makefile	1.3	98/11/10 SMI"
#
# Copyright (c) 1994-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/cachefs/cfsd
#

FSTYPE=		cachefs
LIBPROG_C=	cachefsd
ATTMK=		$(LIBPROG_C)
LINKVALUE=      ../lib/fs/$(FSTYPE)/$(LIBPROG_C)

include		../../Makefile.fstype

PROGOBJS =	cfsd_main.o cfsd_svc.o cfsd_kmod.o cfsd_maptbl.o \
		cfsd_logelem.o cfsd_cache.o cfsd_fscache.o cfsd_all.o \
		cfsd_logfile.o cfsd_subr.o

LDLIBS +=	$(MDBUGLIB) -lkstat -mt
include		../Makefile.cachefs

MDBUGLIB=	../mdbug/libdbug.a
CFLAGS +=	-g -I.. -mt
RPCGENFLAGS=	-M -C -T
CLOBBERFILES +=	$(LIBPROG_C)

all : $(LIBPROG_C)

install : $(ROOTLIBFSTYPE)/$(LIBPROG_C)

$(LIBPROG_C):	$(OBJS)
		$(CC) $(LDFLAGS) -o $@ -R /usr/lib $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

testmap : cfsd_maptbl.o testmap.o
		$(LINK.c) -o $@ cfsd_maptbl.o testmap.o $(LDLIBS)

cachefsd_tbl.i :	$(CACHEFSDIR)/cachefsd.x
		$(RPCGEN) $(RPCGENFLAGS) -t -o cachefsd_tbl.i \
			$(CACHEFSDIR)/cachefsd.x

cfsd_main.o :	cfsd_main.c cachefsd_tbl.i

clobber :	templates

templates :
		rm -rf SunWS_cache

$(LIBPROG_C) :	$(CFSLIB) $(MDBUGLIB)

$(MDBUGLIB) :
		cd $(@D); pwd; $(MAKE) $(TARGET);
		@pwd

$(PROGOBJS) :	$(CACHEFSDIR)/cachefsd.h ../mdbug/mdbug.h
