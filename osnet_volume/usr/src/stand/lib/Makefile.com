#
#ident	"@(#)Makefile.com	1.9	99/02/23 SMI"
#
# Copyright (c) 1994-1999 by Sun Microsystems, Inc.
#
# stand/lib/Makefile.com
#
# Common part of makefiles for standalone libraries
#

# CMNDIR - architecture independent code
# OBJSDIR - where the .o's go

CMNDIR=		../common
OBJSDIR=	objs

# conditional assignments, affecting the lists of OBJECTS used by targets
#
SPRINTF.o=	sprintf.o
SPRINTF.ln=	sprintf.ln
SPRINTF.c=	$(CMNDIR)/sprintf.c
$(LIBKADB) :=	SPRINTF.o =
$(LIBKADB) :=	SPRINTF.ln =
$(LIBKADB) :=	SPRINTF.c =
$(LIBKADB) :=	OBJSDIR = kadbobjs
$(LIBKADB_L) :=	SPRINTF.o =
$(LIBKADB_L) :=	SPRINTF.ln =
$(LIBKADB_L) :=	SPRINTF.c =
$(LIBKADB_L) :=	OBJSDIR = kadbobjs

#
# Dynamic, depending on which library is being built
#
OBJS=		$(OBJECTS:%=$(OBJSDIR)/%) $(FS_OBJECTS:%=$(OBJSDIR)/%)
L_OBJS=		$(OBJECTS_L:%=$(OBJSDIR)/%) $(FS_OBJECTS_L:%=$(OBJSDIR)/%)
SRCS=		$(SOURCES) $(FS_SOURCES)

CFLAGS +=	-v
LDFLAGS=	-r

#
# Lint rules (adapted from Makefile.uts)
#
LHEAD		= ( $(ECHO) "\n$@";
LGREP		= grep -v "pointer cast may result in improper alignment"
LTAIL		= ) 2>&1 | $(LGREP)
LINT_DEFS	+= -Dlint

#
# For building lint objects
#
LINTFLAGS.c	= -nsxumF -dirout=$(OBJSDIR)
LINTFLAGS.s	+= -dirout=$(OBJSDIR)
LINT.c		= $(LINT) $(LINTFLAGS.c) $(LINT_DEFS) $(CPPFLAGS) -c
LINT.s		= $(LINT) $(LINTFLAGS.s) $(LINT_DEFS) $(CPPFLAGS) -c

#
# For building lint libraries
# LINT.inc is used for adding -I's specific to certain fstypes (nfs).
#
LINTFLAGS.lib	= -ynsxumF
LINT.lib	= $(LINT) $(LINTFLAGS.lib) $(LINT_DEFS) $(LINT.inc) $(CPPFLAGS)

.KEEP_STATE:

#
# NB: Unfortunately, the deferred dependency, below, doesn't seem to work.
# Thus, all the including makefiles have hardcoded knowledge of the
# target directories and their own .PARALLEL targets to allow parallel
# make to work.
#
.PARALLEL:	$$(OBJS) $$(L_OBJS)
.PARALLEL:	$(ALL_LIBS) $(ALL_LIBS_L)
.PARALLEL:	objs/sprintf.o objs/sprintf.ln
.PARALLEL:	kadbobjs/sprintf.o kadbobjs/sprintf.ln

all install alllibs: objs kadbobjs .WAIT $(ALL_LIBS)

lintlibs lint: objs kadbobjs .WAIT $(ALL_LIBS_L)

$(ALL_LIBS): $$(OBJSDIR) .WAIT $$(OBJS)
	$(BUILD.AR) $(OBJS)

objs kadbobjs:
	-@[ -d $@ ] || mkdir $@

$(ALL_LIBS_L): $$(OBJSDIR) .WAIT $$(L_OBJS) $$(SRCS)
	@$(ECHO) "\nlint library construction:" $@
	@$(LINT.lib) -o $(@:llib-l%.ln=%) $(SRCS) 2>&1 | $(LGREP)

clobber: clean
	$(RM) $(ALL_LIBS) $(ALL_LIBS_L)

clean:
	$(RM) kadbobjs/*.o objs/*.o *.a a.out *.i core make.out
	$(RM) kadbobjs/*.ln objs/*.ln lint.out

#
# build rules using standard library object subdirectory
#
$$(OBJSDIR)/%.o: %.s
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)
$$(OBJSDIR)/%.o: %.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
$$(OBJSDIR)/%.o: $(CMNDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
$$(OBJSDIR)/%.o: $(CMNDIR)/%.s
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)
$$(OBJSDIR)/%.o: $(UFSDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
$$(OBJSDIR)/%.o: $(HSFSDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
$$(OBJSDIR)/%.o: $(NFSDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
$$(OBJSDIR)/%.o: $(DHCP_CMNDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
$$(OBJSDIR)/%.o: $(FSCMNDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
$$(OBJSDIR)/%.o: $(PCFSDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
$$(OBJSDIR)/%.o: $(COMPFSDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
$$(OBJSDIR)/%.o: $(CACHEFSDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$$(OBJSDIR)/%.ln: %.s
	@$(LHEAD) $(LINT.s) $< $(LTAIL)
$$(OBJSDIR)/%.ln: %.c
	@$(LHEAD) $(LINT.c) $< $(LTAIL)
$$(OBJSDIR)/%.ln: $(CMNDIR)/%.c
	@$(LHEAD) $(LINT.c) $< $(LTAIL)
$$(OBJSDIR)/%.ln: $(CMNDIR)/%.s
	@$(LHEAD) $(LINT.s) $< $(LTAIL)
$$(OBJSDIR)/%.ln: $(UFSDIR)/%.c
	@$(LHEAD) $(LINT.c) $< $(LTAIL)
$$(OBJSDIR)/%.ln: $(HSFSDIR)/%.c
	@$(LHEAD) $(LINT.c) $< $(LTAIL)
$$(OBJSDIR)/%.ln: $(NFSDIR)/%.c
	@$(LHEAD) $(LINT.c) $< $(LTAIL)
$$(OBJSDIR)/%.ln: $(DHCP_CMNDIR)/%.c
	@$(LHEAD) $(LINT.c) $< $(LTAIL)
$$(OBJSDIR)/%.ln: $(FSCMNDIR)/%.c
	@$(LHEAD) $(LINT.c) $< $(LTAIL)
$$(OBJSDIR)/%.ln: $(PCFSDIR)/%.c
	@$(LHEAD) $(LINT.c) $< $(LTAIL)
$$(OBJSDIR)/%.ln: $(COMPFSDIR)/%.c
	@$(LHEAD) $(LINT.c) $< $(LTAIL)
$$(OBJSDIR)/%.ln: $(CACHEFSDIR)/%.c
	@$(LHEAD) $(LINT.c) $< $(LTAIL)
