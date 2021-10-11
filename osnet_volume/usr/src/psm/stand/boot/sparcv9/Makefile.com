#
#ident	"@(#)Makefile.com	1.6	99/10/04 SMI"
#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/boot/sparcv9/Makefile.com


include $(TOPDIR)/psm/stand/boot/Makefile.boot

TARG_MACH	= sparcv9

BOOTSRCDIR	= ../..

CMN_DIR		= $(BOOTSRCDIR)/common
MACH_DIR	= ../../sparc/common
PLAT_DIR	= .

CONF_SRC	= fsconf.c hsfsconf.c

CMN_C_SRC	= boot.c heap_kmem.c readfile.c bootflags.c

MACH_C_SRC	= boot_plat.c bootops.c bootprop.c boot_services.c
MACH_C_SRC	+= get.c

CONF_OBJS	= $(CONF_SRC:%.c=%.o)
CONF_L_OBJS	= $(CONF_OBJS:%.o=%.ln)

SRT0_OBJ	= $(SRT0_S:%.s=%.o)
SRT0_L_OBJ	= $(SRT0_OBJ:%.o=%.ln)

C_SRC		= $(CMN_C_SRC) $(MACH_C_SRC) $(ARCH_C_SRC) $(PLAT_C_SRC)
S_SRC		= $(MACH_S_SRC) $(ARCH_S_SRC) $(PLAT_S_SRC)

OBJS		= $(C_SRC:%.c=%.o) $(S_SRC:%.s=%.o)
L_OBJS		= $(OBJS:%.o=%.ln)

CPPDEFS		= $(ARCHOPTS) -D$(PLATFORM) -D_BOOT -D_KERNEL -D_MACHDEP
CPPDEFS		+= -D_ELF64_SUPPORT
CPPINCS		+= -I$(SRC)/uts/common
CPPINCS		+= -I$(SRC)/uts/sun
CPPINCS		+= -I$(SRC)/uts/$(PLATFORM)
CPPINCS		+= -I$(SRC)/uts/sparc/$(ARCHVERS)
CPPINCS		+= -I$(SRC)/uts/sparc
CPPINCS		+= -I$(SRC)/uts/$(ARCHMMU)
CPPINCS		+= -I$(ROOT)/usr/platform/$(PLATFORM)/include
CPPINCS		+= -I$(ROOT)/usr/include/$(ARCHVERS)
CPPINCS		+= -I$(PSMSYSHDRDIR) -I$(STANDDIR)
CPPFLAGS	= $(CPPDEFS) $(CPPINCS) $(CPPFLAGS.master)
CPPFLAGS	+= $(CCYFLAG)$(STANDDIR)
ASFLAGS		+= $(CPPDEFS) -P -D_ASM $(CPPINCS)
#
# XXX	Should be globally enabled!
CFLAGS64		+= -v
CFLAGS64		+= ../../sparc/common/sparc.il

#
# Until we are building on a MACH=sparcv9 machine, we have to override
# where to look for libraries.
#
SYSLIBDIR	= $(STANDDIR)/lib/$(TARG_MACH)
PSMNAMELIBDIR	= $(PSMSTANDDIR)/lib/names/$(TARG_MACH)
PSMPROMLIBDIR	= $(PSMSTANDDIR)/lib/promif/$(TARG_MACH)

#
# The following libraries are built in LIBNAME_DIR
#
LIBNAME_DIR     += $(PSMNAMELIBDIR)/$(PLATFORM)
LIBNAME_LIBS    += libnames.a
LIBNAME_L_LIBS  += $(LIBNAME_LIBS:lib%.a=llib-l%.ln)

#
# The following libraries are built in LIBPROM_DIR
#
LIBPROM_DIR     += $(PSMPROMLIBDIR)/$(PROMVERS)/common
LIBPROM_LIBS    += libprom.a
LIBPROM_L_LIBS  += $(LIBPROM_LIBS:lib%.a=llib-l%.ln)

#
# The following libraries are built in LIBSYS_DIR
#
LIBSYS_DIR      += $(SYSLIBDIR)
LIBSYS_LIBS     += libsa.a libufs.a libhsfs.a libnfs.a libcachefs.a
LIBSYS_L_LIBS   += $(LIBSYS_LIBS:lib%.a=llib-l%.ln)

#
# Used to convert ELF to an a.out and ensure alignment
#
STRIPALIGN = stripalign

#
# Program used to post-process the ELF executables
#
ELFCONV	= ./$(STRIPALIGN)			# Default value

.KEEP_STATE:

.PARALLEL:	$(OBJS) $(CONF_OBJS) $(SRT0_OBJ)
.PARALLEL:	$(L_OBJS) $(CONF_L_OBJS) $(SRT0_L_OBJ)
.PARALLEL:	$(UFSBOOT) $(HSFSBOOT) $(NFSBOOT)

all: $(ELFCONV) $(UFSBOOT) $(HSFSBOOT) $(NFSBOOT)

$(STRIPALIGN): $(CMN_DIR)/$$(@).c
	$(NATIVECC) -o $@ $(CMN_DIR)/$@.c

# 4.2 ufs filesystem booter
#
# Libraries used to build ufsboot
#
LIBUFS_LIBS     = libufs.a libnfs.a libprom.a libnames.a libsa.a \
		libcachefs.a $(LIBPLAT_LIBS)
LIBUFS_L_LIBS   = $(LIBUFS_LIBS:lib%.a=llib-l%.ln)
UFS_LIBS        = $(LIBUFS_LIBS:lib%.a=-l%)
UFS_DIRS        = $(LIBNAME_DIR:%=-L%) $(LIBSYS_DIR:%=-L%)
UFS_DIRS        += $(LIBPLAT_DIR:%=-L%) $(LIBPROM_DIR:%=-L%)

LIBDEPS=	$(SYSLIBDIR)/libufs.a \
		$(SYSLIBDIR)/libhsfs.a \
		$(SYSLIBDIR)/libnfs.a \
		$(SYSLIBDIR)/libcachefs.a \
		$(SYSLIBDIR)/libsa.a \
		$(LIBPROM_DIR)/libprom.a $(LIBPLAT_DEP) \
		$(LIBNAME_DIR)/libnames.a

L_LIBDEPS=	$(SYSLIBDIR)/llib-lufs.ln \
		$(SYSLIBDIR)/llib-lhsfs.ln \
		$(SYSLIBDIR)/llib-lnfs.ln \
		$(SYSLIBDIR)/llib-lcachefs.ln \
		$(SYSLIBDIR)/llib-lsa.ln \
		$(LIBPROM_DIR)/llib-lprom.ln $(LIBPLAT_DEP_L) \
		$(LIBNAME_DIR)/llib-lnames.ln

#
# Loader flags used to build ufsboot
#
UFS_MAPFILE	= $(MACH_DIR)/mapfile
UFS_LDFLAGS	= -dn -M $(UFS_MAPFILE) -e _start $(UFS_DIRS)
UFS_L_LDFLAGS	= $(UFS_DIRS)

#
# Object files used to build ufsboot
#
UFS_SRT0        = $(SRT0_OBJ)
UFS_OBJS        = $(OBJS) fsconf.o
UFS_L_OBJS      = $(UFS_SRT0:%.o=%.ln) $(UFS_OBJS:%.o=%.ln)

#
# Build rules to build ufsboot
#
$(UFSBOOT).elf: $(UFS_MAPFILE) $(UFS_SRT0) $(UFS_OBJS) $(LIBDEPS)
	$(LD) $(UFS_LDFLAGS) -o $@ $(UFS_SRT0) $(UFS_OBJS) $(UFS_LIBS)
	$(MCS) -d $@
	$(POST_PROCESS)
	$(POST_PROCESS)
	$(MCS) -c $@

$(UFSBOOT): $(UFSBOOT).elf
	$(RM) $@; cp $@.elf $@
	$(STRIP) $@

$(UFSBOOT)_lint: $(L_LIBDEPS) $(UFS_L_OBJS)
	@echo ""
	@echo ufsboot lint: global crosschecks:
	$(LINT.2) $(UFS_L_LDFLAGS) $(UFS_L_OBJS) $(UFS_LIBS)

# High-sierra filesystem booter.  Probably doesn't work.

#
# Libraries used to build hsfsboot
#
LIBHSFS_LIBS    = libhsfs.a libprom.a libnames.a libsa.a $(LIBPLAT_LIBS)
LIBHSFS_L_LIBS  = $(LIBHSFS_LIBS:lib%.a=llib-l%.ln)
HSFS_LIBS       = $(LIBHSFS_LIBS:lib%.a=-l%)
HSFS_DIRS       = $(LIBNAME_DIR:%=-L%) $(LIBSYS_DIR:%=-L%)
HSFS_DIRS       += $(LIBPLAT_DIR:%=-L%) $(LIBPROM_DIR:%=-L%)

