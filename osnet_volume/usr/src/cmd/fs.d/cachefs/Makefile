#
#ident	"@(#)Makefile	1.20	99/05/24 SMI"
#
# Copyright (c) 1995, 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#  cmd/fs.d/cachefs/Makefile
#

SUBDIRS=	common \
		cfsadmin \
		cfsfstype \
		cfstagchk \
		fsck \
		mount \
		umount \
		cachefslog \
		cachefswssize \
		cachefsstat \
		cachefspack \
		mdbug \
		cfsd \
		dfshares \
		share \
		unshare

# Zip Stuff
#SUBDIRS += 	cfscowchk \
#		cfscowchk \
#		cfscvtmnt \
#		cfslu \
#		cfspin \
#		cfsunpin 

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

# include Makefile.cmd and Makefile.targ for _msg target
include ../../Makefile.cmd

POFILE= cachefs.po
GREP= grep
SED= sed

.KEEP_STATE:

all install clean clobber lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

catalog: $(POFILE)

$(POFILE):
	$(RM) messages.po
	$(XGETTEXT) $(XGETFLAGS) `$(GREP) -l gettext */*.[ch]`
	$(SED) "/^domain/d" < messages.po > $@
	$(RM) messages.po

local_clobber:
	$(RM) $(CLOBBERFILES)

FRC:

include ../../Makefile.targ
