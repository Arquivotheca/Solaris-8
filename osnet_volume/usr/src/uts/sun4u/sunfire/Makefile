#
# uts/sun4u/sunfire/Makefile
# Copyright (c) 1998 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile 1.6     99/11/03 SMI"
#
#	This makefile drives the production of the sun4u sunfire platform
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
include $(UTSBASE)/sun4u/sunfire/Makefile.sunfire

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

def all clean clean.lint clobber: $(SUNFIRE_KMODS)

modlintlib:	$(SUNFIRE_KMODS)
  
IMPLEMENTED_PLATFORM	= SUNW,Ultra-Enterprise

install:	$(ROOT_SUNFIRE_DIR) $(USR_SUNFIRE_DIR) \
		$(USR_SUNFIRE_INC_DIR) \
		$(USR_SUNFIRE_SBIN_DIR) \
		$(USR_SUNFIRE_LIB_DIR) \
		.WAIT $(SUNFIRE_KMODS)

install_h check:	FRC
	@cd sys; pwd; $(MAKE) $(TARGET)

lint:		modlintlib

#
# The 'lint.platmod' target lints the sunfire platform module against the sun4u
# kernel.  This ends up doing all the kernel cross-checks, so it takes a couple
# of minutes.  Due to the low ROI, it's not run by default, but it's a good
# idea to run this if you change os/sunfire.c.
#
LINT_LIBS	 = $(LINT_LIB) \
		   -L$(SUNFIRE_LINT_LIB_DIR) \
		   -L$(LINT_LIB_DIR) $(LINT_KMODS:%=-l%) \
		   -L$(SPARC_LIB_DIR) $(SPARC_LINTS:%=-l%)

lint.platmod:	modlintlib
	@-$(ECHO) "\nSunfire Platform-dependent module: global crosschecks:"
	@-$(LINT) $(LINTFLAGS) $(LINT_LIBS) 2>&1 | $(LGREP.2)

$(SUNFIRE_KMODS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

#
#	Include common targets.
#
include $(UTSBASE)/sun4u/sunfire/Makefile.targ
