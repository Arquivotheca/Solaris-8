#
#ident	"@(#)Makefile	1.6	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/devmgmt/Makefile
#
# include global definitions
include ../Makefile.cmd

SUBDIRS= devattr devreserv getvol putdgrp \
         devfree getdev listdgrp getdgrp \
         putdev mkdtab data

TXTS= Prototype

all:=           TARGET= all
install:=       TARGET= install
clean:=         TARGET= clean
clobber:=       TARGET= clobber
lint:=          TARGET= lint

.KEEP_STATE:

all install clean clobber lint: $(SUBDIRS) $(TXTS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
