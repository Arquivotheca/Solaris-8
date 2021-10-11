#
# Copyright (c) 1994,1996 by SunSoft, Inc.
# All rights reserved.
#
# "@(#)%M 1.9     99/07/12 SunSoft"
#
# cmd/print/Makefile
#

include ../Makefile.cmd

JAVA_SUBDIRS = printmgr

PRINT_SUBDIRS  =		\
		scripts		\
		lpget		\
		lpset		\
		lp		\
		lpstat		\
		cancel		\
		lpmove		\
		conv_fix	\
		gateway	

SUBDIRS = $(PRINT_SUBDIRS) $(JAVA_SUBDIRS)

ROOTDIRS = 	$(ROOTLIB)/print \
		$(ROOTLIB)/print/bsd-adaptor

all	:=	TARGET= all
install :=	TARGET= install
clean	:=	TARGET= clean
clobber	:=	TARGET= clobber
lint	:=	TARGET= lint
strip	:=	TARGET= strip
_msg	:=      TARGET = _msg

# For testing message catalogs
_msg_test:=	TARGET = _msg_test

POFILE= print.po

.KEEP_STATE:

all install:	$(ROOTDIRS) $(SUBDIRS)

#
# We define our own definition for _msg here because most of these
# commands have the same PROG names as their counterparts in
# cmd/lp. Using the _msg rule defined in Makefile.cmd would 
# result in clobbering the cmd/lp message files.
# To get around this we will define one message file "print.po"
# for these commands (except java printmgr). To build
# this file we find all of the .c files and run xgettext on them.
# Then concatenate this with the scripts.po file.
#
_msg:   $(MSGDOMAIN) scripts $(JAVA_SUBDIRS)
	@$(RM)	$(POFILE)
	$(XGETTEXT) -s `/bin/find . -type d -name SCCS -prune -o -type f -name '*.c' -print`
	@/bin/cat messages.po scripts/scripts.po | sed '/domain/d' > $(POFILE)
	@$(RM) messages.po
	$(RM)  $(MSGDOMAIN)/$(POFILE)
	/bin/cp $(POFILE)    $(MSGDOMAIN)

#
# Create a message file to test with.
#
_msg_test: scripts
	@$(RM)  $(POFILE)
	$(XGETTEXT) -s -m "xxx" `/bin/find . -print | grep '\.c$$' | sed '/SCCS/d'`
	@/bin/cat messages.po scripts/scripts.po | sed '/domain/d' > $(POFILE)
	echo 'domain "SUNW_OST_OSCMD"' > SUNW_OST_OSCMD.po
	cat $(POFILE) >> SUNW_OST_OSCMD.po
	msgfmt SUNW_OST_OSCMD.po
	@$(RM) messages.po $(POFILE) SUNW_OST_OSCMD.po

clean strip cstyle lint:	$(SUBDIRS)

clobber: $(SUBDIRS)
	$(RM) $(POFILE) $(CLOBBERFILES)

$(ROOTDIRS) $(MSGDOMAIN):
	$(INS.dir)

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
