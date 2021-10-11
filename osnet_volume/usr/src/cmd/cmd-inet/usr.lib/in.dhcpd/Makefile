#
#ident	"@(#)Makefile	1.51	99/08/27 SMI"
#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.lib/in.dhcpd/Makefile
#

COMMON_NET =	$(SRC)/common/net/dhcp

PROG =		in.dhcpd

LOCAL_OBJS=	bootp.o convert.o dhcp.o dhcptab.o dlprims.o encode.o \
		ethers.o generic.o hash.o icmp.o interfaces.o logging.o \
		main.o misc.o per_network.o pf.o relay.o
LOCAL_SRCS=	$(LOCAL_OBJS:%.o=%.c)

CMN_OBJS=	octet.o scan.o ipv4_sum.o udp_sum.o
CMN_SRCS=	$(CMN_OBJS:%.o=$(COMMON_NET)/%.c)

SRCS=		$(LOCAL_SRCS) $(CMN_SRCS)
OBJS=		$(LOCAL_OBJS) $(CMN_OBJS)

include ../../../Makefile.cmd

LDLIBS +=       -ldhcp -lsocket -lnsl -lthread
COPTFLAG +=	-DNDEBUG -D_REENTRANT -I$(COMMON_NET)
LINTFLAGS +=	-Xt -DNDEBUG -D_REENTRANT -I$(COMMON_NET)

# for messaging catalog. No messages are present in COMMON_NET sources.
POFILE = ${PROG}.po
POFILES = $(LOCAL_OBJS:%.o=%.po)
XGETFLAGS +=	-a -x in.dhcpd.xcl
.PARALLEL: $(OBJS)
.WAIT: $(PROG)

.KEEP_STATE:

all:	$(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

%.o:	$(COMMON_NET)/%.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POST_PROCESS_O)

$(POFILE):	$(POFILES)
	$(RM)	$@
	$(CAT) $(POFILES) > $@

include ../Makefile.lib

install:	all $(ROOTLIBINETPROG)

clean:
	$(RM) $(OBJS)

lint:
	$(LINT.c) $(SRCS) $(ENVLDLIBS1) -ldhcp -lsocket -lnsl

include ../../../Makefile.targ
