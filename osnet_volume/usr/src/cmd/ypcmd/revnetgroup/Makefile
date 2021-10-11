# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.10	96/04/25 SMI"
#
# cmd/ypcmd/revnetgroup/Makefile
#
							    

PROG = revnetgroup

include ../../Makefile.cmd

OBJS = revnetgroup.o getgroup.o table.o util.o
SRCS = $(OBJS:.o=.c)
HDRS = getgroup.h table.h util.h
LDLIBS += -ldl


all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPROG)

clean: 
	$(RM) $(OBJS)

lint:
	${LINT.c} ${SRCS}

cstyle:
	${CSTYLE} ${SRCS}

clobber: clean
	$(RM) $(PROG)

FRC:
