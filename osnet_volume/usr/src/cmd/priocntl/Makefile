#
#ident	"@(#)Makefile	1.9	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/priocntl/Makefile
#

PROG= priocntl

PRIOCNTLOBJ=	$(PROG).o subr.o

PRIOCNTLSRC= $(PRIOCNTLOBJ:%.o=%.c)
OBJS= $(PRIOCNTLOBJ) rt$(PROG).o ts$(PROG).o ia$(PROG).o
SRCS= $(OBJ:%.o=%.c)

include ../Makefile.cmd

CLASSD = $(ROOTLIB)/class
RTD = $(CLASSD)/RT
TSD = $(CLASSD)/TS
IAD = $(CLASSD)/IA
DIRS = $(CLASSD) $(RTD) $(TSD) $(IAD)

RTPROG = RT$(PROG)
TSPROG = TS$(PROG)
IAPROG = IA$(PROG)
ROOTRTPROG = $(RTD)/$(RTPROG)
ROOTTSPROG = $(TSD)/$(TSPROG)
ROOTIAPROG = $(IAD)/$(IAPROG)

$(ROOTUSRSBINPROG) := FILEMODE = 04555
$(ROOTUSRSBINPROG) := OWNER= root
$(ROOTUSRSBINPROG) := GROUP= root
$(DIRS) := FILEMODE = 0775
$(DIRS) := OWNER = root
$(DIRS) := GROUP = bin
CLOBBERFILES += $(RTPROG) $(TSPROG) $(IAPROG)

# installation rules
$(RTD)/% : %
	$(INS.file)

$(TSD)/% : %
	$(INS.file)

$(IAD)/% : %
	$(INS.file)

.KEEP_STATE:

all: $(PROG) $(RTPROG) $(TSPROG) $(IAPROG)

$(PROG): $(PRIOCNTLOBJ)
	$(LINK.c) $(PRIOCNTLOBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(RTPROG): rt$(PRIOCNTLOBJ)
	$(LINK.c) rt$(PRIOCNTLOBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(TSPROG): ts$(PRIOCNTLOBJ)
	$(LINK.c) ts$(PRIOCNTLOBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(IAPROG): ia$(PRIOCNTLOBJ)
	$(LINK.c) ia$(PRIOCNTLOBJ) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(DIRS) $(ROOTPROG) $(ROOTRTPROG) $(ROOTTSPROG) $(ROOTIAPROG)

$(DIRS):
	$(INS.dir)

clean:
	$(RM) $(OBJS)

lint:
	$(LINT.c) $(PRIOCNTLSRC)
	$(LINT.c) rt$(PRIOCNTLSRC)
	$(LINT.c) ts$(PRIOCNTLSRC)
	$(LINT.c) ia$(PRIOCNTLSRC)

include ../Makefile.targ
