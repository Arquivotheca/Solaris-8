#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.6	99/03/23 SMI"
#
# lib/libproc/Makefile.com

LIBRARY = libproc.a
VERS = .1

CMNOBJS =	\
	P32ton.o	\
	Pcontrol.o	\
	Pcore.o		\
	Pexecname.o	\
	Pisprocdir.o	\
	Plwpregs.o	\
	Pservice.o	\
	Psymtab.o	\
	Pscantext.o	\
	Pstack.o	\
	Putil.o		\
	pr_door.o	\
	pr_exit.o	\
	pr_fcntl.o	\
	pr_getitimer.o	\
	pr_getrlimit.o	\
	pr_ioctl.o	\
	pr_lseek.o	\
	pr_memcntl.o	\
	pr_mmap.o	\
	pr_open.o	\
	pr_getsockname.o \
	pr_rename.o	\
	pr_sigaction.o	\
	pr_stat.o	\
	pr_statvfs.o	\
	pr_waitid.o	\
	proc_dirname.o	\
	proc_get_info.o	\
	proc_names.o	\
	proc_arg.o

ISAOBJS =	\
	Pisadep.o

OBJECTS = $(CMNOBJS) $(ISAOBJS)

# include library definitions
include ../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES +=	$(MAPFILE)

SRCS =		$(CMNOBJS:%.o=../common/%.c) $(ISAOBJS:%.o=%.c)

LIBS = $(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS =	-mux -I../common
LINTFLAGS64 =	-mux -Xarch=v9 -I../common
LINTOUT =	lint.out

LINTSRC =	$(LINTLIB:%.ln=%)
ROOTLINTDIR =	$(ROOTLIBDIR)
ROOTLINT =	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES =	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v -I../common
CFLAGS64 +=	-v -I../common
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-lrtld_db -lelf -lc

$(LINTLIB) :=	SRCS = ../common/llib-lproc
$(LINTLIB) :=	LINTFLAGS = -nvx -I../common
$(LINTLIB) :=	LINTFLAGS64 = -nvx -Xarch=v9 -I../common

.KEEP_STATE:

all: $(LIBS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: %.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)
