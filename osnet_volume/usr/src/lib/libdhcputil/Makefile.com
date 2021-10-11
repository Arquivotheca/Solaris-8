#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.1	99/08/02 SMI"
#
# lib/libdhcputil/Makefile.com

LIBRARY = libdhcputil.a
VERS    = .1
OBJECTS = dhcp_inittab.o dhcpmsg.o

# include library definitions
include ../../Makefile.lib

MAPDIR  =../spec/$(MACH)
MAPFILE = $(MAPDIR)/mapfile

SRCS    = $(OBJECTS:%.o=../common/%.c)
LIBS   += $(DYNLIB) $(LINTLIB)

CFLAGS += -v -I..
LDLIBS += -lc -lnsl
DYNFLAGS += -M $(MAPFILE)

CLEANFILES   += $(LINTOUT) $(LINTLIB)
CLOBBERFILES += $(MAPFILE)

# lint definitions
LINTFLAGS   = -I.. -u
LINTSRC     = $(LINTLIB:%.ln=%)
ROOTLINT    = $(LINTSRC:%=$(ROOTLINTDIR)/%)
ROOTLINTDIR = $(ROOTLIBDIR)
$(LINTLIB) := SRCS = ../common/$(LINTSRC)

.KEEP_STATE:

all:		$(LIBS)

$(DYNLIB):	$(MAPFILE)

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

# include library targets
include ../../Makefile.targ
