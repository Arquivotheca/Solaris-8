#
# ident	"@(#)Makefile	1.4	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/crontab/Makefile
#

include 		../Makefile.lp

ROOTCRONTABS =		$(ROOT)/var/spool/cron/crontabs

PROG =			lp
ROOTCRONTABPROG =	$(PROG:%=$(ROOTCRONTABS)/%)


OWNER =			root
GROUP =			root
FILEMODE =		444

.KEEP_STATE:

all :			$(PROG)

install:		all $(ROOTCRONTABPROG)

$(ROOTCRONTABS)/% :	%
			$(INS.file)

clobber clean strip lint:
