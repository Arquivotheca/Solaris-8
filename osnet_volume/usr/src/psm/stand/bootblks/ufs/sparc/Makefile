#
#ident	"@(#)Makefile	1.3	95/02/14 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
#
# psm/stand/bootblks/ufs/sparc/Makefile
#

BASEDIR = ../..

include $(BASEDIR)/ufs/Makefile.ufs

SUBDIRS	= unix sun4c sun4m sun4d sun4u

all	:=	TARGET= all
install	:=	TARGET= install
clean	:=	TARGET= clean
clobber	:=	TARGET= clobber
lint	:=	TARGET= lint

.KEEP_STATE:

all: $(INSTALLBOOT) $(SUBDIRS)

install: $(USR_SBIN_INSTALLBOOT) $(SUBDIRS)

lint clean: $(SUBDIRS)

clobber: $(SUBDIRS)
	-$(RM) $(INSTALLBOOT)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

#
# Pattern matching rules for source in this directory
#
%: %.sh
	$(RM) $@
	cat $< > $@
	chmod +x $@
