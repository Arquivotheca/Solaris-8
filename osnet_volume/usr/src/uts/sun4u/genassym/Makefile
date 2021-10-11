#
# uts/sun4u/genassym/Makefile
# Copyright (c) 1991-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.6	97/11/12 SMI"
#
#	This makefile drives the production of assym.h through genconst/stabs.
#
#	sun4u implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

ASSYM_H		= $(DSF_DIR)/$(OBJS_DIR)/assym.h
GENCONST	= $(DSF_DIR)/$(OBJS_DIR)/genconst
OFFSETS_C	= $(DSF_DIR)/$(OBJS_DIR)/offsets.c
OFFSETS_S	= $(DSF_DIR)/$(OBJS_DIR)/offsets.s
OFFSETS_TMP	= $(DSF_DIR)/$(OBJS_DIR)/offsets.tmp
OFFSETS_OUT	= $(DSF_DIR)/$(OBJS_DIR)/offsets.out

#
#	Include common rules.
#
include $(UTSBASE)/sun4u/Makefile.sun4u

#
#	Define targets
#
ALL_TARGET	= $(ASSYM_H)

#
# This is DSF_DIR.  Use a short path.
#
DSF_DIR		= .

#
#	Overrides
#
CLEANFILES	= $(GENCONST) $(OFFSETS_OUT)  $(OFFSETS_C) $(OFFSETS_S) \
		  $(OFFSETS_TMP) Nothing_to_remove
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
#	genconst build.
#
$(GENCONST):    $(GENCONST_SRC)
	$(NATIVECC) -w $(ALWAYS_DEFS) $(GENCONST_DEFS) $(NATIVE_INC_PATH) \
	    -o $(GENCONST) $(GENCONST_SRC)

#
#	Execute genassym and genconst to produce assym.h.
#
$(ASSYM_H):	$(GENCONST) $(OFFSETS_OUT)
	@cp $(OFFSETS_OUT) $(ASSYM_H)
	$(GENCONST) >> $(ASSYM_H)

.SUFFIXES: .o .c .in .out

$(OFFSETS_OUT): $(OFFSETS)
	@grep "^#" $(OFFSETS) > $(OFFSETS_C)
	$(CC) $(CFLAGS) $(CPPFLAGS) -g -S -o $(OFFSETS_S) $(OFFSETS_C)
	@grep -v "^#" $(OFFSETS) > $(OFFSETS_TMP)
	$(STABS) -t genassym -m $(MODEL) $(OFFSETS_TMP) \
	   < $(OFFSETS_S) > $(OFFSETS_OUT)
	@rm $(OFFSETS_C) $(OFFSETS_S) $(OFFSETS_TMP)

#
#	Include common targets.
#
include $(UTSBASE)/sun4u/Makefile.targ
