#
# ident	"@(#)Makefile.com	1.1	99/09/12 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/flashprom/Makefile.com
#

#
# Used when building links in /usr/platform/$(PLATFORM)/lib 
#
LINKED_PLATFORMS	= SUNW,Ultra-1
LINKED_PLATFORMS	+= SUNW,Ultra-4
LINKED_PLATFORMS	+= SUNW,Ultra-250
LINKED_PLATFORMS	+= SUNW,Ultra-Enterprise
LINKED_PLATFORMS	+= SUNW,Ultra-Enterprise-10000
