#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.1	99/10/14 SMI"
#
# lib/liblwp_db/Makefile.com

LIBRARY = libthread_db.a
VERS = .1

# Setting DEBUG = -DDEBUG (or "make DEBUG=-DDEBUG ...")
# enables ASSERT() checking in the library
#DEBUG = -DDEBUG
DEBUG =

OBJECTS = thread_db.o

# include library definitions
include ../../Makefile.lib

# Override defaults
ZDEFS =
ROOTLIBDIR =	$(ROOT)/usr/lib/lwp
ROOTLIBDIR64 =	$(ROOTLIBDIR)/$(MACH64)

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES +=	$(MAPFILE)

SRCS =		$(OBJECTS:%.o=../common/%.c)
INCDIRS =	-I../common -I../../liblwp/common

LIBS = $(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS =	-mux $(XARCH) $(INCDIRS) $(DEBUG) -D_REENTRANT
LINTFLAGS64 =	$(LINTFLAGS)
LINTOUT =	lint.out

LINTSRC =	$(LINTLIB:%.ln=%)
ROOTLINTDIR =	$(ROOTLIBDIR)
ROOTLINT =	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES =	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v $(INCDIRS) $(DEBUG) -D_REENTRANT
CFLAGS64 +=	-v $(INCDIRS) $(DEBUG) -D_REENTRANT -D_SYSCALL32
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-ldl -lc

$(LINTLIB) :=	SRCS = ../common/llib-lthread_db
$(LINTLIB) :=	LINTFLAGS = -nvx $(XARCH) $(INCDIRS) $(DEBUG) -D_REENTRANT
$(LINTLIB) :=	LINTFLAGS64 = $(LINTFLAGS)

.KEEP_STATE:

all: $(LIBS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)

# install rule for the redefined ROOTLIBDIR
$(ROOTLIBDIR) $(ROOTLIBDIR64):
	$(INS.dir)
