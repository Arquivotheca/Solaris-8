#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.5	99/04/13 SMI"
#
# psm/stand/kadb/sparcv9/sun4u/starfire/Makefile
#
# a starfire-architecture-specific kadb Makefile
#

# override global macros as necessary
TARGET_ARCH =	sun4u
ARCH =		sun4u
KARCH =		sun4u
MACH=		sparcv9
TARG_MACH=	sparcv9
CSMACH=		sparcv9
KSUN= 		sun
MMU=		sfmmu
ARCHVERS =	v9
PROMTYPE=	ieee1275
PROMLIB=	libprom.a
PLATLIB=	$(PLATDIR)/libplat.a
CSPROM=		$(PROMTYPE)/common $(PROMTYPE)/$(KARCH)
ARCHOPTS=	-D__sparcv9cpu -D__sparcv9 -D_SYSCALL32

ARCHOBJ= $(SPECIAL) savestate.o machstate.o machdep.o physmem.o invoke.o

CFLAGS=		$(CFLAGS64)
CCFLAGS=	$(CCFLAGS64)
CCOPTFLAG=	$(CCOPTFLAG64)
COMPILE.c=	$(COMPILE64.c)
COMPILE.s=	$(COMPILE64.s)
COMPILE.cc=	$(COMPILE64.cc)
LINK.c=		$(LINK64.c)
LINK.cc=	$(LINK64.cc)
LINT.c=		$(LINT64.c)

KADBDIR=	../../..

# include architecture specific kadb definitions
include ../../common/Makefile.sparcv9

# include common kadb definitions
include ../../../common/Makefile.com.64

PLATFORM=	SUNW,Ultra-Enterprise-10000

MAPFILE=	../mapfile

ALWAYS_DEFS	+= -D__sparc -D__sparcv9 -D_SYSCALL32_IMPL
AS_DEFS		+= -D__sparc -D__sparcv9
CPPFLAGS	+= -D__sparc -D__sparcv9 -D_SYSCALL32
ASFLAGS		+= -D__sparc -D__sparcv9
LINTFLAGS64	+= -errchk=longptr64

#
# so we can use privileged V9 instructions
#

ASFLAGS	+=	$(sparcv9_XARCH)
$(OBJSDIR)/%.o:= CFLAGS	+=	$(CFLAGS64)
CCOPTFLAG +=	$(CCOPTFLAG64)

$(ARCHOBJ):=	ARCHOPTS +=	-D_STARFIRE

# include common kadb targets
include ../../../common/Makefile.targ.64

all: $(PROG)

install: $(PROG) $(ROOT_PSM_PROG)

clean clean.lint:	FRC
	$(RM) genassym assym.s *.o *.ln genpf genpf32 pf.c pf32.c errs
	@cd $(ADBDIR)/$(ARCH); pwd; $(MAKE) clean
	@cd ${MACDIR}/macrogen; pwd; $(MAKE) clean
	@cd $(MACDIR)/platform/$(TARG_MACH)/$(ARCH); pwd; $(MAKE) clean

clobber: clean FRC
	$(RM) kadb.o kadb kadbka kadbnetka
	@cd $(ADBDIR)/$(ARCH); pwd; $(MAKE) clobber
	@cd ${MACDIR}/macrogen; pwd; $(MAKE) clobber
	@cd $(MACDIR)/platform/$(TARG_MACH)/$(ARCH); pwd; $(MAKE) clobber

lint:	$(PROG)_lint

# build rules
$(OBJSDIR)/%.o: $(COMDIR)/%.c
	$(COMPILE.c) $<

$(OBJSDIR)/%.o: ../%.c
	$(COMPILE.c) $<

$(OBJSDIR)/%.o: ../../common/%.c
	$(COMPILE.c) $<

locore.o: ../locore.s
	$(COMPILE.s) -o $@ ../locore.s

savestate.o: ../savestate.s
	$(COMPILE.s) -o $@ ../savestate.s

invoke.o: ../invoke.s
	$(COMPILE.s) -o $@ ../invoke.s

physmem.o: ../physmem.s
	$(COMPILE.s) -o $@ ../physmem.s

$(OBJSDIR)/%.ln: $(COMDIR)/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))

$(OBJSDIR)/%.ln: ../%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))

$(OBJSDIR)/%.ln: ../../common/%.c
	@($(LHEAD) $(LINT.c) $< $(LTAIL))

$(OBJSDIR)/%.ln: ../%.s
	@($(LHEAD) $(LINT.s) $< $(LTAIL))

$(ARCHOBJ): assym.s

assym.s:	offsets.out 
	@cp	offsets.out assym.s

genassym: genassym.c
	(unset LD_LIBRARY_PATH; \
	$(NATIVECC) $(CFLAGS) $(CPPINCS) -o $@ genassym.c)

offsets.out:
	@grep "^#" ../offsets.in > offsets.c
	$(CC) $(CFLAGS) $(CPPINCS) -g -S -o offsets.s offsets.c
	@grep -v "^#" ../offsets.in > offsets.tmp
	$(STABS) -t genassym -m ilp32 offsets.tmp < offsets.s > offsets.out
	@rm offsets.s offsets.c offsets.tmp	
