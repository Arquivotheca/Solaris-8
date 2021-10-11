#
#ident	"@(#)Makefile	1.5	98/12/02 SMI"
#
# Copyright (c) 1982 by Sun Microsystems, Inc.
#
#  /usr/src/cmd/lib/fs/pcfs is the directory of all pcfs specific commands
#  whose executable reside in $(INSDIR1) and $(INSDIR2).
#

SUBDIR1=	mount ident fstyp
SUBDIR2=	mkfs
SUBDIRS=	$(SUBDIR1) $(SUBDIR2)

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
catalog:=       TARGET= catalog

# for messaging catalog
#
POFILE= pcfs.po
POFILES= $(SUBDIR2:%=%/%.po)

.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all install:	$(SUBDIRS)

catalog:        $(POFILE)

$(POFILE):      $(SUBDIR2)
	$(RM) 	$@
	cat     $(POFILES)      > $@

clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
