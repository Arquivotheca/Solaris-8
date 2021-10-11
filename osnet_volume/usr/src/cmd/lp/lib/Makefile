#
# ident	"@(#)Makefile	1.11	99/08/16 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/lp/lib/Makefile
#

SUBDIRS =		\
		access	\
		class	\
		filters	\
		forms	\
		lp	\
		msgs	\
		oam	\
		requests\
		secure	\
		systems	\
		printers\
		users


all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
strip :=	TARGET= strip

.KEEP_STATE:

all install:	$(SUBDIRS)

clean lint:	$(SUBDIRS)

clobber: $(SUBDIRS) local_clobber

strip:

local_clobber:
	$(RM) $(CLOBBERFILES)

_msg:
	@echo "Messages are made in usr/src/cmd/lp"

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
