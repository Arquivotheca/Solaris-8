#
#ident	"@(#)Makefile	1.19	99/10/11 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/common/netinet/Makefile
#
# include global definitions
include ../../../Makefile.master

HDRS=	arp.h dhcp.h icmp6.h icmp_var.h if_ether.h igmp.h igmp_var.h \
	in.h in_pcb.h in_systm.h in_var.h ip.h ip6.h ip_icmp.h \
	ip_mroute.h ip_var.h pim.h tcp.h tcp_debug.h tcp_fsm.h tcp_seq.h \
	tcp_timer.h tcp_var.h tcpip.h udp.h udp_var.h

ROOTDIRS= $(ROOT)/usr/include/netinet

ROOTHDRS= $(HDRS:%=$(ROOT)/usr/include/netinet/%)

CHECKHDRS= $(HDRS:%.h=%.check)

$(ROOTDIRS)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
