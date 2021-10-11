#
# uts/i86pc/genassym/Makefile
# Copyright (c) 1991-1994, by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.9	97/11/12 SMI"
#
#	This makefile drives the production of assym.h through genassym.
#
#	i86pc implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

ASSYM_H		= $(DSF_DIR)/$(OBJS_DIR)/assym.h
GENASSYM	= $(DSF_DIR)/$(OBJS_DIR)/genassym

#
#	Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

#
# This is DSF_DIR.  Use a short path.
#
DSF_DIR		= .

#
#	Define targets
#
ALL_TARGET	= $(ASSYM_H)

#
#	Overrides
#
CLEANFILES	= $(GENASSYM) Nothing_to_remove
CLOBBERFILES	= $(ASSYM_H) $(CLEANFILES) Nothing_to_remove

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

clean.lint:

install:	def

#
#	genassym build.
#
$(GENASSYM):    $(GENASSYM_SRC)
	$(NATIVECC) $(ALWAYS_DEFS) $(GENASSYM_DEFS) $(NATIVE_INC_PATH) \
	-O -o $(GENASSYM) $(GENASSYM_SRC)

#
#	Execute genassym to produce assym.h.
#
$(ASSYM_H):     $(GENASSYM)
	$(GENASSYM) > $(ASSYM_H)

#
#	Include common targets.
#
include $(UTSBASE)/i86pc/Makefile.targ
