#
#ident	"@(#)Makefile	1.9	99/05/18 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sadm/dhcpmgr/lib/Makefile
#
LIBRARY=	dhcpmgr.a
VERS= 		.1
OBJECTS= 	dhcptab.o exception.o network.o optiondefs.o service.o \
		dd_misc.o dd_opt.o

# include library definitions
include $(SRC)/lib/Makefile.lib

# Override install location
ROOTLIBDIR=	$(ROOT)/usr/sadm/admin/dhcpmgr
ROOTLIBS=	$(LIBS:%=$(ROOTLIBDIR)/%)

LIBS=		$(DYNLIB)

CPPFLAGS += 	-I. -I$(SRC)/lib/libdhcp \
		-I../com/sun/dhcpmgr/bridge \
		-I$(JAVA_ROOT)/include \
		-I$(JAVA_ROOT)/include/solaris

LDLIBS += -ldhcp -lsocket -lresolv -lnsl -lc

CLOBBERFILES +=	$(LINTLIB) $(LINTOUT)

# definitions for i18n
POFILE= $(LIBRARY:%.a=%.po)

.KEEP_STATE:
.PARALLEL: $(OBJECTS)
.WAIT: $(DYNLIB)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install
lint :=		TARGET= lint

all: $(DYNLIB)

install: all $(ROOTLIBS)

clean clobber:

lint: $(LINTLIB)

_msg:   $(POFILE)

$(POFILE):
	$(RM) $@
	$(XGETTEXT) $(XGETFLAGS) `grep -l gettext *.[ch]`
	sed -e '/^# msg/d' -e '/^domain/d' messages.po > $@
	$(RM) messages.po

# include library targets
include $(SRC)/lib/Makefile.targ

FRC:
