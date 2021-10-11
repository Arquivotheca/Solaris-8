#
#pragma ident	"@(#)Makefile	1.3	99/08/25 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sbin/ping/Makefile

PROG=		ping
PINGOBJS=	ping.o ping_aux.o ping_aux6.o
COMMONOBJS=	ifaddrlist.o
OBJS=		$(PINGOBJS) $(COMMONOBJS)
SUIDPROG=	ping

include	../../../Makefile.cmd
include	../../Makefile.cmd-inet

PINGSRCS=	$(PINGOBJS:.o=.c)
COMMONSRCS=	$(CMDINETCOMMONDIR)/$(COMMONOBJS:.o=.c)
SRCS=		$(PINGSRCS) $(COMMONSRCS)
HDRS=		ping.h $(CMDINETCOMMONDIR)/ifaddrlist.h

$(ROOTUSRSBIN)/ping	:=	FILEMODE= 04555
$(ROOTUSRSBIN)/ping	:=	OWNER= root


CPPFLAGS +=	-I$(CMDINETCOMMONDIR)

# Ping uses the ancillary data feature which is available only through
# UNIX 98 standards version of Socket interface. This interface is supposed to
# be accessed by -lxnet. In addition -lsocket and -lnsl are used to
# capture new not-yet-standard interfaces. Someday -lxnet alone should be enough
# when IPv6 inspired new interfaces are part of standards.
LDLIBS +=	-lxnet -lsocket -lnsl  	

# these #defines are required to use UNIX 98 interfaces
_D_UNIX98_EXTN = -D_XOPEN_SOURCE=500 -D__EXTENSIONS__

$(PINGOBJS)		:= CPPFLAGS +=	$(_D_UNIX98_EXTN)

LINTFLAGS +=	$(_D_UNIX98_EXTN)

.KEEP_STATE:

.INIT:	$(HDRS)


all:	$(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

$(COMMONOBJS): $(COMMONSRCS)
	$(COMPILE.c) $(COMMONSRCS)

install: all $(ROOTUSRSBINPROG)

clean:	
	$(RM) $(OBJS) tags

lint:	lint_SRCS

tags:	$(SRCS)
	ctags $(SRCS)


include ../../../Makefile.targ
