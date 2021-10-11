#
#ident	"@(#)Makefile	1.42	99/07/07 SMI"
#
# Copyright (c) 1995 - 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/autofs/Makefile

FSTYPE=		autofs
AUTO=		automount
MOUNT=		mount
VERS=		.2
FNSO=		fn.so$(VERS)
DFSHARES=	dfshares
SHARE=		share
UNSHARE=	unshare
LIBPROG=	$(AUTO) $(MOUNT) $(FNSO) $(DFSHARES) $(SHARE) $(UNSHARE)
TYPEPROG=	automountd

LINKINSTALL=	$(ROOTUSRSBIN)/$(AUTO)
LINKVALUE=	../lib/fs/$(FSTYPE)/$(AUTO)
MAPS=		auto_master auto_home
MAPINSTALL=	$(MAPS:%=$(ROOTETC)/%)
OTHERINSTALL=	$(MAPINSTALL) $(LINKINSTALL);
UNCHECKED_HDRS=	webnfs.h

include		../Makefile.fstype

$(MAPINSTALL)	:= OWNER= root
$(MAPINSTALL)	:= FILEMODE= 0644

REAL_COMMON=	debug_alloc.o
COMMON=		ns_generic.o ns_files.o ns_nisplus.o ns_nis.o \
		ns_ldap.o auto_mnttab.o auto_subr.o $(REAL_COMMON)

AUTOOBJS=	automount.o $(COMMON) fslib.o

MOUNTOBJS=	mount.o fslib.o $(REAL_COMMON)

LOCAL=		autod_main.o key_generic.o \
		autod_parse.o autod_mount.o autod_nfs.o nfs_cast.o \
		autod_autofs.o autod_xdr.o autod_readdir.o autod_lookup.o

TYPEOBJS=	$(LOCAL) $(COMMON) replica.o nfs_sec.o nfs_subr.o fslib.o \
		webnfs_xdr.o webnfs_client.o selfcheck.o

FNOBJS=		ns_fnmount.o ns_fnreaddir.o ns_fnutils.o

SHAREOBJS=	$(SHARESRCS:%.c=%.o)

UNSHAREOBJS=	$(UNSHARESRCS:%.c=%.o)

POFILE= autofs.po
GREP= egrep
SED= sed

DYNFLAGS=	-G -ztext -h $(FNSO)

$(AUTO) := 	LDLIBS += -lnsl -lsldap -ldl
$(TYPEPROG) :=	LDLIBS += -lrpcsvc -lsocket -lnsl -ldl -lthread -lsldap
$(TYPEPROG) :=	LDFLAGS += -R/usr/lib/fs/$(FSTYPE)
$(FNSO) :=	LDLIBS += -lxfn -lc
$(FNSO) :=	LDFLAGS += $(DYNFLAGS)

CFLAGS +=	-v -D_FILE_OFFSET_BITS=64 -o $@
$(FNOBJS) :=	CFLAGS += -K pic
CPPFLAGS=	-I. -I.. -I../nfs/lib $(CPPFLAGS.master) -D_REENTRANT \
			$(MALLOC_DEBUG)
OBJS=		$(AUTOOBJS) $(MOUNTOBJS) $(TYPEOBJS) $(FNOBJS)

AUTOSRCS=	automount.c $(COMMON:%.o=%.c) ../fslib.c
MOUNTSRCS=	mount.c ../fslib.c
TYPESRCS=	$(LOCAL:%.o=%.c) $(COMMON:%.o=%.c) \
		../nfs/lib/replica.c ../nfs/lib/nfs_sec.c \
		../nfs/lib/nfs_subr.c ../fslib.c ../nfs/lib/selfcheck.c
FNSRCS=		$(FNOBJS:%.o=%.c)
SHARESRCS=	$(SHARE:%=%.c)
UNSHARESRCS=	$(UNSHARE:%=%.c)
DFSHARESSRCS=	$(DFSHARES:%=%.sh)
SRCS=		$(OBJS:%.o=%.c)

ns_generic.o :=	CPPFLAGS += -DFN_SO=\"$(FNSO)\"
nfs_sec.o :=	CPPFLAGS += -DWNFS_SEC_NEGO

$(AUTO):	$(AUTOOBJS)
		$(LINK.c) -o $@ $(AUTOOBJS) $(LDLIBS)
		$(POST_PROCESS)

$(MOUNT):	$(MOUNTOBJS)
		$(LINK.c) -o $@ $(MOUNTOBJS) $(LDLIBS)
		$(POST_PROCESS)

$(TYPEPROG):	webnfs.h $(TYPEOBJS)
		$(LINK.c) -o $@ $(TYPEOBJS) $(LDLIBS)
		$(POST_PROCESS)

$(FNSO):	$(FNOBJS)
		$(LINK.c) -o $@ $(FNOBJS) $(LDLIBS)
		$(POST_PROCESS_SO)

$(SHARE):	$(SHAREOBJS)
		$(LINK.c) -o $@ $(SHAREOBJS)
		$(POST_PROCESS)

$(UNSHARE):	$(UNSHAREOBJS)
		$(LINK.c) -o $@ $(UNSHAREOBJS)
		$(POST_PROCESS)

DUMP_PROG=malloc_dump
BUILDDIR=tmp
DUMP_PROTO=$(BUILDDIR)/$(DUMP_PROG)_client
XFILE=$(BUILDDIR)/malloc_dump.x
XLINE= "program DUMP { version VERS \
	{ void DUMP_IT(void) = 1000000; } = 2; } = 100099;"

$(XFILE):	$(BUILDDIR)
		$(RM) $(XFILE)
		echo $(XLINE) > $(XFILE)
		
$(BUILDDIR):
		$(RM) -r $(BUILDDIR)
		mkdir $(BUILDDIR)

$(DUMP_PROTO):	$(BUILDDIR) $(XFILE)
		cd $(BUILDDIR); $(RM) *.[ch] makefile.malloc_dump ; rpcgen -a `basename $(XFILE)
		cd $(BUILDDIR); make -f makefile.malloc_dump `basename $@`

$(DUMP_PROG):	$(DUMP_PROTO)
		$(RM) $@
		cp $(DUMP_PROTO) $@

clean_dump:
		$(RM) -r $(BUILDDIR)
		$(RM) $(DUMP_PROG)
$(ROOTUSRSBIN)/$(AUTO):
		$(RM) $@; $(SYMLINK) $(LINKVALUE) $@

fslib.o:	../fslib.c
		$(COMPILE.c) ../fslib.c

replica.o:	../nfs/lib/replica.c
		$(COMPILE.c) ../nfs/lib/replica.c

nfs_sec.o:	../nfs/lib/nfs_sec.c
		$(COMPILE.c) ../nfs/lib/nfs_sec.c

nfs_subr.o:	../nfs/lib/nfs_subr.c
		$(COMPILE.c) ../nfs/lib/nfs_subr.c

selfcheck.o:	../nfs/lib/selfcheck.c
		$(COMPILE.c) ../nfs/lib/selfcheck.c


webnfs_xdr.c:	webnfs.x
	$(RPCGEN) -C -c -o $@ webnfs.x

webnfs_client.c:	webnfs.x
	$(RPCGEN) -C -l -o $@ webnfs.x

webnfs.h:	webnfs.x
	$(RPCGEN) -C -h -o $@ webnfs.x

webnfs.x:	../nfs/lib/webnfs.x
	$(RM) webnfs.x
	cp ../nfs/lib/webnfs.x .

catalog: $(POFILE)

$(POFILE):
	$(RM) messages.po
	$(SED) -e 's/pr_msg/gettext/' `$(GREP) -l "pr_msg|gettext" *.[ch]` | \
	$(XGETTEXT) $(XGETFLAGS) -
	$(SED) -e '/^# msg/d' -e '/^domain/d' < messages.po > $@
	$(RM) messages.po

lint:
	$(LINT.c) $(AUTOSRCS)
	$(LINT.c) $(MOUNTSRCS)
	$(LINT.c) $(TYPESRCS)
	$(LINT.c) $(FNSRCS)

clean:	clean_dump
	$(RM) $(OBJS) webnfs_xdr.c webnfs_client.c webnfs.h webnfs.x
