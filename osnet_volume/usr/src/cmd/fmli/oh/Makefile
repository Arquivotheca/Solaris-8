#
#ident	"@(#)Makefile	1.5	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/fmli/oh/Makefile
#
LIBRARY = liboh.a

OBJECTS = action.o alias.o cmd.o detab.o detect.o dispfuncs.o \
	evstr.o externoot.o fm_mn_par.o getval.o helptext.o \
	if_ascii.o if_dir.o if_init.o if_exec.o if_form.o if_help.o \
	if_menu.o ifuncs.o interrupt.o is_objtype.o misc.o \
	namecheck.o nextpart.o obj_to_opt.o obj_to_par.o odftread.o \
	odikey.o oh_init.o ootpart.o ootread.o opt_rename.o optab.o \
	optabfuncs.o ott_mv.o partab.o partabfunc.o pathtitle.o \
	pathfstype.o path_to_vp.o pathott.o scram.o slk.o \
	suffuncs.o typefuncs.o typetab.o

# include library definitions
include ../../../lib/Makefile.lib


# inc must be first.
CPPFLAGS = -I../inc $(CPPFLAGS.master)
CPPFLAGS += -DWISH

.KEEP_STATE:

all: $(LIBS)

install: all
	@echo "$(LIBRARY) is not installed."

# include library targets
include ../../../lib/Makefile.targ
