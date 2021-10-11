#
#ident	"@(#)Makefile	1.6	99/10/05 SMI"
#
# Copyright (c) 1997,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#	This makefile drives the production of the sun4u starfire platform
#	module.
#
#	sun4u starfire implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

include $(UTSBASE)/sun4u/starfire/Makefile.starfire

#
#	The following are SPARC specific (rather than sun4u) specific modules
#	which are required for the sun4u kernel to completely lint. They are
#	not involved in the build in any other way. In order to minimize
#	build time, it is assumed that they are up to date. But since sun4u
#	is really a separate architecture we cannot use the v7 sparc modules.
#
SPARC_LIB_DIR	= $(UTSBASE)/sparc/lint-libs/$(OBJS_DIR)

SPARC_LINTS	=

SUN4U_LIB_DIR	= $(UTSBASE)/sun4u/lint-libs/$(OBJS_DIR)

#
#
#
LINT_LIBS	= $(LINT_LIB) \
		  $(SPARC_LINTS:%=$(SPARC_LIB_DIR)/llib-l%.ln) \
		  $(LINT_KMODS:%=$(SUN4U_LIB_DIR)/llib-l%.ln)

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

IMPLEMENTED_PLATFORM	= SUNW,Ultra-Enterprise-10000

#
#	Default build targets.
#
.KEEP_STATE:

.PARALLEL:	$(STARFIRE_KMODS)

.NO_PARALLEL:	$(STARFIRE_CPU_KMODS)

def all clean clobber clean.lint: genassym unix .WAIT \
		$(STARFIRE_CPU_KMODS) $(STARFIRE_KMODS)

install: $(ROOT_STARFIRE_DIR) $(USR_STARFIRE_DIR) \
	$(USR_STARFIRE_INC_DIR) \
	$(USR_STARFIRE_SBIN_DIR) \
	$(USR_STARFIRE_LIB_DIR) \
	genassym unix .WAIT $(STARFIRE_CPU_KMODS) $(STARFIRE_KMODS)

lintlib:	unix

modlintlib:	$(STARFIRE_KMODS) $(STARFIRE_CPU_KMODS)

genassym unix $(STARFIRE_CPU_KMODS) $(STARFIRE_KMODS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

install_h check:	FRC
	@cd sys; pwd; $(MAKE) $(TARGET)

#
#	Full kernel lint target.
#
LINT_TARGET	= globallint

globallint:
	@-$(ECHO) "\nStarfire KERNEL: global crosschecks:"
	@-$(LINT) $(LINTFLAGS) $(LINT_LIBS) 2>&1 | $(LGREP.2)

lint:	lintlib .WAIT modlintlib .WAIT $(LINT_DEPS)
 
#
#	Include common targets.
#
include $(UTSBASE)/sun4u/starfire/Makefile.targ
