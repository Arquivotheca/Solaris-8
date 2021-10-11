#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.1	97/06/06 SMI"
#
# ucblib/libtermcap/Makefile.com

LIBRARY=	libtermcap.a
VERS=		.1

OBJECTS= termcap.o	\
	 tgoto.o	\
	 tputs.o

# include library definitions
include $(SRC)/lib/Makefile.lib

ROOTLIBDIR=	$(ROOT)/usr/ucblib
ROOTLIBDIR64=	$(ROOT)/usr/ucblib/$(MACH64)

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES +=	$(MAPFILE)
SRCS=		$(OBJECTS:%.o=../%.c)

# static is only for 32-bit
LIBS = $(DYNLIB) $(LINTLIB)

LINTSRC= $(LINTLIB:%.ln=%)
ROOTLINTDIR= $(ROOTLIBDIR)
ROOTLINTDIR64= $(ROOTLIBDIR)/$(MACH64)
ROOTLINT= $(LINTSRC:%=$(ROOTLINTDIR)/%)
ROOTLINT64= $(LINTSRC:%=$(ROOTLINTDIR64)/%)
STATICLIB= $(LIBRARY:%=$(ROOTLIBDIR)/%)

# install rule for lint source file target
$(ROOTLINTDIR)/%: ../%
	$(INS.file)
$(ROOTLINTDIR64)/%: ../%
	$(INS.file)

$(LINTLIB):= SRCS=../llib-ltermcap
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=

# definitions for lint

LINTFLAGS=	-u
LINTFLAGS64=	-u -D__sparcv9
LINTOUT=	lint.out
CLEANFILES +=	$(LINTOUT) $(LINTLIB)

CFLAGS	+=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	
DYNFLAGS32 =	-Wl,-M,$(MAPFILE)
DYNFLAGS64 =	-Wl,-M,$(MAPFILE)
LDLIBS +=	-lc

DEFS= -DCM_N -DCM_GT -DCM_B -DCM_D
CPPFLAGS = $(DEFS) -I$(ROOT)/usr/ucbinclude $(CPPFLAGS.master)

.KEEP_STATE:

all: $(LIBS)

lint: $(LINTLIB)

$(DYNLIB): 	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

#
# Include library targets
#
include $(SRC)/lib/Makefile.targ

objs/%.o pics/%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for 32-bit libtermcap.a
$(STATICLIBDIR)/%: %
	$(INS.file)

