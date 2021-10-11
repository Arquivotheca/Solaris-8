#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	99/09/20 SMI"
#
# ucblib/librpcsoc/sparcv9/Makefile

MAPDIR=	../spec/sparcv9
include ../Makefile.com
include $(SRC)/lib/Makefile.lib.64

DYNFLAGS += $(DYNFLAGS64)
BUILD.SO= $(CC) $(CFLAGS) -o $@ -G $(DYNFLAGS) $(PICS) \
	-L $(ROOT)/usr/ucblib/$(MACH64) -L $(ROOT)/usr/lib/$(MACH64) $(LDLIBS)

BUILD.s=        $(AS) $(ASFLAGS) -xarch=v9 -D__sparcv9 $< -o $@

LIBS=	$(DYNLIB) $(LINTLIB)

.KEEP_STATE:

all:	$(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64)
