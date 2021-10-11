#
# ident	"@(#)Makefile	1.1	99/07/02 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libmd5_psr/Makefile
#

include $(SRC)/Makefile.master

SUBDIRS= spec .WAIT $(MACH) $(BUILD64) $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install

.KEEP_STATE:

.PARALLEL: $(MACH) $(MACH64)

all clean clobber install: $(SUBDIRS)

spec $(MACH) $(MACH64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

#
# Empty targets.
# lint is an empty target since md5.c is already linted
# up and down in the kernel and a lint library is already
# created in lib/libmd5.
#
_msg install_h check lint:

FRC:
