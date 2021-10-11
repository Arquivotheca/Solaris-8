#
#ident	"@(#)Makefile	1.5	94/01/21 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

SUBDIRS=	log strcmd

include ../Makefile.cmd

OWNER=	root
GROUP=	sys

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

.KEEP_STATE:

all:	$(SUBDIRS) 

install: all $(SUBDIRS)

clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
