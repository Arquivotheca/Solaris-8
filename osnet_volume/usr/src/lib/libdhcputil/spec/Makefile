#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	99/05/21 SMI"
#
# lib/libdhcputil/spec/Makefile

# no sparcv9 build of libdhcputil
# if it is desirable to build one then uncomment the line below and
# delete the rest of the lines.
#
#include        $(SRC)/lib/Makefile.spec.arch

i386_ARCHITECTURES =	i386
sparc_ARCHITECTURES =	sparc

all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint

.KEEP_STATE:

all install clean clobber: $($(MACH)_ARCHITECTURES)

$($(MACH)_ARCHITECTURES): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

#
# This will make sure that any target not
# explicitly defined will not break the build.
#
# XXX pmake on intel does not like the following rules
# %: ignore_and_exit_quietly
# ignore_and_exit_quietly:
#
# So here it is manually
#
IGNORE=		_msg catalog install_h delete \
		package tcov debug private_h \
		check analyse test dynamic lint
$(IGNORE):

FRC:
