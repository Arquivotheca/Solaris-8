#
#ident	"@(#)Makefile	1.1	99/01/25 SMI"
#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/abi/daa2x/backends/common/Makefile

i386_ARCHITECTURES = i386
sparc_ARCHITECTURES = sparc

all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint

.KEEP_STATE:

all install clean clobber lint: $($(MACH)_ARCHITECTURES)

$($(MACH)_ARCHITECTURES): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

