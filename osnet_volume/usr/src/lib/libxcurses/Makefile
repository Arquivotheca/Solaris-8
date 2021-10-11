#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.15	00/09/14 SMI"
#
# lib/libxcurses/Makefile

include	../Makefile.lib

SUBDIRS = spec .WAIT $(MACH) $(BUILD64) $(MACH64)

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint


# definitions for install_h target
HDRS=		h/term.h h/curses.h h/unctrl.h

# extra clean-up work
CLOBBERFILES=	$(GENERATED)

.KEEP_STATE:

LIBS=		$(DYNLIB) $(LINTLIB)

all: $(GENERATED) .WAIT $(SUBDIRS)

install: all .WAIT $(SUBDIRS)

clean clobber: $(SUBDIRS)
	$(RM) $(GENERATED)

lint: $(SUBDIRS)

# still need to implement cstyle check of term.h
# do this when we go through and make this library cstyle clean.
check: $(HDRS)

$(MACH) $(MACH64) spec: $(GENERATED)
	@cd $@; pwd; $(MAKE) $(TARGET)


FRC:
