#
# uts/sparc/elfexec/Makefile
# Copyright (c) 1991-1994, by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.8	97/11/12 SMI"
#
#	This makefile drives the production of the elfexec exec kernel
#	module.
#
#	sparc architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= elfexec
ELFEXEC_OBJS_32	=
ELFEXEC_OBJS_64	= elf32.o elf32_notes.o old32_notes.o
ELFEXEC_OBJS	+= $(ELFEXEC_OBJS_$(CLASS))
OBJECTS		= $(ELFEXEC_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(ELFEXEC_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_EXEC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
#	Overrides.
#
CFLAGS		+= -v

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ

.NO_PARALLEL:	$(LINTS)

$(OBJS_DIR)/elf32.o:	$(UTSBASE)/common/exec/elf/elf.c
	$(COMPILE.c) -o $@ -D_ELF32_COMPAT $(UTSBASE)/common/exec/elf/elf.c

$(OBJS_DIR)/elf32_notes.o: $(UTSBASE)/common/exec/elf/elf_notes.c
	$(COMPILE.c) -o $@ -D_ELF32_COMPAT $(UTSBASE)/common/exec/elf/elf_notes.c

$(OBJS_DIR)/old32_notes.o: $(UTSBASE)/common/exec/elf/old_notes.c
	$(COMPILE.c) -o $@ -D_ELF32_COMPAT $(UTSBASE)/common/exec/elf/old_notes.c

$(LINTS_DIR)/elf32.ln:	$(UTSBASE)/common/exec/elf/elf.c
	@($(LHEAD) $(LINT.c) -Celf32 -D_ELF32_COMPAT $(UTSBASE)/common/exec/elf/elf.c $(LTAIL))
	@$(MV) $(@F) $@

$(LINTS_DIR)/elf32_notes.ln: $(UTSBASE)/common/exec/elf/elf_notes.c
	@($(LHEAD) $(LINT.c) -Celf32_notes -D_ELF32_COMPAT $(UTSBASE)/common/exec/elf/elf_notes.c $(LTAIL))
	@$(MV) $(@F) $@

$(LINTS_DIR)/old32_notes.ln: $(UTSBASE)/common/exec/elf/old_notes.c
	@($(LHEAD) $(LINT.c) -Cold32_notes -D_ELF32_COMPAT $(UTSBASE)/common/exec/elf/old_notes.c $(LTAIL))
	@$(MV) $(@F) $@
