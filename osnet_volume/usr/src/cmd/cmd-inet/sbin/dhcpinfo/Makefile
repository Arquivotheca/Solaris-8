#
#ident	"@(#)Makefile	1.2	99/08/18 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/sbin/dhcpinfo/Makefile
#

PROG    = dhcpinfo
OBJS    = $(PROG).o

include ../../../Makefile.cmd

#
# The PKT_LIST structure in <netinet/dhcp.h> is different depending on
# whether DHCP_CLIENT is defined.  Although dhcpinfo doesn't use the
# PKT_LIST structure anywhere, it includes <netinet/dhcp.h>, which is
# also included by libdhcpagent.so.  Lint will check all function
# prototypes and the fields in all of the structures in those
# prototypes, and since there are prototypes in <netinet/dhcp.h> that
# take a PKT_LIST * as an argument, dhcpinfo must lint with the same
# defines as libdhcpagent.
# 
LINTFLAGS += -DDHCP_CLIENT

LDLIBS  += -ldhcpagent -ldhcputil -lsocket -lnsl -lc
LDFLAGS += -dy -R/etc/lib $(LDFLAGS.cmd)

.KEEP_STATE:

all:		$(PROG)

$(PROG):	$(OBJS)
	  	LD_OPTIONS=-I/etc/lib/ld.so.1	\
			$(LINK.c) -o $@ $(OBJS) -ldl -Bstatic $(LDLIBS)
	  	$(POST_PROCESS)

install:	all $(ROOTSBINPROG)

clean:
	  	$(RM) $(OBJS)

lint:		lint_PROG

include ../../../Makefile.targ
