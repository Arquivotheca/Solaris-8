#
#ident	"@(#)Makefile	1.3	97/12/08 SMI"
#
# Copyright (c) 1993,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/lex/Makefile
#

include		../../../Makefile.master

SUBDIRS=	$(MACH) $(BUILD64) $(MACH64)

# conditional statements

all :=		TARGET= all
install :=	TARGET= install
install_lib :=	TARGET= install_lib
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

.KEEP_STATE:

all clean clobber install install_lib lint:	$(SUBDIRS)


$(MACH) $(MACH64): FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
