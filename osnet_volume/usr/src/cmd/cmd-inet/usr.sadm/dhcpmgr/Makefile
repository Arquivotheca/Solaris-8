#
#ident	"@(#)Makefile	1.7	99/05/19 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.sadm/dhcpmgr/Makefile

include $(SRC)/Makefile.master

SUBDIRS =	com lib bin help

all	:=	TARGET= all
install	:=	TARGET= install
clean	:=	TARGET= clean
clobber	:=	TARGET= clobber
lint	:=	TARGET= lint
_msg	:=	TARGET= _msg

JARFILE= dhcpmgr.jar

JARFILES=	com/sun/dhcpmgr/*/*.class \
		com/sun/dhcpmgr/*/*.gif \
		com/sun/dhcpmgr/*/*.properties

ROOTDIR = $(ROOT)/usr/sadm/admin/dhcpmgr
ROOTDIRS= $(ROOT)/usr/sadm/admin $(ROOTDIR)

ROOTDIRJAR =   $(JARFILE:%=$(ROOTDIR)/%)

FILEMODE =	0444
DIRMODE =	0755

.KEEP_STATE:

all: $(SUBDIRS) $(JARFILE)

install: all $(ROOTDIRS) $(SUBDIRS) $(ROOTDIRJAR)

clean clobber: $(SUBDIRS)
	-$(RM) $(JARFILE)

lint:	$(SUBDIRS)

_msg:	$(SUBDIRS)

strip:

$(JARFILE): FRC
	$(JAR) cf $@ $(JARFILES) 2>/dev/null

$(ROOTDIR)/%:	%
	$(INS.file)

$(ROOTDIRS):
	$(INS.dir)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
