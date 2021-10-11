#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)Makefile	1.8	96/08/16 SMI"	/* SVr4.0 1.2	*/
# cmd/localedef/Makefile

include ../Makefile.cmd

SUBDIRS=chrtbl colltbl montbl

all:=           TARGET= all
install:=       TARGET= install
clean:=         TARGET= clean
clobber:=       TARGET= clobber

.KEEP_STATE:

all:	$(SUBDIRS) 

clobber clean install: $(SUBDIRS) 

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
