#
# Copyright (c) 1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.8	99/01/25 SMI"
#
# sgs/libdl/Makefile

include		$(SRC)/Makefile.master

all install clean clobber: spec $(BUILD64) .WAIT $(SPEC64)

include		$(SRC)/cmd/sgs/Makefile.sub

$(SPEC) $(SPEC64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)
