#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved. 
#
#pragma ident	"@(#)Makefile.com 1.2	99/08/10 SMI"
#
# lib/cfgadm_plugins/scsi/Makefile.com
#

LIBRARY= scsi.a
VERS= .1

OBJECTS= cfga_ctl.o cfga_cvt.o cfga_list.o cfga_scsi.o cfga_utils.o cfga_rcm.o

# include library definitions
include ../../../Makefile.lib

ROOTLIBDIR=	$(ROOT)/usr/lib/cfgadm
ROOTLIBDIR64=	$(ROOTLIBDIR)/$(MACH64)

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES +=	$(MAPFILE)

SRCS=		$(OBJECTS:%.o=../common/%.c)

LIBS=	$(DYNLIB)

# definitions for lint

LINTFLAGS=	-u
LINTFLAGS64=	-u
LINTOUT=	lint.out
CLEANFILES=	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v
CFLAGS64 +=	-v

DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-lc -ldevice -ldevinfo -lrcm

.KEEP_STATE:

all:	$(LIBS)

lint:	$(LINTLIB)

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# Install rules

$(ROOTLIBDIR)/%: % $(ROOTLIBDIR)
	$(INS.file)

$(ROOTLIBDIR64)/%: % $(ROOTLIBDIR64)
	$(INS.file)

$(ROOTLIBDIR) $(ROOTLIBDIR64):
	$(INS.dir)

# include library targets
include ../../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
