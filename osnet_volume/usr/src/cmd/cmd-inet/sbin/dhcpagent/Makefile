#
#ident	"@(#)Makefile	1.6	99/08/25 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/sbin/dhcpagent/Makefile
#

PROG    = dhcpagent
LOCOBJS = adopt.o agent.o arp_check.o async.o bound.o class_id.o defaults.o \
	  dlpi_io.o dlprims.o event_handler.o inform.o init_reboot.o \
	  interface.o ipc_action.o packet.o release.o renew.o request.o \
	  select.o timer_queue.o util.o
COMDIR  = $(SRC)/common/net/dhcp
COMOBJS = ipv4_sum.o octet.o scan.o udp_sum.o

include ../../../Makefile.cmd

DFLTD   = $(ROOTETC)/default
DDIRS   = $(DFLTD)
ETCDFLTPROG = $(PROG:%=$(DFLTD)/%)
$(ETCDFLTPROG) := FILEMODE = 0444
$(ETCDFLTPROG) := OWNER = root
$(ETCDFLTPROG) := GROUP = sys

OBJS    = $(COMOBJS) $(LOCOBJS)
SRCS    = $(COMOBJS:%.o=$(COMDIR)/%.c) $(LOCOBJS:%.o=%.c)

POFILES = $(LOCOBJS:%.o=%.po)
XGETFLAGS += -a -x dhcpagent.xcl

#
# set CDEBUGFLAG to "-xO0 -g" either in the shell environment or
# makefile commandline before compiling to compile a debug version.
#

LINTFLAGS += -DDHCP_CLIENT -I$(COMDIR)
COPTFLAG  += $(CDEBUGFLAG) -DDHCP_CLIENT -I$(COMDIR)
LDLIBS    += -lsocket -lcmd -lc -lnsl -ldhcpagent -ldhcputil
LDFLAGS   += -dy -R/etc/lib $(LDFLAGS.cmd)

.KEEP_STATE:

#
# technically, since dhcpagent is statically linked against a number of
# libraries, we should rebuild when they change.
#

all:		$(PROG) $(PROG).dfl

install:	all $(ROOTSBINPROG) $(DDIRS) $(ETCDFLTPROG)

$(PROG):	$(OBJS)
		LD_OPTIONS=-I/etc/lib/ld.so.1	\
			$(LINK.c) -o $@ $(OBJS) -ldl -Bstatic $(LDLIBS)
		$(POST_PROCESS)

%.o:		$(COMDIR)/%.c
		$(COMPILE.c) $(CFLAGS) $<
		$(POST_PROCESS_O)

$(DDIRS):
		$(INS.dir)

$(DFLTD)/%:	%.dfl
		$(INS.rename)

$(POFILE):	$(POFILES)
		$(RM) $@; cat $(POFILES) > $@; $(RM) $(POFILES)
clean:
		$(RM) $(OBJS)

lint:		lint_SRCS

include ../../../Makefile.targ
