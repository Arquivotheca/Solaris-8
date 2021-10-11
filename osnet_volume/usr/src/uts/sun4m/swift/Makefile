#
# uts/sun4m/colorado/Makefile
# Copyright (c) 1996 by Sun Microsystems, Inc.
#
#ident   "@(#)Makefile 1.3     97/11/12 SMI"
#
#	This makefile drives the production of the swift sun4m cpu driver module
#
#	sun4m implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..
#
#	Define the module and object file sets.
#
MODULE		= FMI,MB86904
OBJECTS		= $(SWIFT_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SWIFT_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_CPU_DIR)/$(MODULE)
SOFTLINKS	= FMI,MB86907
ROOTSOFTLINKS	= $(SOFTLINKS:%=$(ROOT_PSM_CPU_DIR)/%)

#
#	Include common rules.
#
include $(UTSBASE)/sun4m/Makefile.sun4m

#
#	Define targets
#
ALL_TARGET	= $(CPUSYM_MOD)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOTSOFTLINKS)

#
#	Override defaults
#
CLEANFILES	+= $(CPULIB) $(CPUSYM_MOD)

#
# lint pass one enforcement
#
CFLAGS += -v

#
#	Default build targets.
#
.KEEP_STATE:

.PARALLEL:	$(OBJECTS)

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

$(CPUSYM_MOD):	$(BINARY)
	@echo "resolving symbols against unix.o"
	$(LD) -o $(CPULIB) -G $(BINARY)
	@(cd $(UNIX_DIR); CPU_DIR=../swift $(MAKE) symcheck)

$(ROOTSOFTLINKS): $(ROOTMODULE)
	$(RM) $@; $(SYMLINK) $(MODULE) $@ $(CHOWNLINK) $(CHGRPLINK)

#
#	Include common targets.
#
include $(UTSBASE)/sun4m/Makefile.targ
