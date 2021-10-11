#
#ident	"@(#)Makefile	1.12	98/11/04 SMI"
#
# Copyright (c) 1989,1998 by Sun Microsystems, Inc.
#

PROG= diff diffh
SRCS=	$(PROG:%=%.c)
DIFF= diff
DIFFH= diffh

include ../Makefile.cmd
#
# for messaging catalog
#
POFILE= diff.po
DCFILE= diff.dc
#XGETFLAGS += -a -x diff.xcl

ROOTBINDIFF = $(DIFF:%=$(ROOTBIN)/%)
ROOTLIBDIFFH = $(DIFFH:%=$(ROOTLIB)/%)

.KEEP_STATE:

all: $(PROG) 

#
# Use private rule
#
$(POFILE):
	$(RM) $@
	$(COMPILE.cpp)   $(SRCS) > diff.po.i
	$(XGETTEXT) $(XGETFLAGS) diff.po.i
	sed "/^domain/d"        messages.po     > $@
	$(RM)  diff.po.i messages.po

$(DCFILE):
	$(RM) $@
	$(COMPILE.cpp)   $(SRCS) > diff.dc.i
	$(XGETTEXT) -c TRANSLATION_NOTE_FOR_DC -t diff.dc.i
	sed "/^domain/d"        messages.po     > $@
	$(RM)  diff.dc.i messages.po

install: all $(ROOTBINDIFF) $(ROOTLIBDIFFH)

clean:

lint: lint_SRCS

include ../Makefile.targ
