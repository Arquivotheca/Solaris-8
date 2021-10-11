#
# uts/sun4u/tazmo/Makefile
# Copyright (c) 1997 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile 1.3     99/08/24 SMI"
#
#	This makefile drives the production of the sun4u tazmo platform
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
include $(UTSBASE)/sun4u/tazmo/Makefile.tazmo

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

def all clean clean.lint clobber: $(TAZMO_KMODS)
  
modlintlib:	$(TAZMO_KMODS)
 
install:	$(ROOT_TAZMO_DIR) $(USR_TAZMO_DIR) \
		$(USR_TAZMO_INC_DIR) \
		$(USR_TAZMO_SBIN_DIR) \
		$(USR_TAZMO_LIB_DIR) \
		$(TAZMO_KMODS)

check install_h:

lint:		modlintlib

#
# The 'lint.platmod' target lints the tazmo platform module against the sun4u
# kernel.  This ends up doing all the kernel cross-checks, so it takes a couple
# of minutes.  Due to the low ROI, it's not run by default, but it's a good
# idea to run this if you change os/tazmo.c.
#
LINT_LIBS	 = $(LINT_LIB) \
		   -L$(TAZMO_LINT_LIB_DIR) \
		   -L$(LINT_LIB_DIR) $(LINT_KMODS:%=-l%) \
		   -L$(SPARC_LIB_DIR) $(SPARC_LINTS:%=-l%)

lint.platmod:	modlintlib
	@-$(ECHO) "\nTazmo Platform-dependent module: global crosschecks:"
	@-$(LINT) $(LINTFLAGS) $(LINT_LIBS) 2>&1 | $(LGREP.2)

$(TAZMO_KMODS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

#
#	Include common targets.
#
include $(UTSBASE)/sun4u/tazmo/Makefile.targ
