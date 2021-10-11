#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.3	99/11/24 SMI"
#
# cmd/sort/Makefile.com
#

PROG = sort
XPG4PROG = sort
OBJS =	check.o fields.o initialize.o internal.o main.o merge.o \
	options.o streams.o streams_array.o streams_mmap.o streams_stdio.o \
	streams_wide.o utility.o
XPG4OBJS = $(OBJS:%.o=xpg4_%.o)
SRCS =  $(OBJS:%.o=../common/%.c)
LNTS =	$(OBJS:%.o=%.ln)
CLEANFILES = $(OBJS) $(XPG4OBJS) $(LNTS)

include ../../Makefile.cmd

SED =		sed
DCFILE = 	$(PROG).dc

SPACEFLAG =
SPACEFLAG64 =
COPTFLAG =	-xO3
COPTFLAG64 =

CFLAGS +=	-v
$(XPG4)	:=	CFLAGS += -DXPG4
CFLAGS64 +=	-v
CPPFLAGS +=	-D_FILE_OFFSET_BITS=64
LINTFLAGS =	-U_FILE_OFFSET_BITS -mx
LINTFLAGS64 +=	-Xarch=v9 

.KEEP_STATE :

.PARALLEL : $(OBJS) $(XPG4OBJS) $(LNTS)

all : $(PROG) $(XPG4)

lint : $(LNTS)
	$(LINT.c) $(LINTFLAGS) $(LNTS) $(LDLIBS)

clean :
	$(RM) $(CLEANFILES)

include ../../Makefile.targ

# rules for $(PROG) and $(XPG4)

$(PROG) : $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

$(XPG4) : $(XPG4OBJS)
	$(LINK.c) -o $@ $(XPG4OBJS) $(LDLIBS)
	$(POST_PROCESS)

%.o : ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

xpg4_%.o : ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

%.o : ../common/%.h types.h

xpg4_%.o : ../common/%.h types.h

%.ln: ../common/%.c
	$(LINT.c) $(LINTFLAGS) -c $<
