#
#ident	"@(#)Makefile	1.10	98/08/21 SMI"
#
# Copyright (c) 1989, 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/adb/sparc/Makefile
#

PROG= adb

COMMON=		../common
ARCHDIR=	.

OBJ_TARGET=	accesssr.o disasm.o fpasciisr.o command_$(MACH).o \
        	opsetsr.o printsr.o setupsr.o ptrace.o runpcs.o \
		sym64.o
OBJ_COM=	access.o command.o expr.o fio.o format.o \
        	input.o main.o output.o pcs.o print.o \
        	sym.o ttycontrol.o 
#kvm_stubs.o
OBJS=		$(OBJ_COM) $(OBJ_TARGET)

SRC_TARGET=	$(OBJ_TARGET:%.o=%.c)
SRC_COM=	$(OBJ_COM:%.o=$(COMMON)/%.c)
SRCS=		$(SRC_TARGET) $(SRC_COM)

include ../../Makefile.cmd
include ../../Makefile.cmd.64

INCS =		-I$(ARCHDIR) -I$(COMMON)
DEFINES =	-DADB -D$(MACH) -D__ELF -D_KMEMUSER -D__sparcv9 -D_SYSCALL32
CPPFLAGS +=	$(DEFINES) $(INCS)
LDLIBS =	$(LDLIBS64)
LDLIBS +=	-L$(ROOT)/usr/lib/sparcv9 -lkvm -lelf -lc -ldl

# build rules
%.o : $(ARCHDIR)/%.c
	$(COMPILE.c) $<

%.o : $(COMMON)/%.c
	$(COMPILE.c) $<

.KEEP_STATE:

.PARALLEL:	$(OBJS)

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG64)

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

#
# The cscope.out file is made in the current directory and spans
# all architectures.
# Things to note:
#	1. We use relative names for cscope and tags.
#	2. We *don't* remove the old cscope.out file, because cscope is
#	   smart enough to only build what has changed.  It can be
#	   confused, however, if files are renamed or removed, so it may
#	   be necessary to manually remove cscope.out if a lot of
#	   reorganization has occured.
#	3. We deliberately avoid names that include '.del' in their names
#	   as these are usually files that have been 'deleted' by nsefile.
#
TOPDIR	= ../../..
CSDIR	= .
CSDIRS	= $(CSDIR) ../common $(TOPDIR)/stand/lib $(TOPDIR)/stand/sys
CSINCS	= $(CSDIRS:%=-I%) \
	  -I. \
	  -I../common \
	  $(ENVCPPFLAGS1) $(ENVCPPFLAGS2) $(ENVCPPFLAGS3) $(ENVCPPFLAGS4)

CSCOPE	= cscope
CTAGS	= ctags

cscope.out: cscope.files FRC
	${CSCOPE} -b -f `pwd`/$@

cscope.files:   FRC
	@$(RM) -f cscope.files
	echo $(CSINCS) > cscope.files
	find $(CSDIRS) -name SCCS -prune -o \
	    \( -type d -name '.del*' \) -prune -o -type f \
	    \( -name '*.[chs]' -o -name 'Makefile*' -o -name '*.il' \) \
	    -a ! -name '.del*' -print >> cscope.files
	@wc -l cscope.files

#
# Create a tags data base, similar to above.
# Since assembler files now contain C fragments for lint,
# the lint fragments will allow ctags to "work" on assembler.
#
# Things to note:
#	1. We put plain files before headers, and .c before .s (because
#	   ctags doesn't understand assembly comments).
#	2. We *don't* sort the output of find, as we want functions in
#	   architecture directories to take precedence over those in
#	   sun, and those over those in common.
#
tags: cscope.files
	$(CTAGS) -wt `sed 1d cscope.files`

FRC:

include ../../Makefile.targ
