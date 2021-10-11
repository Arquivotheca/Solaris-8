#
# Copyright (c) 1994-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.8	99/04/14 SMI"
#
# Makefile for name service cache daemon
#
# cmd/nscd/Makefile
#

PROG=	nscd
NISPROG=	nscd_nischeck

include ../Makefile.cmd

OBJS=	server.o getpw.o getgr.o gethost.o getnode.o hash.o \
	nscd_biggest.o nscd_parse.o nscd_wait.o \
	getexec.o getprof.o getuser.o attrstr.o

NISOBJS=	nscd_nischeck.o  

CLOBBERFILES=	nscd nscd_nischeck

SRCS=	${OBJS:%.o=%.c} 

NISSRC=	${NISOBJS:%.o=%.c}

CPPFLAGS += 	-D_REENTRANT -DSUN_THREADS

# TCOV_FLAG=	-ql
# GPROF_FLAG=	-xpg
# DEBUG_FLAG=	-g

LDLIBS += 	-ldoor -lresolv -lnsl -lpthread -lthread

# install macros and rule
#
GROUP=	bin
ROOTPROG=	${ROOTUSRSBIN}/nscd
ROOTNISPROG=	${ROOTLIB}/nscd_nischeck

.KEEP_STATE:

all: $(PROG) $(NISPROG)

${PROG}: ${OBJS}
	${LINK.c} ${OPT} -o $@ ${OBJS} ${LDLIBS}
	${POST_PROCESS}

${NISPROG}: ${NISOBJS}
	${LINK.c} ${OPT} -o $@ ${NISOBJS} ${LDLIBS}

lint :=	LINTFLAGS=-x -b -u

lint:
	$(LINT.c) ${SRCS}

lint2:
	 $(LINT.c) ${NISSRC}

cstyle:
	${CSTYLE} ${SRCS} ${NISSRC}

install: all $(ROOTPROG) $(ROOTNISPROG)

clean:     
	${RM} ${OBJS} ${NISOBJS}

${ROOTUSRSBIN}/%: %
	${INS.file}

${ROOTUSRLIB}/%: %
	${INS.file}



include ../Makefile.targ



