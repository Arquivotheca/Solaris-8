#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.4	99/09/21 SMI"
#
# ucblib/libucb/Makefile.com
# common part for i386/sparc/sparcv9

LIBRARY=	libucb.a
VERS=		.1

PORTSYSOBJS=		\
	flock.o		\
	getdtblsize.o	\
	gethostid.o	\
	gethostname.o	\
	getpagesize.o	\
	getrusage.o	\
	gettimeofday.o	\
	killpg.o	\
	mctl.o		\
	reboot.o	\
	setpgrp.o	\
	wait3.o		\
	wait4.o

PORTSTDIOOBJS=		\
	doprnt.o	\
	fopen.o		\
	fprintf.o	\
	printf.o	\
	sprintf.o	\
	vfprintf.o	\
	vprintf.o	\
	vsprintf.o

PORTGENOBJS=		\
	_psignal.o	\
	bcmp.o		\
	bcopy.o		\
	bzero.o		\
	ftime.o		\
	getwd.o		\
	index.o		\
	nice.o		\
	nlist.o		\
	psignal.o	\
	rand.o		\
	readdir.o	\
	regex.o		\
	rindex.o	\
	scandir.o	\
	setbuffer.o	\
	setpriority.o	\
	siglist.o	\
	sleep.o		\
	statfs.o	\
	times.o		\
	ualarm.o	\
	usleep.o

OBJECTS= $(SYSOBJS) $(PORTGENOBJS) $(PORTSYSOBJS) $(PORTSTDIOOBJS)

# include library definitions
include $(SRC)/lib/Makefile.lib

ROOTLIBDIR=	$(ROOT)/usr/ucblib
ROOTLIBDIR64=	$(ROOT)/usr/ucblib/$(MACH64)

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES += $(MAPFILE)
MAPOPTS=	$(MAPFILE:%=-M %)

SRCS=		$(PORTGENOBJS:%.o=../port/gen/%.c) \
		$(PORTSTDIOOBJS:%.o=../port/stdio/%.c) \
		$(PORTSYSOBJS:%.o=../port/sys/%.c)

LIBS = $(DYNLIB) $(LINTLIB)

LINTSRC= $(LINTLIB:%.ln=%)
ROOTLINTDIR= $(ROOTLIBDIR)
ROOTLINTDIR64= $(ROOTLIBDIR)/$(MACH64)
ROOTLINT= $(LINTSRC:%=$(ROOTLINTDIR)/%)
ROOTLINT64= $(LINTSRC:%=$(ROOTLINTDIR64)/%)

STATICLIB=      $(LIBRARY:%=$(ROOTLIBDIR)/%)


# install rule for lint source file target
$(ROOTLINTDIR)/%: ../port/%
	$(INS.file)
$(ROOTLINTDIR64)/%: ../%
	$(INS.file)

$(LINTLIB):= SRCS=../port/llib-lucb
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=

# definitions for lint

LINTFLAGS=	-u
LINTFLAGS64=	-u -D__sparcv9
LINTOUT=	lint.out
CLEANFILES +=	$(LINTOUT) $(LINTLIB)

CFLAGS	+=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	$(MAPOPTS)
LDLIBS +=	-lelf -lc

CPPFLAGS = -D$(MACH) -I$(ROOT)/usr/ucbinclude -Iinc -I../inc -I../../../lib/libc/inc $(CPPFLAGS.master)

ASFLAGS= -P -D__STDC__ -DLOCORE -D_SYS_SYS_S -D_ASM $(CPPFLAGS)

pics/%.o:= ASFLAGS += -K pic

# libc method of building an archive, using AT&T ordering
BUILD.AR= $(RM) $@ ; \
	$(AR) q $@ `$(LORDER) $(OBJECTS:%=$(DIR)/%)| $(TSORT)`

.KEEP_STATE:

all: $(LIBS)

lint: $(LINTLIB)

$(DYNLIB): 	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

objs/%.o pics/%.o: ../port/gen/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
objs/%.o pics/%.o: ../port/stdio/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
objs/%.o pics/%.o: ../port/sys/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# shared (sparc/sparcv9/i386) platform-specific rule
objs/%.o pics/%.o: sys/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

#
# Include library targets
#
include $(SRC)/lib/Makefile.targ

# install rule for 32-bit libucb.a
$(STATICLIBDIR)/%: %
	$(INS.file)

