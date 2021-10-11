#
#ident	"@(#)Makefile	1.17	97/12/08 SMI"
#
# Copyright (c) 1991-1993, 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/syslogd/Makefile
#

PROG= 		syslogd

include ../Makefile.cmd

SUBDIRS=	$(MACH)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

.KEEP_STATE:

all:	$(SUBDIRS)

clean clobber lint:	$(SUBDIRS)

install:	$(SUBDIRS)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

include ../Makefile.targ

