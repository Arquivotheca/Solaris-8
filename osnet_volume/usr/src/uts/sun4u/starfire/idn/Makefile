#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)Makefile	1.3	99/11/05 SMI"
#
#	This makefile drives the production of the idn driver module.
#
#	sun4u starfire implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../../..

#
#	Define the module and object file sets.
#
MODULE		= idn
OBJECTS		= $(IDN_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(IDN_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_STARFIRE_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/sun4u/starfire/io

IDN_OFFSETS	= $(UTSBASE)/sun4u/starfire/ml/idn_offsets.in
IDN_OFFSETS_H	= $(OBJS_DIR)/idn_offsets.h
IDN_OFFSETS_C	= $(OBJS_DIR)/idn_offsets.c
IDN_OFFSETS_S	= $(OBJS_DIR)/idn_offsets.s
IDN_OFFSETS_TMP	= $(OBJS_DIR)/idn_offsets.tmp
IDN_OFFSETS_OUT	= $(OBJS_DIR)/idn_offsets.out

#
#	Include common rules.
#
include $(UTSBASE)/sun4u/starfire/Makefile.starfire

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFFILE)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

CLEANFILES	+= $(IDN_OFFSETS_H) $(IDN_OFFSETS_OUT)

#
# lint pass one enforcement
#
CFLAGS += -v -DIPV6

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

AS_INC_PATH	+= -I$(OBJS_DIR)

$(IDN_DEPS:%=$(OBJS_DIR)/%):	$(IDN_OFFSETS_H)

$(IDN_OFFSETS_H):	$(GENCONST) $(IDN_OFFSETS_OUT)
	@cp $(IDN_OFFSETS_OUT) $(IDN_OFFSETS_H)
	$(GENCONST) >> $(IDN_OFFSETS_H)

$(IDN_OFFSETS_OUT): $(IDN_OFFSETS)
	@grep "^#" $(IDN_OFFSETS) > $(IDN_OFFSETS_C)
	$(CC) $(CFLAGS) $(CPPFLAGS) -g -S -o $(IDN_OFFSETS_S) $(IDN_OFFSETS_C)
	@grep -v "^#" $(IDN_OFFSETS) > $(IDN_OFFSETS_TMP)
	$(STABS) -t genassym -m $(MODEL) $(IDN_OFFSETS_TMP) \
	   < $(IDN_OFFSETS_S) > $(IDN_OFFSETS_OUT)
	@rm $(IDN_OFFSETS_C) $(IDN_OFFSETS_S) $(IDN_OFFSETS_TMP)

#
#	Include common targets.
#
include $(UTSBASE)/sun4u/starfire/Makefile.targ
