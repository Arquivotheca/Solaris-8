#
#ident	"@(#)Makefile.com	1.2	99/01/25 SMI"
#
#
# Copyright (c) 1989,1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libtnfprobe/Makefile
#

LIBRARY=	libtnfprobe.a
VERS=		.1
OBJECTS.c=	tnf_buf.o	\
		trace_init.o	\
		trace_funcs.o	\
		debug_funcs.o	\
		probe_mem.o	\
		tnf_args.o	\
		tnf_trace.o	\
		probe_cntl.o

UFSDIR=		$(SRC)/uts/common/tnf
UFSOBJS=	tnf_writer.o tnf_probe.o

OBJECTS.s=	$(MACH)_locks.o

OBJECTS=	$(OBJECTS.c) $(UFSOBJS) $(OBJECTS.s)

include ../../Makefile.lib

# We omit $(OBJECTS.s:%.o=%.s) in the next line, because lint no like
SRCS= $(OBJECTS.c:%.o=../%.c) $(UFSOBJS:%.o=$(UFSDIR)/%.c)

LIBS=		$(DYNLIB)

MAPFILE=	$(MAPDIR)/mapfile
MAPOPTS=	$(MAPFILE:%=-M %)
DYNFLAGS +=	$(MAPOPTS)
CLOBBERFILES +=	$(MAPFILE)

HDRS=		com.h writer.h probe.h
ROOTHDRDIR=	$(ROOT)/usr/include/tnf
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)
$(ROOTHDRS) := 	FILEMODE = 0644
CHECKHDRS =	$(HDRS:%.h=%.check)

# Include .. first to pick up tnf_trace.h in current dir, Include UFSDIR to
#	pick up tnf_types.h
CPPFLAGS +=	-I.. -I$(UFSDIR) -D_REENTRANT -D_TNF_LIBRARY

LINTFLAGS +=	-y

$(ROOTHDRS) :=	FILEMODE = 644

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

LDLIBS += -ldl -lc

.KEEP_STATE:

all: $(LIBS)

install_h: $(ROOTHDRDIR) $(ROOTHDRS)

lint:
	$(LINT.c) $(SRCS)

check: $(CHECKHDRS)

$(ROOTLIBDIR) $(ROOTHDRDIR):
	$(INS.dir)

$(ROOTHDRDIR)/% : %
	$(INS.file)

#ASFLAGS=	-K pic -P -D_SYS_SYS_S -D_LOCORE -D_ASM -DPIC -DLOCORE $(CPPFLAGS)
ASFLAGS=	-P -D_SYS_SYS_S -D_LOCORE -D_ASM -DPIC -DLOCORE $(CPPFLAGS)
BUILD.s=	$(AS) $(ASFLAGS) $< -o $@

objs/%.o pics/%.o: ../%.s
		$(COMPILE.s) -o $@ $<
		$(POST_PROCESS_O)

pics/%.o objs/%.o: ../%.c
		$(COMPILE.c) -o $@ $<
		$(POST_PROCESS_O)

pics/%.o objs/%.o:	$(UFSDIR)/%.c
		$(COMPILE.c) -o $@ $<
		$(POST_PROCESS_O)

include ../../Makefile.targ
