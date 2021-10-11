#
#ident	"@(#)Makefile	1.2	99/09/27 SMI"
#
# Copyright (c) 1998, by Sun Microsystems, Inc.
# All rights reserved.
#

PROG=slpd
CONF=slp.conf.example

include ../../../Makefile.master

# /etc installation rules
ROOTETCINET=    $(ROOT)/etc/inet
ROOTETCINIT=    $(ROOT)/etc/init.d

DIRS=	$(ROOTETCINIT) $(ROOTETCINET) \
	$(ROOT)/etc/rc0.d $(ROOT)/etc/rc1.d \
	$(ROOT)/etc/rcS.d $(ROOT)/etc/rc2.d

ROOTETCINETCONF=        $(CONF:%=$(ROOTETCINET)/%)
ROOTETCINITPROG=        $(PROG:%=$(ROOTETCINIT)/%)

# only root can execute slpd successfully, so add a hint with the FILEMODE
$(ROOTETCINITPROG) := FILEMODE = 744

$(ROOTETCINET)/%:	$(ROOTETCINET) %
	$(INS.file)

$(ROOTETCINIT)/%:	$(ROOTETCINIT) %
	$(INS.file)


CLOBBERFILES = $(ROOT)/etc/rc2.d/S72$(PROG) \
	$(ROOT)/etc/rc1.d/K41$(PROG) \
	$(ROOT)/etc/rcS.d/K41$(PROG) \
	$(ROOT)/etc/rc0.d/K41$(PROG) \
	$(ROOTETCINITPROG) \
	$(ROOTETCINETCONF)

# override Makefile.master's GROUP=bin definition
GROUP=sys

.KEEP_STATE:

all lint clean _msg:

install:	$(DIRS) $(ROOTETCINITPROG) $(ROOTETCINETCONF)
	$(RM) $(ROOT)/etc/rc2.d/S72$(PROG)
	$(LN) $(ROOTETCINIT)/$(PROG) $(ROOT)/etc/rc2.d/S72$(PROG)
	$(RM) $(ROOT)/etc/rc1.d/K41$(PROG)
	$(LN) $(ROOTETCINIT)/$(PROG) $(ROOT)/etc/rc1.d/K41$(PROG)
	$(RM) $(ROOT)/etc/rcS.d/K41$(PROG)
	$(LN) $(ROOTETCINIT)/$(PROG) $(ROOT)/etc/rcS.d/K41$(PROG)
	$(RM) $(ROOT)/etc/rc0.d/K41$(PROG)
	$(LN) $(ROOTETCINIT)/$(PROG) $(ROOT)/etc/rc0.d/K41$(PROG)

clobber:
	$(RM) $(CLOBBERFILES)

$(DIRS):
	$(INS.dir)
