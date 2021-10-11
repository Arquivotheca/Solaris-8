#	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.
#	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
#	UNIX System Laboratories, Inc.
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)Makefile	1.4	93/12/15 SMI"

#       Makefile for fdisk

PROG= fdisk

ROOTSBINLINK=	$(ROOTUSRSBIN)/$(PROG)

include ../Makefile.cmd

LDLIBS += -lcurses -ladm

LDFLAGS += -dn


all:	$(PROG)

install: all $(ROOTSBINPROG) $(ROOTSBINLINK)

$(ROOTSBINLINK):	FRC
	-$(RM) $@; $(SYMLINK) ../../sbin/$(PROG) $@

FRC:

clean:

include ../Makefile.targ
