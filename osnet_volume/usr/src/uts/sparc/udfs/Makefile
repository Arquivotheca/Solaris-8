#
# ident	"@(#)Makefile	1.3	99/02/13 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#	This makefile drives the production of the udfs file system
#	kernel module.
#
#	sparc implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= udfs
OBJECTS		= $(UDFS_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(UDFS_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_FS_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
#	Overrides.
#
MODSTUBS_DIR	 = $(OBJS_DIR)
$(MODSTUBS_O)	:= AS_CPPFLAGS += -DUDFS_MODULE
CLEANFILES	+= $(MODSTUBS_O)
CFLAGS		+= -v

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

putback:
	-cstyle -p ../../common/fs/udfs/udf_alloc.c
	-cstyle -p ../../common/fs/udfs/udf_bmap.c
	-cstyle -p ../../common/fs/udfs/udf_dir.c
	-cstyle -p ../../common/fs/udfs/udf_inode.c
	-cstyle -p ../../common/fs/udfs/udf_subr.c
	-cstyle -p ../../common/fs/udfs/udf_vfsops.c
	-cstyle -p ../../common/fs/udfs/udf_vnops.c
	-keywords ../../common/fs/udfs/udf_alloc.c
	-keywords ../../common/fs/udfs/udf_bmap.c
	-keywords ../../common/fs/udfs/udf_dir.c
	-keywords ../../common/fs/udfs/udf_inode.c
	-keywords ../../common/fs/udfs/udf_subr.c
	-keywords ../../common/fs/udfs/udf_vfsops.c
	-keywords ../../common/fs/udfs/udf_vnops.c
	-keywords Makefile
	-keywords ../../common/sys/fs/udf_inode.h
	-keywords ../../common/sys/fs/udf_volume.h
	-hdrchk ../../common/sys/fs/udf_inode.h
	-hdrchk ../../common/sys/fs/udf_volume.h



#
#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ
