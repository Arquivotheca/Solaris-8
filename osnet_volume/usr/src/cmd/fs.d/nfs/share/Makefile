#
#ident	"@(#)Makefile	1.21	99/02/21 SMI"
#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
#

FSTYPE=		nfs
LIBPROG=	share
ATTMK=		$(LIBPROG)

COMMON=		nfs_sec.o sharetab.o nfslog_config.o nfslogtab.o
LOCAL=		$(LIBPROG).o issubdir.o
OBJS=		$(LOCAL) $(COMMON)
SRCS=		$(LOCAL:%.o=%.c) $(COMMON:%.o=../lib/%.c)
OTHERINSTALL=	$(ROOTETC)/dfs/fstypes

#
# Message catalog
#
POFILE= share.po


catalog: $(POFILE)

$(POFILE): $(SRCS)
	$(RM) $@
	$(COMPILE.cpp) $(SRCS)   > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	sed "/^domain/d"        messages.po     > $@
	$(RM) $(POFILE).i messages.po

include		../../Makefile.fstype

LDLIBS +=	-lnsl

$(ROOTETC)/dfs/fstypes :=	FILEMODE= 644
$(ROOTETC)/dfs/fstypes :=	OWNER= root
$(ROOTETC)/dfs/fstypes :=	GROUP= root

$(ROOTETC)/dfs/%: %
	$(INS.file)

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

nfs_sec.o:	../lib/nfs_sec.c
		$(COMPILE.c) ../lib/nfs_sec.c

sharetab.o:	../lib/sharetab.c
		$(COMPILE.c) ../lib/sharetab.c

nfslog_config.o:	../lib/nfslog_config.c
		$(COMPILE.c) ../lib/nfslog_config.c

nfslogtab.o:	../lib/nfslogtab.c
		$(COMPILE.c) ../lib/nfslogtab.c

lint:	lint_SRCS

clean:     
	$(RM) $(OBJS)
