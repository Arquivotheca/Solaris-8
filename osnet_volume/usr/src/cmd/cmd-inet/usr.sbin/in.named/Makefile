#
#pragma ident	"@(#)Makefile	1.3	98/02/12 SMI"
#
# Copyright (c) 1996,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sbin/in.named/Makefile
#

PROG=
SUBDIRS=	named named-xfer tools

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint


.KEEP_STATE:

all: $(PROG) $(SUBDIRS)

install: all $(ROOTUSRSBINPROG) $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:

clean:	$(SUBDIRS)
	$(RM) $(OBJS)

clobber: $(SUBDIRS)
	$(RM) $(OBJS) $(PROG)

lint: $(SUBDIRS)
