#
# ident	"@(#)Makefile.com	1.2	99/09/10 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libmd5_psr/Makefile.com
#

#
# Create default so empty rules don't confuse make
#
CLASS= 32

LIBRARY= libmd5_psr.a
VERS= .1

OBJECTS= md5.o
COMMON= $(SRC)/common/md5

include $(SRC)/lib/Makefile.lib
include $(SRC)/Makefile.psm

#
# Macros to help build the shared object
#
LIBS= $(DYNLIB)
LDLIBS += -lc
CPPFLAGS += -D__RESTRICT

#
# Macros for the mapfile. Other makefiles need to include this file
# after setting MAPDIR
#
MAPFILE=	$(MAPDIR)/mapfile-$(PLATFORM)
DYNFLAGS +=	-M $(MAPFILE)
CLOBBERFILES +=	$(MAPFILE)

#
# Used when building links in /usr/platform/$(PLATFORM)/lib 
#
LINKED_PLATFORMS	= SUNW,Ultra-1
LINKED_PLATFORMS	+= SUNW,Ultra-4
LINKED_PLATFORMS	+= SUNW,Ultra-250
LINKED_PLATFORMS	+= SUNW,Ultra-Enterprise
LINKED_PLATFORMS	+= SUNW,Ultra-Enterprise-10000

.KEEP_STATE:
