#
#ident	"@(#)Makefile	1.5	99/09/20 SMI"
#
# Copyright (c) 1994-1999 by Sun Microsystems, Inc.
# All rights reserved.
#

include ../Makefile.cmd

SUBDIRS =	\
	pcred	\
	pfiles	\
	pflags	\
	pldd	\
	pmap	\
	prun	\
	psig	\
	pstack	\
	pstop	\
	ptime	\
	ptree	\
	pwait	\
	pwdx

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean
clobber	:=	TARGET = clobber
lint	:=	TARGET = lint

.KEEP_STATE:

.PARALLEL: $(SUBDIRS)

all install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) -f ../Makefile.ptool $(TARGET)

FRC:
