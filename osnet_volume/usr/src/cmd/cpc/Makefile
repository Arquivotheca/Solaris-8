#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/08/15 SMI"

include ../Makefile.cmd

SUBDIRS=	cputrack cpustat

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint
strip	:=	TARGET = strip
_msg	:=	TARGET = _msg

.KEEP_STATE:

all:	$(SUBDIRS)

install strip clean clobber lint _msg:	$(SUBDIRS)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
