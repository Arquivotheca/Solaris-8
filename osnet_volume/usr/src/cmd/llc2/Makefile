#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	99/03/16 SMI"
#
# cmd/llc2/Makefile
#
# Makefile for top level LLC2 source directory
#

SUBDIRS=	bin etc
MSGDIRS=	bin

include 	../Makefile.cmd

all:=           TARGET= all
install:=       TARGET= install
clean:=         TARGET= clean
clobber:=       TARGET= clobber
lint:=          TARGET= lint
_msg:=		TARGET= _msg

.KEEP_STATE:

all:		$(SUBDIRS)

clean lint:	$(SUBDIRS)

clobber:	clean $(SUBDIRS)

install: 	$(SUBDIRS)

_msg:		$(MSGDIRS)

$(SUBDIRS): 	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
