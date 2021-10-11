#
# ident	"@(#)Makefile	1.6	92/12/17 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/troff/troff.d/tmac.d/Makefile
#

include 	../../../Makefile.cmd

ROOTTMAC =	$(ROOTSHLIB)/tmac

FILES =		acm.me an ansun ansun.tbl bib \
		chars.me deltext.me e eqn.me  \
		float.me footnote.me index.me local.me m \
		mmn mmt ms.acc ms.cov ms.eqn ms.ref ms.tbl \
		ms.ths ms.toc null.me refer.me s sh.me \
		tbl.me thesis.me tz.map v vgrind

FILEMODE =	644

ROOTFILES =	$(FILES:%=$(ROOTTMAC)/%)

.KEEP_STATE:

all:		$(FILES) 

install:	all $(ROOTTMAC) $(ROOTFILES)

$(ROOTTMAC) :
		$(INS.dir)

$(ROOTTMAC)/% :	%
		$(INS.file)

clean clobber lint strip :
