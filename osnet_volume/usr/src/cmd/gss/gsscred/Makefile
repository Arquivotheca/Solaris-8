#
# Copyright (c) 1997-1999, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.7	99/07/18 SMI"
#
# cmd/gss/gsscred/Makefile

SBINPROG = gsscred

INCDIRS = -I. -I$(ROOT)/usr/include

LIBPATH = -L$(ROOT)/usr/lib

PROG= $(SBINPROG)

GSSCREDOBJS = gsscred.o gsscred_utils.o gsscred_xfn.o gsscred_file.o

OBJS	= $(GSSCREDOBJS)
SRCS	= $(OBJS:.o=.c)

include ../../Makefile.cmd

TEXT_DOMAIN = SUNW_OST_NETRPC
POFILE = $(PROG).po
POFILES = generic.po

ROOTBINPROG=		$(BINPROG:%=$(ROOTBIN)/%)

$(ROOTUSRSBIN)/gsscred :=	OWNER=	root
$(ROOTUSRSBIN)/gsscred :=	GROUP=	sys

COPTFLAG += -xs $(INCDIRS) $(LIBPATH)

LDLIBS += -lgss -lxfn -lnsl -lsocket -ldl

$(GPROGS) := CPPFLAGS += -DSYSV -DSunOS=50 

.KEEP_STATE:

all: $(PROG)

gsscred:	$(OBJS) 
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(ROOTBINPROG):
	$(INS.dir)

$(ROOTUSRSBIN)/%:	%
	$(INS.file)

install: all $(DIRS) $(ROOTBINPROG) $(ROOTUSRSBIN)/gsscred

install_h:

clean: 
	$(RM) $(OBJS)
	$(RM) $(PROG)

lint: lint_SRCS

include ../../Makefile.targ

$(POFILE): $(DERIVED_FILES) .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

generic.po: FRC
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext *.[ch]`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po

FRC:
