#
# uts/intel/acpi_intp/Makefile
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident "@(#)Makefile	1.2	99/08/30 SMI"
#
# This makefile drives the production of the acpi_intp misc kernel module.
#
# intel architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= acpi_intp
OBJECTS		= $(ACPI_INTP_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(ACPI_INTP_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

ACPI_GCOM_SRC	= acpi_gcom.c acpi_gcom_exc.c
ACPI_GCOM_OBJS	= $(ACPI_GCOM_SRC:%.c=%.o)
GCOM_OBJECTS	= $(ACPI_GCOM_OBJS:%=$(OBJS_DIR)/%)
GCOM_LINTS	= $(ACPI_GCOM_OBJS:%.o=$(LINTS_DIR)/%.ln)

#
#	Include common rules.
#
include $(UTSBASE)/intel/Makefile.intel

#
#	Override defaults
#
ACPIIABASE	= $(UTSBASE)/intel
ACPIBASE	= $(ACPIIABASE)/os/acpi_intp
ACPI_GCOM	= $(OBJS_DIR)/acpi_gcom
ACPI_GRAM	= $(ACPIBASE)/acpi_gram.c

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint acpi_gcom.lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
# lint pass one enforcement
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
include $(UTSBASE)/intel/Makefile.targ


#
#	Special rules to generate ACPI grammar
#


# grammar compiler
$(ACPIBASE)/acpi_gcom_exc.c: $(ACPIBASE)/acpi_exc.c 
	$(RM) $@; $(SYMLINK) acpi_exc.c $@

$(GCOM_OBJECTS): $(ACPIBASE)/$$(@F:.o=.c)
	$(NATIVECC) -c -I$(ACPIIABASE) -o $@ $(ACPIBASE)/$(@F:.o=.c)

$(ACPI_GCOM): $(OBJS_DIR) $(ACPIBASE)/acpi_gcom_exc.c .WAIT $(GCOM_OBJECTS)
	$(NATIVECC) -o $@ $(GCOM_OBJECTS)

CLEANFILES += $(ACPIBASE)/acpi_gcom_exc.c $(GCOM_OBJECTS) $(OBJS_DIR)/acpi_gcom


# generated grammar rules
$(ACPI_GRAM): $(ACPIBASE)/acpi_grammar $(ACPI_GCOM)
	./$(ACPI_GCOM) $(ACPIBASE)/acpi_grammar $(ACPIBASE)/acpi_elem.h $(ACPIBASE)/acpi_gram.c

$(OBJECTS) $(LINTS): $(ACPI_GCOM) .WAIT $(ACPI_GRAM)

CLEANFILES += $(ACPIBASE)/acpi_elem.h $(ACPIBASE)/acpi_gram.c


# lint rules
GCOM_LINT.c = $(LINT) -c -dirout=$(LINTS_DIR) $(LINTFLAGS) -I$(ACPIIABASE) 
GCOM_LINT_LIB = $(LINT_LIB_DIR)/llib-lacpi_gcom.ln

$(GCOM_LINTS): $(ACPIBASE)/$$(@F:.ln=.c)
	@($(LHEAD) $(GCOM_LINT.c) $(ACPIBASE)/$(@F:.ln=.c) $(LTAIL))

$(GCOM_LINT_LIB): $(LINT_LIB_DIR) $(GCOM_LINTS)
	@-$(ECHO) "\n$(LINTS_DIR)/acpi_gcom: (library construction):"
	@$(LINT) -o acpi_gcom $(LINTFLAGS) $(GCOM_LINTS)
	@$(MV) $(@F) $@

acpi_gcom.lint: $(GCOM_LINT_LIB)
	@-$(ECHO) "\n$(LINTS_DIR)/acpi_gcom: global crosschecks:"
	@$(LINT) $(LINTFLAGS) $(GCOM_LINT_LIB)

CLEANFILES += $(GCOM_LINTS) $(GCOM_LINT_LIB)


# eof
