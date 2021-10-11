#
#ident	"@(#)Makefile	1.22	99/03/04 SMI"
#
# Copyright (c) 1989,1996,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/nfs/Makefile
#
#  cmd/fs.d/nfs is the directory of all nfs specific commands
#  whose executable reside in $(INSDIR1) and $(INSDIR2).
#

SUBDIR1=	exportfs nfsd rquotad unshare \
		statd nfsstat mountd dfshares \
		nfsfind
SUBDIR2=	clear_locks lockd umount showmount \
		share mount dfmounts nfslog
SUBDIR3=	etc
SUBDIRS=	$(SUBDIR1) $(SUBDIR2) $(SUBDIR3)

# for messaging catalog files
#
POFILES=        $(SUBDIR2:%=%/%.po)
POFILE=         nfs.po

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
catalog:=       TARGET= catalog

.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all install clean clobber lint: $(SUBDIRS)

catalog: $(SUBDIR2)
	$(RM) $(POFILE)
	cat $(POFILES) > $(POFILE)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
