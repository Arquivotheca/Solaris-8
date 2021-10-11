#
#ident	"@(#)Makefile.profile	1.2	99/10/23 SMI"
#
# Copyright (c) 1996-1999 by Sun Microsystems, Inc. All rights Reserved.
#
# lib/libdhcp/Makefile.profile
#
LIBRARY = libdhcp.a
VERS = .2
HDRS = dhcdata.h
ROOTHDRDIR =	$(ROOT)/usr/include
ROOTHDRS =	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKDIRS =	$(HDRS:%.h=%.check)
CMN_NET_DHCP =	$(SRC)/common/net/dhcp

# Objects from sources under $(CMN_NET_DHCP).
CMN_DHCP_OBJS =	ipv4_sum.o octet.o scan.o udp_sum.o

# Objects from local sources.
LOCAL_OBJS =	dd.o dd_impl.o dd_defs.o defaults.o nisplus_dd.o ufs_dd.o \
		valid.o

OBJECTS =	$(LOCAL_OBJS) $(CMN_DHCP_OBJS)

# include library definitions
include ../Makefile.lib

MAPFILE=	mapfile-vers
# TCOV COPTFLAG =	-Xt -xprofile=tcov
# GPROF COPTFLAG =	-Xt -xpg
COPTFLAG =	-Xt -xpg
LIBS = $(DYNLIB)
CPPFLAGS += -I. -I$(CMN_NET_DHCP)
LDLIBS += -lxfn -lsocket -lnsl -lc
CLOBBERFILES += $(DYNLIB)
DYNFLAGS +=	-M $(MAPFILE)
$(ROOTHDRS) :=  FILEMODE= 644

.KEEP_STATE:

all: $(LIBS)

$(DYNLIB):	$(MAPFILE)

# build rule for COMMON source
objs/%.o profs/%.o pics/%.o: $(CMN_NET_DHCP)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTDYNLIB)

install_h: $(ROOTHDRS)

check: $(CHECKHDRS)

lint: llib-ldhcp.ln

$(ROOTHDRDIR)/%: %
	$(INS.file)

# include library targets
include ../Makefile.targ
