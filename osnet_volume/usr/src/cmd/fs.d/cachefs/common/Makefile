#
#ident	"@(#)Makefile	1.5	96/04/18 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/cachefs/common
#

FSTYPE=		cachefs

include		../../Makefile.fstype

PROGOBJS=	subr.o stats_create.o stats_stats.o stats_log.o stats_dbm.o \
		cachefsd_clnt.o cachefsd_xdr.o

include		../Makefile.cachefs

ARFLAGS=	rc
CLEANFILES +=	cachefsd_clnt.c cachefsd_xdr.c
CLOBBERFILES += $(LIBRARY) cachefsd.h
RPCGENFLAGS=	-M -C -T
CPPFLAGS +=	-D_LARGEFILE64_SOURCE

all:	$(LIBRARY)

$(LIBRARY):	$(OBJS)
	-$(RM) $(LIBRARY)
	$(AR) $(ARFLAGS) $(LIBRARY) $(OBJS)

cachefsd_clnt.c : cachefsd.x
	$(RPCGEN) $(RPCGENFLAGS) -l -o cachefsd_clnt.c cachefsd.x

cachefsd_xdr.c : cachefsd.x
	$(RPCGEN) $(RPCGENFLAGS) -c -o cachefsd_xdr.c cachefsd.x

cachefsd.h : cachefsd.x
	$(RPCGEN) $(RPCGENFLAGS) -h -o cachefsd.h cachefsd.x

cachefsd_clnt.o cachefsd_xdr.o : cachefsd.h
