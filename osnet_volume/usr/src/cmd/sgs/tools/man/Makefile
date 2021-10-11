#
#ident	"@(#)Makefile	1.9	95/10/14 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
# Note, these man pages are not part of the product.

MAN1l=		elfdump.1l	lsd.1l		so425.1l	interface.1l \
		scoping.1l
MAN3t=		ld_concurrency.3t
MAN3l=		ld_support.3l

MANS=		$(MAN1l)  $(MAN3t) $(MAN3l)

include 	../../../../Makefile.master

ROOTDIR=	$(ROOT)/opt/SUNWonld/man
ROOT1lMAN=	$(MAN1l:%=$(ROOTDIR)/man1l/%)
ROOT3tMAN=	$(MAN3t:%=$(ROOTDIR)/man3t/%)
ROOT3lMAN=	$(MAN3l:%=$(ROOTDIR)/man3l/%)

ROOTMANS=	$(ROOT1lMAN)  $(ROOT3tMAN) $(ROOT3lMAN)

FILEMODE=	0444

.KEEP_STATE:
all:		$(MANS)

install:	all $(ROOTMANS)

$(ROOTDIR)/man1l/%:	%
		$(INS.file)

$(ROOTDIR)/man3t/%:	%
		$(INS.file)

$(ROOTDIR)/man3l/%:	%
		$(INS.file)

clobber clean lint:
