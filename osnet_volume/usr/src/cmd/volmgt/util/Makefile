#
# Copyright 1996, Sun Microsystems
# All rights reserved.
#
# Makefile for volume management utilities
#
#ident	"@(#)Makefile	1.28	96/10/10 SMI"
#
# cmd/volmgt/util/Makefile

include ../../Makefile.cmd
include ../Makefile.volmgt

CAT=		cat

OBJS=		eject.o volcancel.o volstat.o volmissing.o volcheck.o \
		volrmmount.o
SRCS=		$(OBJS:%.o=%.c)

LDLIBS +=	-lvolmgt -ladm
LPROG=		volcancel volstat volmissing
BPROG=		eject volcheck volrmmount

# for messaging catalog
#
POFILES=$(LPROG:%=%.po) $(BPROG:%=%.po)
POFILE=util.po

# install macros and rules
#
ROOTBINPROG=	${BPROG:%=${ROOTBIN}/%}
VOLLIBPROG=	${LPROG:%=${VOLLIBDIR}/%}

#
# eject, volcheck and volrmmount need to be suid root
#
${ROOTBIN}/eject := FILEMODE = 04555
${ROOTBIN}/volcheck := FILEMODE = 04555
${ROOTBIN}/volrmmount := FILEMODE = 04555
${ROOTBIN}/eject := OWNER = root
${ROOTBIN}/volcheck := OWNER = root
${ROOTBIN}/volrmmount := OWNER = root

lint := LINTFLAGS += -errchk=longptr64 -Nlevel=4 -Ncheck -fd

.KEEP_STATE:

all: volsetup ${LPROG} ${BPROG}

eject:	eject.o
	${LINK.c} -o $@ eject.o ${LDLIBS}
	$(POST_PROCESS)

volcancel: volcancel.o
	${LINK.c} -o $@ volcancel.o ${LDLIBS}
	$(POST_PROCESS)

volmissing: volmissing.o
	${LINK.c} -o $@ volmissing.o ${LDLIBS}
	$(POST_PROCESS)

volstat: volstat.o
	${LINK.c} -o $@ volstat.o ${LDLIBS}
	$(POST_PROCESS)

volcheck: volcheck.o
	${LINK.c} -o $@ volcheck.o ${LDLIBS}
	$(POST_PROCESS)

volrmmount: volrmmount.o
	${LINK.c} -o $@ volrmmount.o ${LDLIBS}
	$(POST_PROCESS)

catalog: $(POFILE)

$(POFILE): $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

${VOLLIBDIR}/%: %
	${INS.file}

${VOLLIBDIR}:
	${INS.dir}

install: all ${VOLLIBDIR} ${VOLLIBPROG} ${ROOTBINPROG}

cstyle:
	${CSTYLE} ${SRCS}

lint:
	${LINT.c} $(LINTFLAGS) ${SRCS}

clean:
	${RM} ${OBJS} *.o a.out core

clobber: clean
	${RM} ${LPROG} $(BPROG)
