#
#ident	"@(#)Makefile	1.21	97/12/23 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
#  /usr/src/cmd/lib/fs/ufs is the directory of all ufs specific commands
#  whose executable reside in $(INSDIR1) and $(INSDIR2).
#
#	NOTE:   ufsdump and ufsrestore have been merged into hsmdump and
#		hsmrestore.  The source is located in usr/src/cmd/backup.
#		When the Online Backup producted is no longer supported
#		they should be moved here again.
#

SUBDIR1=	clri edquota ff fsck fsdb fsirand ident labelit lockfs \
		ncheck newfs quot quota quotacheck quotaon \
		repquota tunefs
SUBDIR2= 	df fstyp mkfs mount volcopy
ROLLDIR=	roll_log
SUBDIRS=	$(ROLLDIR) $(SUBDIR1) $(SUBDIR2)

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
catalog:=       TARGET= catalog

# for messaging catalog
#
POFILE= ufs.po
POFILES= $(SUBDIR2:%=%/%.po)

.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all install:	$(ROLLDIR) .WAIT $(SUBDIR1) $(SUBDIR2)

catalog:        $(POFILE)

$(POFILE):      $(SUBDIR2)
	$(RM) 	$@
	cat     $(POFILES)      > $@

clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
