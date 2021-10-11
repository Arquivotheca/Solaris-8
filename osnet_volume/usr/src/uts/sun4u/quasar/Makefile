#
# uts/sun4u/quasar/Makefile
# Copyright (c) 1999 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile 1.2     99/07/12 SMI"
#
#	This makefile drives the production of the sun4u quasar platform
#	module.
#
#	sun4u implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Include common rules.
#
include $(UTSBASE)/sun4u/quasar/Makefile.quasar

def		:=	TARGET= def
all		:=	TARGET= all
install		:=	TARGET= install
install_h	:=	TARGET= install_h
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint
lintlib		:=	TARGET= lintlib
modlintlib	:=	TARGET= modlintlib
clean.lint	:=	TARGET= clean.lint
check		:=	TARGET= check

#
#	Default build targets.
#
.KEEP_STATE:

def all install_h clean lint lintlib modlintlib clean.lint clobber check:

install:	install_darwin $(INSTALL_DEPS) $(USR_QUASAR_DIR)

INSTALL_TARGET	=	platmod

platmod: 	$(ROOT_QUASAR_MISC_DIR)
	-@$(RM) -r $(OBJS_DIR)
	-@$(RM) $(ROOT_QUASAR_MISC_DIR)/$@
	$(SYMLINK) $(ROOT_QUASAR_MISC_LINK)/$@ $(ROOT_QUASAR_MISC_DIR)/$@

install_darwin:
	@cd ../darwin; $(MAKE) $(TARGET)

#
#	Include common targets.
#
include $(UTSBASE)/sun4u/quasar/Makefile.targ
