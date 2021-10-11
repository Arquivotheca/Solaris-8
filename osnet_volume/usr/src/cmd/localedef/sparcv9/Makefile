#
# Copyright (c) 1997, 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile 1.9	00/09/14 SMI"
#
#include $(SRC)/Makefile.master.64

PROG=	localedef

OBJS1=	gram.o

OBJS2=	check.o \
	copy.o \
	err.o \
	gen.o \
	init.o \
	list.o \
	localedef.o \
	method.o \
	scan.o \
	sem_chr.o \
	sem_ctype.o \
	sem_xlat.o \
	sem_method.o  \
	semstack.o \
	sem_coll.o \
	symtab.o \
	sem_comp.o

OBJS=	$(OBJS1) $(OBJS2)

SRCS= $(OBJS1:%.o=%.c) $(OBJS2:%.o=../%.c)

include ../../Makefile.cmd
include ../../Makefile.cmd.64
include ../Makefile.locale
include ../Makefile.target

$(PROG)	:= CFLAGS += -D_LOCALEDEF -I.
$(PROG)	:= CFLAGS64 += -D_LOCALEDEF -I.
$(ISOOBJECT) := CFLAGS += -K pic -v -xregs=no%appl
$(ISOOBJECT) := CFLAGS64 += -K pic -v -xregs=no%appl 
YFLAGS += -d
LDLIBS += -ldl
DYNFLAGS += -h $@ -z text -z defs 

CLOBBERFILES =	$(ISOOBJECT) $(ISOLOCALE)

CLEANFILES = $(OBJS) gram.c y.tab.h $(ISOLOCALE).o

.KEEP_STATE:

.PARALLEL: $(OBJS)

all: $(ISOOBJECT)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all \
	$(ROOTDIRS64) $(ROOTISOOBJECT64)


clean:
	$(RM) $(CLEANFILES)

lint:	lint_SRCS


scan.o :	y.tab.h

%.o:	%.c
	$(COMPILE.c) $<

%.o:	../%.c
	$(COMPILE.c) $<

#
# WARNING: if the output of localedef changes then this ISOLOCALE
# must be regenerated and the C source put here.
# If you know of a way to build a makefile dependency then please do so.
#

$(ISOOBJECT):	$(ISOLOCALE).o
	$(CC) $(CFLAGS) -o $@ -G $(DYNFLAGS) $(ISOLOCALE).o \
		-L $(ROOT)/usr/lib/$(MACH64) $(LDLIBS) -lc
	$(POST_PROCESS)


include ../../Makefile.targ
