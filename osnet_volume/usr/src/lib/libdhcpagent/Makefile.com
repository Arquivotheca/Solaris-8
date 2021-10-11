#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.2	99/08/18 SMI"
#
# lib/libdhcpagent/Makefile.com

LIBRARY = libdhcpagent.a
VERS    = .1
COMDIR	= $(SRC)/common/net/dhcp
COMOBJS = scan.o
LOCOBJS = dhcp_hostconf.o dhcpagent_ipc.o dhcpagent_util.o
OBJECTS = $(LOCOBJS) $(COMOBJS)

# include library definitions
include ../../Makefile.lib

MAPDIR  =../spec/$(MACH)
MAPFILE = $(MAPDIR)/mapfile

SRCS    = $(LOCOBJS:%.o=../common/%.c) $(COMOBJS:%.o=$(COMDIR)/%.c)
LIBS   += $(DYNLIB) $(LINTLIB)

CFLAGS += -v -I.. -DDHCP_CLIENT
LDLIBS += -lc -lnsl -lsocket
DYNFLAGS += -M $(MAPFILE)

CLEANFILES   += $(LINTOUT) $(LINTLIB)
CLOBBERFILES += $(MAPFILE)

# lint definitions
LINTFLAGS   = -I.. -u -DDHCP_CLIENT
LINTSRC     = $(LINTLIB:%.ln=%)
ROOTLINT    = $(LINTSRC:%=$(ROOTLINTDIR)/%)
ROOTLINTDIR = $(ROOTLIBDIR)
$(LINTLIB) := SRCS = ../common/$(LINTSRC)

.KEEP_STATE:

all:		$(LIBS)

$(DYNLIB):      $(MAPFILE)

lint:		$(LINTLIB) lintcheck

lintcheck:	$$(SRCS)
	$(LINT.c) $(SRCS) > $(LINTOUT) 2>&1

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: $(COMDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# include library targets
include ../../Makefile.targ
