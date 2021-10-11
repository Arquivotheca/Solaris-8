#
#ident	"@(#)Makefile.com	1.7	99/03/19 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libcfgadm/Makefile.com
#

LIBRARY= libcfgadm.a
VERS= .1

PICS= pics/config_admin.o

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

OBJECTS= config_admin.o

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES +=	$(MAPFILE)

SRCS=           $(OBJECTS:%.o=../common/%.c)

DYNFLAGS += -M $(MAPFILE)
LDLIBS +=	-ldevinfo -lc

ROOTDYNLIBS= $(DYNLIB:%=$(ROOTLIBDIR)/%)

LIBS = $(DYNLIB) $(LINTLIB)		# Make sure we don't build a static lib

$(LINTLIB):= SRCS=../common/llib-lcfgadm
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=

LINTSRC= $(LINTLIB:%.ln=%)
ROOTLINTDIR= $(ROOTLIBDIR)
ROOTLINT= $(LINTSRC:%=$(ROOTLINTDIR)/%) $(LINTLIB:%=$(ROOTLINTDIR)/%)

ZDEFS=

.KEEP_STATE:

all: $(TXTS) $(LIBS)

$(DYNLIB): $(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../../Makefile.targ

# lint source file installation target

$(ROOTLINTDIR)/%:	../common/%
	$(INS.file)

lint: $(LINTLIB)
