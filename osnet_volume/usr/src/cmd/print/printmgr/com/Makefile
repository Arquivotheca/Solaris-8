#
# ident	"@(#)Makefile	1.1	99/03/16 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# Makefile for print manager
#
include		$(SRC)/cmd/Makefile.cmd

SUBDIRS =	sun

all	:=	TARGET= all
install	:=	TARGET= install
clean	:=	TARGET= clean
clobber	:=	TARGET= clobber
lint	:=	TARGET= lint
_msg	:=	TARGET= _msg

all install clean clobber lint _msg: $(SUBDIRS)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
