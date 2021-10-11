#
#ident	"@(#)Makefile	1.17	96/06/24 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG=		uname
STATPROG=	uname

# duplicate ROOTSBIN value needed for installation rule
ROOTSBIN=	$(ROOT)/sbin

$(ROOTSBIN)/%: %.static
	$(INS.rename)
	 
include ../Makefile.cmd

CFLAGS += -v

# This flag is being added only for SCO (x86) compatibility  
CFLAGS += $(iBCS2FLAG)

CCMODE=	-Xa

CLOBBERFILES +=	$(STATIC)

.KEEP_STATE:

all: $(PROG) $(STATIC) 

install: all $(ROOTPROG) $(ROOTSBINPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
