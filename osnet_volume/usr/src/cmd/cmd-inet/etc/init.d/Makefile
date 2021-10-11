#
# Copyright (c) 1996-1997, 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.12	99/08/23 SMI"

PROG1=	inetinit
PROG2=	inetsvc
PROG3=	dhcp
PROG4=	ncalogd
PROG5=	ncakmod
PROG6=	dhcpagent
PROG=	$(PROG1) $(PROG2) $(PROG3) $(PROG4) $(PROG5) $(PROG6)

include ../../../Makefile.cmd

STARTINET1= $(ROOTETC)/rc2.d/S69inet
STARTINET2= $(ROOTETC)/rc2.d/S72inetsvc
STARTINET3= $(ROOTETC)/rc2.d/S94ncalogd
STARTINET4= $(ROOTETC)/rcS.d/S42ncakmod
STOPINET1=  $(ROOTETC)/rc0.d/K90dhcpagent

DFLTD= $(ROOTETC)/default
ETCDFLTPROG = $(PROG1:%=$(DFLTD)/%)
$(ETCDFLTPROG) := FILEMODE = $(LIBFILEMODE)
$(ETCDFLTPROG) := GROUP = sys

INITD= $(ROOTETC)/init.d
DIRS= $(INITD) $(ROOTETC)/rcS.d $(ROOTETC)/rc2.d $(ROOTETC)/rc3.d $(DFLTD)
FILEMODE= 0744
OWNER= root
GROUP= sys

ETCINITPROG=	$(PROG:%=$(INITD)/%)

.KEEP_STATE:

all: $(PROG)

install: all $(DIRS) $(ETCINITPROG) $(ETCDFLTPROG) ln_PROG

$(INITD)/% : %
	$(INS.file)

$(DFLTD)/% : %
	$(MV) $(PROG1) $(PROG1).save
	$(LN) $(PROG1).dfl $(PROG1)
	$(INS.file)
	$(RM) $(PROG1)
	$(MV) $(PROG1).save $(PROG1)

ln_PROG :
	@for dir in rc0.d rcS.d rc1.d; do \
		echo $(LN) $(INITD)/$(PROG1) $(ROOTETC)/$$dir/K43inet; \
		$(RM) $(ROOTETC)/$$dir/K43inet; \
		$(LN) $(INITD)/$(PROG1) $(ROOTETC)/$$dir/K43inet; \
		echo $(LN) $(INITD)/$(PROG2) $(ROOTETC)/$$dir/K42inetsvc; \
		$(RM) $(ROOTETC)/$$dir/K42inetsvc; \
		$(LN) $(INITD)/$(PROG2) $(ROOTETC)/$$dir/K42inetsvc; \
		echo $(LN) $(INITD)/$(PROG4) $(ROOTETC)/$$dir/K34ncalogd; \
		$(RM) $(ROOTETC)/$$dir/K34ncalogd; \
		$(LN) $(INITD)/$(PROG4) $(ROOTETC)/$$dir/K34ncalogd; \
	done
	-$(RM) $(STARTINET1)
	$(LN) $(INITD)/$(PROG1) $(STARTINET1)
	-$(RM) $(STARTINET2)
	$(LN) $(INITD)/$(PROG2) $(STARTINET2)
	-$(RM) $(STARTINET3)
	$(LN) $(INITD)/$(PROG4) $(STARTINET3)
	-$(RM) $(STARTINET4)
	$(LN) $(INITD)/$(PROG5) $(STARTINET4)
	-$(RM) $(STOPINET1)
	$(LN) $(INITD)/$(PROG6) $(STOPINET1)

$(DIRS):
	$(INS.dir)

clean:

lint:

include ../../../Makefile.targ
