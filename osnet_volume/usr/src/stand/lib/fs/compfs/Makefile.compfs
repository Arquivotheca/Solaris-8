#
#ident	"@(#)Makefile.compfs	1.6	97/01/31 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All Rights Reserved.
# 
# stand/lib/fs/compfs/Makefile.compfs
#
# Standalone Library COMPFS makefile
#
# This Makefile is included by ../../i386/Makefile and is used
# to build $(LIBCOMPFS).  The library is built in ../../i386.
#

COMPFSOBJ=		compfsops.o decompress.o ramfile.o
COMPFSOBJ_L=		compfsops.ln decompress.ln ramfile.ln
COMPFSSRC=		$(COMPFSOBJ:%.o=$(COMPFSDIR)/%.c)

$(LIBCOMPFS) :=		FS_OBJECTS=	$(COMPFSOBJ)
$(LIBCOMPFS_L) :=	FS_OBJECTS_L=	$(COMPFSOBJ_L)
$(LIBCOMPFS_L) :=	FS_SOURCES=	$(COMPFSSRC)

.PARALLEL:      	$(COMPFSOBJ:%=objs/%) $(COMPFSOBJ_L:%=objs/%)
.PARALLEL:      	$(COMPFSOBJ:%=kadbobjs/%)

