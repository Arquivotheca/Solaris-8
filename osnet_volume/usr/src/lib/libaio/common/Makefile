#
# Copyright (c) 1998, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.2	98/12/22 SMI"
#
# lib/libaio/common/Makefile

LINTSRC32= lintsrc32
LINTOUT32= lint32.out
LINTLIB32= $(LIBNAME)32
$(LINTSRC32):= LINTFLAGS +=

LINTSRC64= lintsrc64
LINTOUT64= lint64.out
LINTLIB64= $(LIBNAME)64
$(LINTSRC64):= LINTFLAGS64 += -errchk=longptr64 -fd -Xtransition=yes

lints	:	$(LINTSRC32) $(LINTSRC64)

$(LINTSRC32):	$$(SRCS)
	$(LINT.c) -o  $(LINTLIB32) $(SRCS) > $(LINTOUT32) 2>&1

$(LINTSRC64):	$$(SRCS)
	$(LINT64.c) -o  $(LINTLIB64) $(SRCS) > $(LINTOUT64) 2>&1

include ../Makefile.com

