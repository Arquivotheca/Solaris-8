#
# uts/sun4u/darwin/Makefile
# Copyright (c) 1999 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile 1.2     99/07/12 SMI"
#
#	This makefile drives the production of the sun4u darwin platform
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
include $(UTSBASE)/sun4u/darwin/Makefile.darwin

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

def all clean clean.lint clobber: $(DARWIN_KMODS)

modlintlib:	$(DARWIN_KMODS)

install:	$(ROOT_DARWIN_DIR) $(USR_DARWIN_DIR) $(DARWIN_KMODS)

check install_h:

lint:		modlintlib

#
# The 'lint.platmod' target lints the darwin platform module against the sun4u
# kernel.  This ends up doing all the kernel cross-checks, so it takes a couple
# of minutes.  Due to the low ROI, it's not run by default, but it's a good
# idea to run this if you change os/darwin.c.
#
LINT_LIBS	 = $(LINT_LIB) \
		   -L$(DARWIN_LINT_LIB_DIR) \
		   -L$(LINT_LIB_DIR) $(LINT_KMODS:%=-l%) \
		   -L$(SPARC_LIB_DIR) $(SPARC_LINTS:%=-l%)

lint.platmod:	modlintlib
	@-$(ECHO) "\nDarwin Platform-dependent module: global crosschecks:"
	@-$(LINT) $(LINTFLAGS) $(LINT_LIBS) 2>&1 | $(LGREP.2)

$(DARWIN_KMODS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

#
#	Include common targets.
#
include $(UTSBASE)/sun4u/darwin/Makefile.targ
