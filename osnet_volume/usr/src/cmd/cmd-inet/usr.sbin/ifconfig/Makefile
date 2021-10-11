#
# Makefile for "ifconfig" 
# Copyright (c) 1992-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.25	99/10/04 SMI"
#

PROG =		ifconfig
OBJS=		dupl_addr.o ifconfig.o revarp.o
SRCS=		$(OBJS:%.o=%.c)

# duplicate ROOTSBIN value needed for installation rule
ROOTSBIN = $(ROOT)/sbin
$(ROOTSBIN)/%: %.ps
	$(INS.rename)

include ../../../Makefile.cmd

LDLIBS +=       -lcmd -lsocket -lnsl -ldevinfo -ldhcpagent
LINTFLAGS +=	-m

# ifconfig uses the ancillary data feature which is available only through
# UNIX 98 standards version of Socket interface. This interface is supposed to
# be accessed by -lxnet. In addition -lresolv, -lsocket and -lnsl are used to
# capture new not-yet-standard interfaces. Someday -lxnet alone should be enough
# when IPv6 inspired new interfaces are part of standards.
LDLIBS +=	-lxnet -lresolv -lsocket -lnsl  	

# these #defines are required to use UNIX 98 interfaces
_D_UNIX98_EXTN= -D_XOPEN_SOURCE=500 -D__EXTENSIONS__

$(OBJS)		:= CPPFLAGS +=	$(_D_UNIX98_EXTN)

LINTFLAGS +=	$(_D_UNIX98_EXTN)

# `PS[ps]' is meant to imply "partially static" because
# the version of ifconfig(1) that goes in $(ROOT)/sbin
# is dynamic wrt ld.so, but static wrt all others.
PS = $(PROG:%=%.ps)
$(PS) := LDLIBS = $(LDLIBS.cmd) $(NEEDU) -Bdynamic -ldl -Bstatic -lcmd -ldevinfo -lsocket -lnsl -lelf -ldhcpagent -lc
$(PROG) := LDLIBS += -ldl -lelf
$(PS) := LDFLAGS = -znodefaultlib -R/etc/lib $(LDFLAGS.cmd)
ROOTPSPROG = $(PROG:%=$(ROOTSBIN)/%)
CLOBBERFILES += $(PS)
#
# We need to make these become externally available so
# that nss_files.so.1 (ps) can find them at run time.
NEEDU=		-u __ctype \
		-u isspace \
		-u llseek \
		-u memcmp \
		-u nlist \
		-u rewind \
		-u strcasecmp \
		-u strstr \
		-u vfprintf \
		-u __nss2herrno

.KEEP_STATE:

all:	$(PROG) $(PS)

$(PROG):$(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(PS):	$(OBJS)
	LD_OPTIONS="-I/etc/lib/ld.so.1" \
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install:	all $(ROOTUSRSBINPROG)  $(ROOTPSPROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../../../Makefile.targ
