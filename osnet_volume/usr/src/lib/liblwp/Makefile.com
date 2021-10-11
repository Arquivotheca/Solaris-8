#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.1	99/10/14 SMI"
#
# lib/liblwp/Makefile.com

LIBRARY = libthread.a
VERS = .1

CMNOBJS =	\
	cancel.o		\
	lwp.o			\
	pthr_attr.o		\
	pthr_cond.o		\
	pthr_mutex.o		\
	pthr_rwlock.o		\
	pthread.o		\
	resv_tls.o		\
	rtsched.o		\
	rwlock.o		\
	scalls.o		\
	sema.o			\
	sigaction.o		\
	synch.o			\
	tdb_agent.o		\
	thread_interface.o	\
	tsd.o

ISAOBJS = machdep.o

ASMOBJS = asm_subr.o

OBJECTS = $(CMNOBJS) $(ISAOBJS) $(ASMOBJS)

# include library definitions
include ../../Makefile.lib

# Setting DEBUG = -DDEBUG (or "make DEBUG=-DDEBUG ...")
# enables ASSERT() checking in the library
# This is automatically enabled for DEBUG builds, not for non-debug builds.
DEBUG =
$(NOT_RELEASE_BUILD)DEBUG = -DDEBUG

# Override defaults
ROOTLIBDIR =	$(ROOT)/usr/lib/lwp
ROOTLIBDIR64 =	$(ROOTLIBDIR)/$(MACH64)

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES +=	$(MAPFILE)

LIBS = $(DYNLIB) $(LINTLIB)

# We need -I../../libc/inc to find <thr_int.h>
INCLUDE = -I../common -I../../libc/inc

# definitions for lint

LINTFLAGS =	-mux $(XARCH) $(INCLUDE) $(DEBUG) -D_REENTRANT
LINTFLAGS64 =	$(LINTFLAGS)
LINTOUT =	lint.out

LINTSRC =	$(LINTLIB:%.ln=%)
ROOTLINTDIR =	$(ROOTLIBDIR)
ROOTLINT =	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES =	$(LINTOUT) $(LINTLIB)

# It would be better if we did not have to do initfirst, but we must.
# Also, we must make the library non-deletable.
INITFIRST = -z initfirst -z nodelete

CFLAGS +=	-v $(INCLUDE) $(MACH).il $(DEBUG) -D_REENTRANT
CFLAGS64 +=	-v $(INCLUDE) $(MACH64).il $(DEBUG) -D_REENTRANT
DYNFLAGS +=	-M $(MAPFILE) $(INITFIRST)
LDLIBS +=	-lc -ldl

$(LINTLIB) :=	SRCS = ../common/llib-lthread
$(LINTLIB) :=	LINTFLAGS = -nvx $(XARCH) $(INCLUDE) $(DEBUG) -D_REENTRANT
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

objs/%.o pics/%.o: %.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ../$(MACH)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: %.s
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ../$(MACH)/%.s
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)

# install rule for the redefined ROOTLIBDIR
$(ROOTLIBDIR) $(ROOTLIBDIR64):
	$(INS.dir)
