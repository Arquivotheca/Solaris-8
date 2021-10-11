#
#ident	"@(#)Makefile	1.3	93/03/09 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/asppp/Makefile
#
# Makefile for top level AS PPP source directory
#

SUBDIRS=	libconn aspppd aspppls etc 


include 	../Makefile.cmd

all:=           TARGET= all
install:=       TARGET= install
clean:=         TARGET= clean
clobber:=       TARGET= clobber
lint:=          TARGET= lint

.KEEP_STATE:

all:		$(SUBDIRS)

clean lint:	$(SUBDIRS)

clobber:	clean $(SUBDIRS)

install: 	$(SUBDIRS)

$(SUBDIRS): 	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