#
# Loader flags used to build hsfsboot
#
HSFS_MAPFILE	= $(MACH_DIR)/mapfile
HSFS_LDFLAGS	= -dn -M $(HSFS_MAPFILE) -e _start $(HSFS_DIRS)
HSFS_L_LDFLAGS	= $(HSFS_DIRS)

#
# Object files used to build hsfsboot
#
HSFS_SRT0       = $(SRT0_OBJ)
HSFS_OBJS       = $(OBJS) hsfsconf.o
HSFS_L_OBJS     = $(HSFS_SRT0:%.o=%.ln) $(HSFS_OBJS:%.o=%.ln)

$(HSFSBOOT).elf: $(HSFS_MAPFILE) $(HSFS_SRT0) $(HSFS_OBJS) $(LIBDEPS)
	$(LD) $(HSFS_LDFLAGS) -o $@ $(HSFS_SRT0) $(HSFS_OBJS) $(HSFS_LIBS)
	$(MCS) -d $@
	$(POST_PROCESS)
	$(POST_PROCESS)
	$(MCS) -c $@

$(HSFSBOOT): $(HSFSBOOT).elf
	$(RM) $(@); cp $@.elf $@
	$(STRIP) $@

$(HSFSBOOT)_lint: $(HSFS_L_OBJS) $(L_LIBDEPS)
	@echo ""
	@echo hsfsboot lint: global crosschecks:
	$(LINT.2) $(HSFS_L_LDFLAGS) $(HSFS_L_OBJS) $(HSFS_LIBS)

# NFS version 2 over UDP/IP booter

#
# Libraries used to build nfsboot
#
LIBNFS_LIBS     = libnfs.a libufs.a libprom.a libnames.a libsa.a \
		 libcachefs.a $(LIBPLAT_LIBS)
LIBNFS_L_LIBS   = $(LIBNFS_LIBS:lib%.a=llib-l%.ln)
NFS_LIBS        = $(LIBNFS_LIBS:lib%.a=-l%)
NFS_DIRS        = $(LIBNAME_DIR:%=-L%) $(LIBSYS_DIR:%=-L%)
NFS_DIRS        += $(LIBPLAT_DIR:%=-L%) $(LIBPROM_DIR:%=-L%)

#
# Loader flags used to build inetboot
#
NFS_MAPFILE	= $(MACH_DIR)/mapfile.inet
NFS_LDFLAGS	= -dn -M $(NFS_MAPFILE) -e _start $(NFS_DIRS)
NFS_L_LDFLAGS	= $(NFS_DIRS)

#
# Object files used to build inetboot
#
NFS_SRT0        = $(SRT0_OBJ)
NFS_OBJS        = $(OBJS) fsconf.o
NFS_L_OBJS      = $(NFS_SRT0:%.o=%.ln) $(NFS_OBJS:%.o=%.ln)

$(NFSBOOT).elf: $(ELFCONV) $(NFS_MAPFILE) $(NFS_SRT0) $(NFS_OBJS) $(LIBDEPS)
	$(LD) $(NFS_LDFLAGS) -o $@ $(NFS_SRT0) $(NFS_OBJS) $(NFS_LIBS)
	$(MCS) -d $@
	$(POST_PROCESS)
	$(POST_PROCESS)
	$(MCS) -c $@

#
# This is a bit strange because some platforms boot elf and some don't.
# So this rule strips the file no matter which ELFCONV is used.
#
$(NFSBOOT): $(NFSBOOT).elf
	$(RM) $@.tmp; cp $@.elf $@.tmp; $(STRIP) $@.tmp
	$(RM) $@; $(ELFCONV) $@.tmp $@; $(RM) $@.tmp

$(NFSBOOT)_lint: $(NFS_L_OBJS) $(L_LIBDEPS)
	@echo ""
	@echo nfsboot lint: global crosschecks:
	$(LINT.2) $(NFS_L_LDFLAGS) $(NFS_L_OBJS) $(NFS_LIBS)

include $(BOOTSRCDIR)/Makefile.rules

clean:
	$(RM) $(OBJS) $(CONF_OBJS) make.out lint.out
	$(RM) $(SRT0_OBJ) $(NFSBOOT).elf $(UFSBOOT).elf $(HSFSBOOT).elf
	$(RM) $(L_OBJS) $(CONF_L_OBJS)
	$(RM) $(SRT0_L_OBJ)

clobber: clean
	$(RM) $(UFSBOOT) $(HSFSBOOT) $(NFSBOOT) $(STRIPALIGN)

lint: $(UFSBOOT)_lint $(HSFSBOOT)_lint $(NFSBOOT)_lint

include $(BOOTSRCDIR)/Makefile.targ

FRC:
