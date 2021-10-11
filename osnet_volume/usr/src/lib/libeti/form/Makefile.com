#
# Copyright (c) 1989-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.5	99/01/25 SMI"
#
# lib/libeti/form/Makefile.com

LIBRARY=	libform.a
VERS=		.1

OBJECTS=  \
	chg_char.o \
	chg_data.o \
	chg_field.o \
	chg_page.o \
	driver.o \
	field.o \
	field_back.o \
	field_buf.o \
	field_fore.o \
	field_init.o \
	field_just.o \
	field_opts.o \
	field_pad.o \
	field_stat.o \
	field_term.o \
	field_user.o \
	fieldtype.o \
	form.o \
	form_init.o \
	form_opts.o \
	form_sub.o \
	form_term.o \
	form_user.o \
	form_win.o \
	post.o \
	regcmp.o \
	regex.o \
	ty_alnum.o \
	ty_alpha.o \
	ty_enum.o \
	ty_int.o \
	ty_num.o \
	ty_regexp.o \
	utility.o

# include library definitions
include ../../../Makefile.lib

MAPFILE=        $(MAPDIR)/mapfile
CLOBBERFILES +=	$(MAPFILE)

SRCS=           $(OBJECTS:%.o=../common/%.c)

LIBS =          $(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS=      -u -I../inc
LINTFLAGS64=    -u -I../inc -D__sparcv9
LINTOUT=        lint.out

LINTSRC=        $(LINTLIB:%.ln=%)

ROOTLINTDIR=    $(ROOTLIBDIR)
ROOTLINT=       $(LINTSRC:%=$(ROOTLINTDIR)/%)

STATICLIBDIR=   $(ROOTLIBDIR)
STATICLIB=      $(LIBRARY:%=$(STATICLIBDIR)/%)

DYNLINKLIBDIR=  $(ROOTLIBDIR)
DYNLINKLIB=     $(LIBLINKS:%=$(DYNLINKLIBDIR)/%)

CLEANFILES +=   $(LINTOUT) $(LINTLIB)

CFLAGS +=       -v -I../inc
CFLAGS64 +=     -v -I../inc
DYNFLAGS +=     -M $(MAPFILE)
LDLIBS +=       -lcurses -lc

.KEEP_STATE:

all: $(LIBS)

lint: $(LINTLIB)

$(DYNLIB):      $(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# include library targets
include ../../../Makefile.targ

llib-lform: ../common/llib-lform.c
	$(RM) $@
	cp ../common/llib-lform.c $@

objs/%.o profs/%.o pics/%.o:    ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)

# install rule for 32-bit libform.a
$(STATICLIBDIR)/%: %
	$(INS.file)

$(DYNLINKLIBDIR)/%: %$(VERS)
	$(INS.liblink)

