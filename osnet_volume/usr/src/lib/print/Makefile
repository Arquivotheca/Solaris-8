#
# Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
# All Right Reserved
#
# "@(#)Makefile	1.17	99/01/21 SMI"
#
# cmd/lp/client/lib/Makefile
#

LIBRARY =	libprint.a
VERS=.2

FILEMODE=	0555

NS_CMN =	ns_bsd_addr.o ns_cmn_kvp.o ns_cmn_printer.o
NS_SWITCH = 	nss_printer.o nss_convert.o nss_write.o
NS_SUPPORT =	ns.o ${NS_CMN} ${NS_SWITCH}

OBJECTS =	network.o job.o list.o misc.o $(NS_SUPPORT)


include		../Makefile.lib

MAPFILES=	mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)

CPPFLAGS +=	-I../../head -I../ -I.  -D_REENTRANT
LDLIBS +=	-lnsl -lsocket -ldl -lc
DYNFLAGS +=	$(MAPOPTS)


LIBS = $(DYNLIB)	# could be += for static and dynamic
# for messaging catalog
#
POFILE= bsd.po
SRCS= $(OBJECTS:%.o=%.c)
XGETFLAGS +=-a
 
.KEEP_STATE:
  
_msg:	$(POFILE)

$(POFILE):      $(SRCS)
	$(RM)	$@
	$(COMPILE.cpp)   $(SRCS) > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	sed "/^domain/d"        messages.po > $@
	$(RM) messages.po $(POFILE).i

all :	$(LIBS) 

install:	$(ROOTLIBDIR) $(ROOTLIBS) $(ROOTLINKS)

# definitions for lint

$(LINTLIB):= SRCS=./llib-lprint
$(LINTLIB):= LINTFLAGS = -nvx
LINTSRC=	$(LINTLIB:%.ln=%)
CLEANFILES +=	$(LINTLIB) $(LINTOUT)
LINTLIBS +=	-lnsl -lsocket -ldl -lc

lint: $(LINTLIB)
	$(LINT.c) $(LINTFLAGS) $(SRCS) 

cstyle:
	cstyle $(SRCS)

strip :

$(ROOTLIBDIR):
	$(INS.dir)

include		../Makefile.targ
