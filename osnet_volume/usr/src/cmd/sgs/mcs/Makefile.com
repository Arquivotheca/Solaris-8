#
#ident	"@(#)Makefile.com	1.5	98/10/16 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.

PROG=		mcs
STRIPFILE=	strip

ROOTLINKS=	$(ROOTCCSBIN)/$(STRIPFILE)

include		$(SRC)/cmd/Makefile.cmd
include		$(SRC)/cmd/sgs/Makefile.com

# avoid bootstrap problems
MCS=		/usr/ccs/bin/mcs

OBJS=		main.o		file.o		utils.o		global.o \
		message.o

LLDFLAGS =	'-R$$ORIGIN/../../lib'
LLDFLAGS64 =	'-R$$ORIGIN/../../../lib/$(MACH64)'
LDFLAGS +=	$(LLDFLAGS)
CPPFLAGS=	-I../../include -I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys $(CPPFLAGS.master)
CONVFLAG =	-L../../libconv/$(MACH)
CONVFLAG64 =	-L../../libconv/$(MACH64)
LDLIBS +=	$(CONVFLAG) -lconv -lelf $(INTLLIB)
LINTFLAGS =	-mx -erroff=E_SUPPRESSION_DIRECTIVE_UNUSED $(LDLIBS)
LINTFLAGS64 =	-mx -errchk=longptr64 $(LDLIBS)

SRCS=		$(OBJS:%.o=../common/%.c)

CLEANFILES +=	$(OBJS) $(LINTOUT)


# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6

package :=	INTLLIB += /usr/lib/libintl.so.1
