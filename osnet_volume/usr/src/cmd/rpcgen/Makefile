#
#ident	"@(#)Makefile	1.9	98/05/06 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/rpcgen/Makefile

PROG= rpcgen 


OBJS= rpc_clntout.o rpc_cout.o rpc_hout.o rpc_main.o rpc_parse.o \
  	  rpc_scan.o rpc_svcout.o rpc_tblout.o rpc_util.o rpc_sample.o
SRCS= $(OBJS:%.o=%.c)

include ../Makefile.cmd

LDLIBS += 

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../Makefile.targ
