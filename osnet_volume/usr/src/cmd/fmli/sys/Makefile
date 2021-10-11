#
#ident	"@(#)Makefile	1.4	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/fmli/sys/Makefile
#
LIBRARY = libsys.a

OBJECTS = actrec.o ar_mfuncs.o backslash.o chgenv.o chgepenv.o \
	compile.o coproc.o copyfile.o estrtok.o evfuncs.o eval.o \
	evalstr.o exit.o expand.o expr.o filename.o genfind.o \
	getaltenv.o getepenv.o grep.o io.o itoa.o memshift.o \
	mencmds.o cut.o nstrcat.o onexit.o parent.o putaltenv.o \
	readfile.o scrclean.o spawn.o strappend.o strCcmp.o stream.o \
	strsave.o tempfiles.o terrmess.o terror.o test.o varappend.o \
	varchkapnd.o varcreate.o vardelete.o vargrow.o varinsert.o \
	varshrink.o watch.o

# include library definitions
include ../../../lib/Makefile.lib


# inc must be first.
CPPFLAGS = -I../inc $(CPPFLAGS.master)

.KEEP_STATE:

all: $(LIBS)

install: all
	@echo "$(LIBRARY) is not installed."

# include library targets
include ../../../lib/Makefile.targ
