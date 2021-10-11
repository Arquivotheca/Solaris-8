#
# ident	"@(#)Makefile	1.8	99/03/30 SMI"
#
# Copyright (c) 1989,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/lp/filter/postscript/filtdesc/Makefile
#

include		../../../Makefile.lp

ROOTETCLPFD=	$(ROOTETCLP)/fd

FILES=		catv.fd		\
		download.fd	\
		dpost.fd	\
		postdaisy.fd	\
		postdmd.fd	\
		postio.fd	\
		postior.fd	\
		postmd.fd	\
		postplot.fd	\
		postprint.fd	\
		postreverse.fd	\
		posttek.fd \
		postpages.fd \
		pr.fd

ROOTFILES =	$(FILES:%=$(ROOTETCLPFD)/%)

TXTS =		README 

FILEMODE =	644

.KEEP_STATE:

all:		$(TXTS) $(FILES)

install:	all $(ROOTETCLPFD) $(ROOTFILES)

$(ROOTETCLPFD) :
		$(INS.dir)

$(ROOTETCLPFD)/% :	%
		$(INS.file)

clean clobber strip lint:
