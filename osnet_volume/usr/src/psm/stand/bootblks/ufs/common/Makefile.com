#
#ident	"@(#)Makefile.com	1.2	94/12/10 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/bootblks/ufs/common/Makefile.com
#

THISDIR = $(BASEDIR)/ufs/common

#
# Files that define the fs-reading capabilities of the C-based boot block
#
FS_C_SRCS = ufs.c

#
# Allow ufs-specific files to find ufs-specific include files
#
$(FS_C_SRCS:%.c=%.o)	:=	CPPINCS += -I$(THISDIR)

#
# Pattern-matching rules for source in this directory
#
%.o: $(THISDIR)/%.c
	$(COMPILE.c) -o $@ $<

%.ln: $(THISDIR)/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))
	
%.fcode: $(THISDIR)/%.fth
	$(TOKENIZE) $<
