#
# ident	"@(#)Makefile	1.4	92/12/15 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/filter/postscript/postscript/Makefile
#

include         ../../../Makefile.lp

COMMONDIR=      ../common

TXTFILES =	README

FILES =				\
		aps.ps		\
		banner.ps	\
		baseline.ps	\
		color.ps	\
		dpost.ps	\
		draw.ps		\
		fatcourier.ps	\
		forms.ps	\
		postdaisy.ps	\
		postdmd.ps	\
		postmd.ps	\
		postplot.ps	\
		postprint.ps	\
		posttek.ps	\
		ps.requests

ROOTPOSTFILES=	$(FILES:%=$(ROOTLIBLPPOST)/%)

.KEEP_STATE:

all:		$(TXTFILES) $(FILES)

install:	all $(ROOTPOSTFILES)

clean clobber  strip lint:

