#
# Copyright (c) 1994, 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.117	99/03/23 SMI"
#
# psm/stand/kadb/common/Makefile.com
#
# kernel debugger (kadb) included common Makefile
# included by kernel-architecture specific makefiles
#

#
# Hack until stand makefiles are fixed
#
CLASS	= 32

# Include global definitions. Note: $(TOPDIR) is defined in
# the architecture specific Makefile.
include $(TOPDIR)/Makefile.master
include $(TOPDIR)/Makefile.psm
include $(TOPDIR)/psm/stand/lib/Makefile.lib

PROG=		kadb
PLATFORM=	$(ARCH)

STANDDIR= 	$(TOPDIR)/stand
SYSDIR=  	$(TOPDIR)/uts
ARCHDIR= 	$(SYSDIR)/$(ARCH)
MACHDIR= 	$(SYSDIR)/$(MACH)
LIBDIR=		$(STANDDIR)/lib/$(MACH)
LIBDIR-net=	$(STANDDIR)/lib/fs/nfs
MMUDIR=		$(SYSDIR)/$(MMU)

# references to cmd/adb source
ADBMACH=	$(TOPDIR)/cmd/adb/$(MACH)
ADBDIR=		$(TOPDIR)/cmd/adb/$(MACH)/kadb
ADBCOM=		$(TOPDIR)/cmd/adb/common

# for NSE compatibility ADBOBJ has ARCH encoded in its name;
# this allows the ARCH value to be passed in the target name
ADBOBJ=		$(ADBDIR)/$(ARCH)-kadb.o

PROMDIR= 	$(PSMSTANDLIBDIR)/promif/$(MACH)/$(PROMTYPE)/common
PLATDIR= 	$(PSMSTANDLIBDIR)/promif/$(MACH)/$(PROMTYPE)/$(KARCH)
PSMNAMELIBDIR=	$(PSMSTANDLIBDIR)/names/$(MACH)/$(KARCH)
PSMBOOTLIBDIR=	$(PSMSTANDLIBDIR)/boot/$(MACH)/$(KARCH)

OBJSDIR=	.

KADBPROMLIB =	$(PROMDIR)/libprom.a

#
# Define PLATLIB in ARCH Makefile if required.
# Need specific path in ARCH Makefile.
#PLATLIB=

OBJ=		kadb.o main.o
AOBJ=		debug.o

MACDIR=		$(SYSDIR)/adb
# returns the list of adb macro files to use as arguments to genpf
#
MACLIST=	`cd ${MACDIR}/platform/$(MACH)/$(ARCH); $(MAKE) maclist`

CPPDEFS=	$(ARCHOPTS) -DKADB -D_KADB -D$(ARCH) -D__$(ARCH) -D$(MACH) \
		-D__$(MACH) -D_KERNEL -D_MACHDEP -D__ELF 

CPPINCS=	-I. -I${COMDIR} $(INCLUDE) -I$(SYSDIR)/$(KSUN) \
		-I$(SYSDIR)/common -I${ADBCOM} \
		-I$(ADBMACH) -I$(TOPDIR)/head

CPPOPTS=  	$(CPPDEFS)  $(CPPINCS)
COPTS=
CPPFLAGS=	$(CCYFLAG)$(SYSDIR)/common $(CPPFLAGS.master)
AS_CPPFLAGS=	$(CPPFLAGS.master)
CFLAGS=		${COPTS} ${CPPOPTS}
ASFLAGS= 	-P -D_ASM -D_KADB $(CPPOPTS) $(AS_CPPFLAGS) -DLOCORE \
		-D_LOCORE -D__STDC__

# Stolen from Makefile.cmd, so that kadb gets built against current libc.
# storing LDLIBS in two macros allows reordering of options
LDLIBS.kadb=	$(ENVLDLIBS1)  $(ENVLDLIBS2)  $(ENVLDLIBS3)
LDLIBS=		$(LDLIBS.kadb)
LDFLAGS.kadb=	$(STRIPFLAG) $(ENVLDFLAGS1) $(ENVLDFLAGS2) $(ENVLDFLAGS3)
LDFLAGS=	$(LDFLAGS.kadb)

LD_LIBNFS=	-L$(LIBDIR) -lnfs
LD_LIBSA=	-L$(LIBDIR) -lsadb
LD_NAMES=	-L$(PSMNAMELIBDIR) -lnames
LD_BOOT=	-L$(PSMBOOTLIBDIR) -lboot
LDLIBS +=	$(LD_NAMES) $(LD_LIBSA) $(LD_BOOT)

LIBSA=		$(LIBDIR)/libsadb.a
L_LIBSA=	$(LIBDIR)/llib-lsadb.ln
LIBNFS=		$(LIBDIR)/libnfs.a
L_LIBNFS=	$(LIBDIR)/llib-lnfs.ln
LIBBOOT=	$(PSMBOOTLIBDIR)/libboot.a
L_LIBBOOT=	$(PSMBOOTLIBDIR)/llib-lboot.ln
LIBNAMES=	$(PSMNAMELIBDIR)/libnames.a
L_LIBNAMES=	$(PSMNAMELIBDIR)/llib-lnames.ln

LIBS=		$(LIBSA) $(LIBNAMES) $(LIBBOOT)
L_LIBS=		$(L_LIBSA) $(L_LIBNAMES) $(L_LIBBOOT)

L_LDLIBS=	$(LDLIBS)

OTHERLIBS=	$(KADBPROMLIB) $(PLATLIB)

L_PROMLIB=	$(KADBPROMLIB:%libprom.a=%llib-lprom.ln)
L_PLATLIB=	$(PLATLIB:%libplat.a=%llib-lplat.ln)
L_OTHERLIBS=	$(L_PROMLIB) $(L_PLATLIB)

# install values
FILEMODE=	644
OWNER=		root
GROUP=		sys

LINT.c=	$(LINT) $(LINTFLAGS.c) $(LINT_DEFS) $(CFLAGS) -c
LINT.s=	$(LINT) $(LINTFLAGS.s) $(LINT_DEFS) $(CFLAGS) -c

# build rules
$(OBJSDIR)/%.o: $(COMDIR)/%.c
	$(COMPILE.c) $<

$(OBJSDIR)/%.o: ./%.c
	$(COMPILE.c) $<

$(OBJSDIR)/%.o: ../common/%.c
	$(COMPILE.c) $<

$(OBJSDIR)/%.ln: $(COMDIR)/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))
 
$(OBJSDIR)/%.ln: ./%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))
 
$(OBJSDIR)/%.ln: ../common/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))
 
$(OBJSDIR)/%.ln: ./%.s
	@($(LHEAD) $(LINT.s) $< $(LTAIL))

.KEEP_STATE:

.PARALLEL:	$(OBJ) $(ARCHOBJ) $(MACHOBJ)

ALL=	kadb
all:	$(ALL)

kadb:	ukadb.o pf.o mapfile $$(LIBS) $$(OTHERLIBS)
	${LD} -dn -M mapfile -e start -Bstatic -o $@ -z muldefs \
		ukadb.o pf.o $(LDLIBS) $(OTHERLIBS)

standalloc.o: $(TOPDIR)/stand/lib/common/standalloc.c
	${CC} -I${STANDDIR} ${CPPOPTS} -c \
		$(TOPDIR)/stand/lib/common/standalloc.c

# ukadb.o is the a.out for all of kadb except the macro file pf.o,
# this makes it is easier to drop in a different set of macros.

ukadb.o: ${AOBJ} ${OBJ} $(ADBOBJ)
	${LD} -r -o $@ ${AOBJ} ${OBJ} $(ADBOBJ)

$(ADBOBJ): FRC
	@cd $(@D); pwd; $(MAKE) -e "ARCHOPTS=$(ARCHOPTS)" $(@F); pwd

$(LIBSA) $(LIBBOOT) $(LIBNAMES) $(KADBPROMLIB) $(PLATLIB) $(LIBNFS): FRC
	@cd $(@D); pwd; $(MAKE) $(@F)

$(L_LIBSA) $(L_LIBBOOT) $(L_LIBNAMES) $(L_PROMLIB) $(L_PLATLIB) \
$(L_LIBNFS): FRC
	@cd $(@D); pwd; $(MAKE) $(@F)

$(AOBJ): $(ARCHOBJ) $(MACHOBJ)
	$(LD) -r -o $@ $(ARCHOBJ) $(MACHOBJ)

$(OBJSDIR):
	-@[ -d $@ ] || mkdir $@

#
# Lint
#
L_OBJS=	$(OBJ:%.o=%.ln) $(ARCHOBJ:%.o=%.ln) $(MACHOBJ:%.o=%.ln)

$(PROG)_lint: $(OBJSDIR) .WAIT $(L_OBJS) $$(L_LIBS) $$(L_OTHERLIBS)
	@$(ECHO) "\nlint library construction:"
	@$(LINT) $(LINTFLAGS.lib) -o kadb $(L_OBJS)
	$(LINT.2) $(CFLAGS) $(L_OBJS) $(L_LDLIBS) $(L_OTHERLIBS)

lint:	$(PROG)_lint

# don't strip to make patching variables easier
#
install: $(ALL) $(ROOT_PSM_PROG) $(ROOT_PSM_PROG_LINKS)

$(ROOTDIR):
	$(INS.dir)

clean:	FRC
	$(RM) genassym assym.s *.o *.ln genpf pf.c errs
	@cd $(ADBDIR)/$(ARCH); pwd; $(MAKE) clobber
	@cd $(MACDIR)/platform/$(MACH)/$(ARCH); pwd; $(MAKE) clobber

clobber: clean FRC
	$(RM) kadb.o kadb

pf.o:	pf.c
	$(COMPILE.c) pf.c

# pf.c is separately dependent upon MACDIR source and genpf.
# genpf need not be rebuilt when MACDIR source is touched.
# hence, first a target then a further dependency.
#
pf.c:	FRC
	@cd ${MACDIR}; pwd; $(MAKE) $(MACH)
	@cd ${MACDIR}/platform/$(MACH); pwd; \
		$(MAKE) "ARCHOPTS=$(ARCHOPTS)" $(ARCH); pwd
	$(RM) $@; ./genpf ${MACLIST}

pf.c:	genpf

genpf:	${COMDIR}/genpf.c
	(unset LD_LIBRARY_PATH; \
	$(NATIVECC) -O -o $@ $(CFLAGS) $(CPPFLAGS) ${COMDIR}/genpf.c)


#
# The cscope.out file is made in the current directory and spans
# all architectures.
# Things to note:
#	1. We use relative names for cscope and tags.
#	2. We *don't* remove the old cscope.out file, because cscope is
#	   smart enough to only build what has changed.  It can be
#	   confused, however, if files are renamed or removed, so it may
#	   be necessary to manually remove cscope.out if a lot of
#	   reorganization has occured.
#	3. We deliberately avoid names that include '.del' in their names
#	   as these are usually files that have been 'deleted' by nsefile.
#
CSDIR	= .

CSDIRS	= $(MACH_CSDIRS) $(COMDIR) . \
	$(TOPDIR)/cmd/adb/$(CSMACH) $(TOPDIR)/cmd/adb/common \
	$(TOPDIR)/stand/lib/common \
	$(TOPDIR)/stand/lib/fs \
	$(TOPDIR)/stand/lib/$(CSMACH) \
	$(TOPDIR)/stand/sys

CSPATHS	= $(CSDIRS:%=$(CSDIR)/%)

CSENVS	= $(ENVCPPFLAGS1) $(ENVCPPFLAGS2) $(ENVCPPFLAGS3) $(ENVCPPFLAGS4)

CSINCS	= $(CPPINCS:%$(MACH)=%$(CSMACH)) \
	  $(CSENVS:%$(MACH)/usr/include=%$(CSMACH)/usr/include)

CSCOPE	= cscope
CSFLAGS	= -b
CTAGS	= ctags

.PRECIOUS:	cscope.out

cscope.out: cscope.files
	${CSCOPE} ${CSFLAGS}

cscope.files:   FRC
	@-$(RM) cscope.files
	echo $(CSINCS) > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
	    \( -type d -name '.del*' \) -prune -o -type f \
	    \( -name '*.[chs]' -o -name 'Makefile*' -o -name '*.il' \) \
	    -a ! -name '.del*' -print >> cscope.files
	@wc -l cscope.files

#
# Create a tags data base, similar to above.
# Since assembler files now contain C fragments for lint,
# the lint fragments will allow ctags to "work" on assembler.
#
# Things to note:
#	1. We put plain files before headers, and .c before .s (because
#	   ctags doesn't understand assembly comments).
#	2. We *don't* sort the output of find, as we want functions in
#	   architecture directories to take precedence over those in
#	   sun, and those over those in common.
#
tags:	cscope.files
	$(CTAGS) -wt `sed 1d cscope.files`

FRC:

include $(TOPDIR)/Makefile.psm.targ
