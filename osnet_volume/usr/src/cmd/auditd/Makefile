#
# Makefile for auditd
#

PROG = auditd

include ../Makefile.cmd

TEXT_DOMAIN=SUNW_OST_OSCMD

LDLIBS += -lbsm -lsocket -lnsl

OBJS = auditd.o flock.o
SRCS = auditd.c flock.c

.KEEP_STATE:

all: $(PROG)
install: all $(ROOTUSRSBINPROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

clean:
	rm -f $(OBJS) $(PROG).po 

include ../Makefile.targ
