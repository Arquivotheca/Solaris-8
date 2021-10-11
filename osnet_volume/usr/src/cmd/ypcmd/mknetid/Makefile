#
#ident	"@(#)Makefile	1.9	96/04/25 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ypcmd/mknetid/Makefile
							    

PROG = mknetid

include ../../Makefile.cmd

LDLIBS += -ldl -lnsl

OBJS = mknetid.o getname.o 
SRCS = $(OBJS:.o=.c)


.KEEP_STATE:

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
