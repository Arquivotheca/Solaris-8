#
#ident	"@(#)Makefile	1.14	99/09/07 SMI"
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/common/inet/Makefile
#
# include global definitions
include ../../../Makefile.master

HDRS=	arp.h common.h ip.h ip6.h ipsecah.h ipsecesp.h ipsec_conf.h \
	ipsec_info.h ip_if.h ip_ire.h ip_multi.h ip_ndp.h ip_rts.h keysock.h \
	led.h mi.h mib2.h nd.h optcom.h sadb.h sha1.h sha1_consts.h snmpcom.h \
	tcp.h tcp_sack.h tun.h

ROOTDIRS= $(ROOT)/usr/include/inet

ROOTHDRS= $(HDRS:%=$(ROOT)/usr/include/inet/%)

CHECKHDRS= $(HDRS:%.h=%.check)

$(ROOTDIRS)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)

