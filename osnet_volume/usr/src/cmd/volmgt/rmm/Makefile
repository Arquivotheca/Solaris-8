#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# Makefile for Removable Media Mounter.
#
#ident	"@(#)Makefile	1.19	98/12/04 SMI"
#
# cmd/volmgt/rmm/Makefile
#

PROG=	rmmount


include ../../../lib/Makefile.lib
include ../../Makefile.cmd
include ../Makefile.volmgt

SED=	sed

OBJS=	rmm.o rmm_util.o rmm_config.o
SRCS=	$(OBJS:%.o=%.c) ${ACTS:%.so.$(ACT_VERS)=%.c}

# identification functions are in src/cmd/fs.d/<fstype>.
IDENT_VERS=1

# actions
ACT_VERS=1
ACTS=	action_workman.so.${ACT_VERS} action_filemgr.so.${ACT_VERS}

CFLAGS += -v

CPPFLAGS += 	-DACT_VERS=${ACT_VERS} -DIDENT_VERS=${IDENT_VERS}

$(PROG) :=	LDLIBS += -ldl -lvolmgt -ladm
$(ACTS) :=	LDLIBS += -lvolmgt -lc

# to get the correct DSO flags used for compilation/linking
# (should also use "-zdefs" for DYNFLAGS, but ACTS are not yet self-contained)
$(ACTS) :=	CFLAGS += -K pic
$(ACTS) :=	HSONAME =
$(ACTS) :=	ZDEFS =

# for messaging catalog
#
POFILE= rmm.po

#
# Rules for making shared objects out of .c files.  Works well if
# we have a one-to-one mapping.  Applies in all cases so far.
#
.SUFFIXES: .so.${ACT_VERS} ${SUFFIXES}

.c.so.${ACT_VERS}:
	${CC} ${CPPFLAGS} ${CFLAGS} -G -o $@ $< $(DYNFLAGS) $(LDLIBS)
	$(POST_PROCESS_SO)

# install macros and rule
#
ROOTUSRSBINPROG=	${PROG:%=${ROOTUSRSBIN}/%}
RMMLIBOBJS=		${ACTS:%=${RMMLIBDIR}/%}

lint :=	LINTFLAGS += -errchk=longptr64 -fd -Ncheck -Nlevel=4

${RMMLIBDIR}/%: %
	${INS.file}

.KEEP_STATE:

all: ${PROG} ${ACTS}

catalog: $(POFILE)

$(POFILE): $(SRCS)
	$(RM) $@
	$(COMPILE.cpp) $(SRCS) | \
	$(XGETTEXT) $(XGETFLAGS) -
	$(SED) -e '/^domain/d' messages.po > $@
	$(RM) messages.po

${PROG}: ${OBJS}
	${LINK.c} -o $@ ${OBJS} $(LDLIBS)
	$(POST_PROCESS)

lint:
	$(LINT.c) ${SRCS}

cstyle:
	${CSTYLE} ${SRCS}

install: all ${RMMLIBDIR} ${RMMLIBOBJS} ${ROOTUSRSBINPROG}

${RMMLIBDIR}:
	${INS.dir}

clean: 
	${RM} $(OBJS) ${ACTS} core a.out

clobber: clean
	${RM} $(PROG)
