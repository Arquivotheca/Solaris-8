#
#ident	"@(#)Makefile	1.1	98/01/07 SMI"
#
# Copyright (c) 1994-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/cpr/sparcv9/Makefile
#

SUBDIRS = sun4u

all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint
clean.lint	:=	TARGET= clean.lint

.KEEP_STATE:

all install clean clobber lint clean.lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
