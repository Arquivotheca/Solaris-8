#
#pragma ident   "@(#)Makefile 1.2     97/12/05 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sbin/in.named/tools/named-bootconf/Makefile
#
TOOL=		named-bootconf
PROG=		$(TOOL) 

include ../../../../../Makefile.cmd

SRCS=		$(OBJS:%.o=%)

#all:		TARGET= all
#install:=	TARGET= install
#clean:=		TARGET= clean
#clobber:=	TARGET= clobber
#lint:=		TARGET= lint

CLEANFILES += $(OBJS)

all: $(PROG)

.KEEP_STATE:

clean: $(SUBDIRS)
clobber: $(SUBDIRS)

install: all $(ROOTUSRSBINPROG) $(SUBDIRS) 


$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

include ../../../../../Makefile.targ
FRC:
