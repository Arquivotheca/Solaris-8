#
#ident	"@(#)Makefile	1.20	99/03/04 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/nfs/nfslog/Makefile

FSTYPE=		nfs
TYPEPROG=	nfslogd
ATTMK=		$(TYPEPROG)

include		../../Makefile.fstype

COMMON=		nfslog_config.o nfslogtab.o
LOCAL=		process_buffer.o fhtab.o nfslogd.o nfslog_elf.o \
		nfslog_trans.o nfslog_ipaddr.o readbuf.o dbtab.o \
		nfs_log_xdr.o buffer_list.o
OBJS=		$(LOCAL) $(COMMON)
SRCS=		$(LOCAL:%.o=%.c) $(COMMON:%.o=../lib/%.c)
LDLIBS +=	-lsocket -lnsl -lcmd
CFLAGS += 	-v

#
# Message catalog
#
POFILE= nfslog.po

catalog:        $(POFILE)

$(POFILE):      $(SRCS)
	$(RM) $@
	$(COMPILE.cpp) $(SRCS)   > $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS)        $(POFILE).i
	sed "/^domain/d"        messages.po     > $@
	$(RM) messages.po $(POFILE).i


DFLTD=		$(ROOTETC)/default
ETCDFLTPROG=	$(TYPEPROG:%=$(DFLTD)/%)

$(ETCDFLTPROG) := FILEMODE = $(LIBFILEMODE)

$(TYPEPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

$(DFLTD)/% : %.dfl
	$(INS.rename)

install:	all $(ETCDFLTPROG)

nfslog_config.o:	../lib/nfslog_config.c
		$(COMPILE.c) ../lib/nfslog_config.c

nfslogtab.o:	../lib/nfslogtab.c
		$(COMPILE.c) ../lib/nfslogtab.c

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

clean:     
	$(RM) $(OBJS)
