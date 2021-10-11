#
# Copyright (c) 1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.23	99/10/23 SMI"
#
# lib/libdhcp/Makefile
#
LIBRARY = libdhcp.a
VERS = .2
M4 =	m4
HDRS = dhcdata.h dhcp_defaults.h
ROOTHDRDIR =	$(ROOT)/usr/include
ROOTHDRS =	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS =	$(HDRS:%.h=%.check)
CMN_NET_DHCP =	$(SRC)/common/net/dhcp

# Objects from sources under $(CMN_NET_DHCP).
CMN_DHCP_OBJS =	ipv4_sum.o octet.o scan.o udp_sum.o
CMN_DHCP_SRCS = $(CMN_DHCP_OBJS:%.o=$(CMN_NET_DHCP)/%.c)

# Objects from local sources.
LOCAL_OBJS =	dd.o dd_impl.o dd_defs.o defaults.o nisplus_dd.o ufs_dd.o \
		valid.o
LOCAL_SRCS = 	$(LOCAL_OBJS:%.o=%.c)

OBJECTS =	$(LOCAL_OBJS) $(CMN_DHCP_OBJS)

# include library definitions
include ../Makefile.lib

SRCS =		$(LOCAL_SRCS) $(CMN_DHCP_SRCS)
LIBS =		$(DYNLIB)
MAPDIR =	./spec/$(MACH)
MAPFILE =	$(MAPDIR)/mapfile

CPPFLAGS += -I. -I$(CMN_NET_DHCP)
LDLIBS += -lxfn -lsocket -lnsl -lgen -lc
DYNFLAGS +=	-M $(MAPFILE)
$(ROOTHDRS) :=  FILEMODE= 644
CLOBBERFILES +=	$(MAPFILE) $(LINTLIB) $(LINTOUT)
CLEANFILES += dhcp_defaults.h

.KEEP_STATE:
.PARALLEL: $(OBJECTS)
.WAIT: $(DYNLIB)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install
lint :=		TARGET= lint

all: spec .WAIT $(DYNLIB)

install: all spec $(ROOTDYNLIB) $(ROOTLINKS)

clean clobber: spec

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# build rule for COMMON source
objs/%.o profs/%.o pics/%.o: $(CMN_NET_DHCP)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

spec:	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

dhcp_defaults.h: $(CMN_NET_DHCP)/dhcp_defaults.m4
	$(M4) $? > $@

install: all $(ROOTLIBS) $(ROOTLINKS)

install_h: $(ROOTHDRS)

check: $(CHECKHDRS)

lint: $(LINTLIB) spec

$(ROOTHDRDIR)/%: %
	$(INS.file)

# include library targets
include ../Makefile.targ

FRC:
