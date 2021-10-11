#
#ident	"@(#)Makefile.com	1.2	94/12/10 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/bootblks/hsfs/common/Makefile.com
#

THISDIR = $(BASEDIR)/hsfs/common

#
# Files that define the fs-reading capabilities of the C-based boot block
#
# uncomment following line if want to build big bootblk
# FS_C_SRCS	= hsfs.c
# uncomment following line if want to build small bootblk
FS_C_SRCS	= hsfs_small.c

#
# Allow hsfs-specific files to find hsfs-specific include files
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
