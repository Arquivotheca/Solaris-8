#
#ident	"@(#)Makefile	1.9	93/03/27 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
#  /usr/src/cmd/lib/fs/hsfs is the directory of all ufs specific commands
#  whose executable reside in $(INSDIR1) and $(INSDIR2).
#

SUBDIR1=        mkproto labelit ident
SUBDIR2=        mount fstyp
SUBDIRS=        $(SUBDIR1) $(SUBDIR2)

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
catalog:=       TARGET= catalog

# for messaging catalog
#
POFILE= hsfs.po
POFILES= $(SUBDIR2:%=%/%.po)


.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all:	$(SUBDIRS) 

catalog:        $(SUBDIR2)
	$(RM) $(POFILE)
	cat $(POFILES) > $(POFILE)

install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
