#
# ident	"@(#)Makefile	1.13	99/08/16 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/lp/model/Makefile
#

include			../Makefile.lp

SUBDIRS =		netpr

PROG =			lp.set		\
			lp.cat		\
			lp.tell		\
			drain.output

SRCS =			$(PROG:%=%.c)

OBJS =			$(PROG:%=%.o)

MODELS =		standard	\
			netstandard

MISC =			alert.proto


ROOTLIBLPMODEL=		$(ROOTLIBLP)/model
ROOTLIBLPBIN =		$(ROOTLIBLP)/bin

ROOTMISC =		$(MISC:%=$(ROOTLIBLPBIN)/%)
ROOTLPPROG =		$(PROG:%=$(ROOTLIBLPBIN)/%)
ROOTMODELS =		$(MODELS:%=$(ROOTLIBLPMODEL)/%)

CPPFLAGS =		-I$(LPINC) $(CPPFLAGS.master)

# conditional assignments
lp.tell :=			LDLIBS += $(LIBMSG) $(LIBLP) -lcurses
lp.set drain.output lp.cat :=	LDLIBS += $(LIBLP) -lcurses

$(ROOTMISC) :=		FILEMODE = 0444

.KEEP_STATE:

all:			$(PROG) $(MODELS) $(MISC) $(SUBDIRS)

install:		all .WAIT $(ROOTLPPROG) $(ROOTMODELS) $(ROOTMISC) \
			$(SUBDIRS)

$(ROOTLIBLPMODEL)/% $(ROOTLIBLPBIN)/%:	%
			$(INS.file)

_msg:
			@echo "Messages are made in usr/src/cmd/lp"

clean:			$(SUBDIRS)
			$(RM) $(OBJS)

clobber:		$(SUBDIRS) clean
			-$(RM) $(PROG) $(CLOBBERFILES)

strip:
			$(STRIP) $(PROG)

lint:
			$(LINT.c) $(SRCS) $(LDLIBS)

$(SUBDIRS) :	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
