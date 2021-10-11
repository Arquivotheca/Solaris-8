#
#ident	"@(#)Makefile	1.5	93/07/20 SMI"	/* 	*/
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/fs.d/lofs/Makefile
#
#  cmd/fs.d/lofs is the directory of all lofs specific commands
#  whose executable reside in $(INSDIR1) and $(INSDIR2).
#

SUBDIRS=	mount

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
