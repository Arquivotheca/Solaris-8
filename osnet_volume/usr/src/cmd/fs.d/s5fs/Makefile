#
#ident "@(#)Makefile	1.2	93/02/17 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
#  /usr/src/cmd/lib/fs/s5fs is the directory of all s5fs specific commands
#  whose executable reside in $(INSDIR1) and $(INSDIR2).
#

SUBDIRS=	fsck fstyp mount ff

TXTS=		s5fs.mk

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

.KEEP_STATE:

all:	$(SUBDIRS) $(TXTS)

install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
