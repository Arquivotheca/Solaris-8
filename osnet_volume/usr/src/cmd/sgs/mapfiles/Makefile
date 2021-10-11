#
#ident	"@(#)Makefile	1.1	98/04/24 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#
# cmd/sgs/mapfiles/Makefile
#

include		../../../Makefile.master

SUBDIRS=	$(MACH)
$(BUILD64)SUBDIRS += $(MACH64)

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
package:=	TARGET= package

.KEEP_STATE:

all clean clobber lint package:

install: $(SUBDIRS)

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
