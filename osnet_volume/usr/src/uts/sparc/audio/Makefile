#
# uts/sparc/audio/Makefile
# Copyright (c) 1991-1994, by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.10	99/10/21 SMI"
#
#	This makefile drives the production of the audio driver kernel
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
MODULE		= audio
OBJECTS		= $(AUDIO_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(AUDIO_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(USR_DRV_DIR)/$(MODULE)

ASSYM_H		= $(OBJS_DIR)/audio_assym.h
GENASSYM	= $(OBJS_DIR)/audio_genassym

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
#	Overrides
#
CLEANFILES	+= $(ASSYM_H) $(GENASSYM)
$(OBJS_DIR)/audio_79C30_intr.o := AS_CPPFLAGS += -I$(OBJS_DIR)
DEF_BUILDS	= $(DEF_BUILDS32)
ALL_BUILDS	= $(ALL_BUILDS32)
CLEANLINTFILES	+= $(LINT64_FILES)

#
#	Depends on misc/diaudio
#
LDFLAGS		+= -dy -Nmisc/diaudio

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

modlintlib:	$(MODLINTLIB_DEPS) lint64

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

$(BINARY):	$(ASSYM_H)

#
#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ

#
#	Assym targets
#
$(GENASSYM):	$(AUDIOGENASSYM_SRC)
	$(NATIVECC) $(ALWAYS_DEFS) $(NATIVE_INC_PATH) -o $(GENASSYM) \
	$(AUDIOGENASSYM_SRC)

$(ASSYM_H):	$(GENASSYM)
	$(GENASSYM) > $(ASSYM_H)
