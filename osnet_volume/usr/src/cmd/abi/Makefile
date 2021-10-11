#
# Copyright (c) 1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.2	99/05/14 SMI"
#
# cmd/abi/Makefile

.KEEP_STATE:
BUILD	=	spectrans	\
		ldcullstabs	\
		libstabspf	\
		apptrace



all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint
_msg 		:=	TARGET= catalog

all install clean clobber lint _msg: $(BUILD)

$(BUILD): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
