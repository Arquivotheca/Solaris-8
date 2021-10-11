#
#pragma ident	"@(#)Makefile.com	1.40	99/11/03 SMI"
#
# Copyright (c) 1992-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/boot/i386/Makefile.com

include $(TOPDIR)/psm/stand/boot/Makefile.boot

BOOTSRCDIR	= ../..

CMN_DIR		= $(BOOTSRCDIR)/common
MACH_DIR	= ../common
ACPI_DIR	= $(TOPDIR)/uts/intel/os/acpi_intp
PLAT_DIR	= .
PIM_DIR		= $(BOOTSRCDIR)/intel

ACPI_UTS_INC	= $(TOPDIR)/uts/intel
ACPI_PROTO_INC	= $(ROOT)/usr/include

CONF_SRC	= fsconf.c

CMN_C_SRC	= boot.c heap_kmem.c readfile.c bootflags.c
PIM_C_SRC	= bootprop.c devtree.c

MACH_C_SRC	= a20enable.c arg.c ata.c boot_plat.c bootops.c i386_bootprop.c
MACH_C_SRC	+= bsetup.c bsh.c bus_probe.c cbus.c chario.c cmds.c
MACH_C_SRC	+= cmdtbl.c compatboot.c debug.c delayed.c i386_devtree.c disk.c
MACH_C_SRC	+= dosbootop.c dosdirs.c dosemul.c dosmem.c env.c
MACH_C_SRC	+= expr.c gets.c help.c i386_memlist.c i386_standalloc.c
MACH_C_SRC	+= if.c initsrc.c ix_alts.c
MACH_C_SRC	+= keyboard.c
MACH_C_SRC	+= keyboard_table.c
MACH_C_SRC	+= load.c map.c memory.c misc_utls.c
MACH_C_SRC	+= net.c probe.c prom.c prop.c setcolor.c src.c var.c
MACH_C_SRC	+= vga.c
MACH_C_SRC	+= acpi_pm.c

MACH_Y_SRC	= exprgram.y

ACPI_GCOM	= acpi_gcom
ACPI_GRAM	= $(ACPI_DIR)/acpi_gram.c

# ACPI boot interface sources
ACPI_C_SRC	= acpi_boot.c acpi_inf.c acpi_bad.c
# ACPI interpreter sources
ACPI_C_SRC	+= acpi_decl.c acpi_exe.c acpi_gram.c acpi_io.c acpi_lex.c
ACPI_C_SRC	+= acpi_name.c acpi_ns.c acpi_op1.c acpi_op2.c acpi_rule.c
ACPI_C_SRC	+= acpi_tab.c acpi_thr.c acpi_val.c
# ACPI parser engine sources
ACPI_C_SRC	+= acpi_exc.c acpi_bst.c acpi_node.c acpi_stk.c acpi_par.c

ACPI_S_SRC	= acpi_ml.s

ACPI_OBJS	= $(ACPI_C_SRC:%.c=%.o) $(ACPI_S_SRC:%.s=%.o)
ACPI_L_OBJS	= $(ACPI_C_SRC:%.c=%.ln) $(ACPI_S_SRC:%.s=%.ln)

ACPI_GCOM_SRC	= acpi_gcom.c acpi_gcom_exc.c

CONF_OBJS	= $(CONF_SRC:%.c=%.o)
CONF_L_OBJS	= $(CONF_OBJS:%.o=%.ln)

ACPI_GCOM_OBJS	= $(ACPI_GCOM_SRC:%.c=%.o)
ACPI_GCOM_L_OBJS = $(ACPI_GCOM_OBJS:%.o=%.ln)

SRT0_OBJ	= $(SRT0_S:%.s=%.o)

C_SRC		= $(CMN_C_SRC) $(MACH_C_SRC) $(ARCH_C_SRC) $(PLAT_C_SRC)
C_SRC		+= $(PIM_C_SRC)  $(ACPI_C_SRC)
S_SRC		= $(MACH_S_SRC) $(ARCH_S_SRC) $(PLAT_S_SRC) $(ACPI_S_SRC)
Y_SRC		= $(MACH_Y_SRC)

OBJS		= $(C_SRC:%.c=%.o) $(S_SRC:%.s=%.o) $(Y_SRC:%.y=%.o)
L_OBJS		= $(OBJS:%.o=%.ln)

ELFCONV =	mkbin
INETCONV =	mkinet

UNIBOOT =	boot.bin

#
#  On x86, ufsboot is now delivered as the unified booter, boot.bin.
#  This delivery change is only happening on x86, otherwise we wouldn't
#  need to modify the macro for the ufsboot delivery item, instead
#  we would change the delivery item to be the unified booter at a
#  higher level in the Makefile hierarchy.
#
ROOT_PSM_UFSBOOT = $(ROOT_PSM_UNIBOOT)

.KEEP_STATE:

.PARALLEL:	$(OBJS) $(L_OBJS) $(SRT0_OBJ) .WAIT \
		$(UFSBOOT) $(NFSBOOT)

all: $(ELFCONV) $(UFSBOOT) $(INETCONV) $(NFSBOOT)

install: all

SYSDIR	=	$(TOPDIR)/uts
STANDSYSDIR=	$(TOPDIR)/stand
SYSLIBDIR=	$(TOPDIR)/stand/lib/$(MACH)

CPPDEFS		= $(ARCHOPTS) -D$(PLATFORM) -D_BOOT -D_KERNEL -D_MACHDEP
CPPINCS		+= -I. -I$(PIM_DIR)
CPPINCS		+= -I$(PSMSYSHDRDIR) -I$(STANDDIR)/$(MACH) -I$(STANDDIR)
CPPINCS		+= -I$(ROOT)/usr/platform/$(PLATFORM)/include
CPPINCS		+= -I$(TOPDIR)/uts/intel

CPPFLAGS	= $(CPPDEFS) $(CPPFLAGS.master) $(CPPINCS)
CPPFLAGS	+= $(CCYFLAG)$(SYSDIR)/common
ASFLAGS =	$(CPPDEFS) -P -D__STDC__ -D_BOOT -D_ASM $(CPPINCS)

CFLAGS	=	../common/i86.il -O
#
# This should be globally enabled!
#
CFLAGS	+=	-v

YFLAGS	=	-d

#LDFLAGS		+= -L$(PSMNAMELIBDIR) -L$(SYSLIBDIR)
#LDLIBS		+= -lnames -lsa -lprom

#
# The following libraries are built in LIBNAME_DIR
#
LIBNAME_DIR     += $(PSMNAMELIBDIR)
LIBNAME_LIBS    += libnames.a
LIBNAME_L_LIBS  += $(LIBNAME_LIBS:lib%.a=llib-l%.ln)

#
# The following libraries are built in LIBPROM_DIR
#
LIBPROM_DIR     += $(PSMPROMLIBDIR)/$(PROMVERS)
LIBPROM_LIBS    += libprom.a
LIBPROM_L_LIBS  += $(LIBPROM_LIBS:lib%.a=llib-l%.ln)

#
# The following libraries are built in LIBSYS_DIR
#
LIBSYS_DIR      += $(SYSLIBDIR)
LIBSYS_LIBS     += libsa.a libufs.a libnfs.a libpcfs.a libcompfs.a
LIBSYS_LIBS     += libhsfs.a libcachefs.a
LIBSYS_L_LIBS   += $(LIBSYS_LIBS:lib%.a=llib-l%.ln)

$(ELFCONV): $(MACH_DIR)/$$(@).c
	$(NATIVECC) -O -o $@ $(MACH_DIR)/$@.c

$(INETCONV): $(MACH_DIR)/$$(@).c
	$(NATIVECC) -O -o $@ $(MACH_DIR)/$@.c

#
# Unified booter:
#	4.2 ufs filesystem
#	nfs
#	hsfs
#	cachefs
#
LIBUNI_LIBS	= libcompfs.a libpcfs.a libufs.a libnfs.a libhsfs.a
LIBUNI_LIBS	+= libprom.a libnames.a libsa.a libcachefs.a $(LIBPLAT_LIBS)
LIBUNI_L_LIBS	= $(LIBUNI_LIBS:lib%.a=llib-l%.ln)
UNI_LIBS	= $(LIBUNI_LIBS:lib%.a=-l%)
UNI_DIRS	= $(LIBNAME_DIR:%=-L%) $(LIBSYS_DIR:%=-L%)
UNI_DIRS	+= $(LIBPLAT_DIR:%=-L%) $(LIBPROM_DIR:%=-L%)

LIBDEPS=	$(SYSLIBDIR)/libcompfs.a \
		$(SYSLIBDIR)/libpcfs.a \
		$(SYSLIBDIR)/libufs.a \
		$(SYSLIBDIR)/libnfs.a \
		$(SYSLIBDIR)/libhsfs.a \
		$(SYSLIBDIR)/libcachefs.a \
		$(SYSLIBDIR)/libsa.a \
		$(LIBPROM_DIR)/libprom.a \
		$(LIBNAME_DIR)/libnames.a

L_LIBDEPS=	$(SYSLIBDIR)/llib-lcompfs.ln \
		$(SYSLIBDIR)/llib-lpcfs.ln \
		$(SYSLIBDIR)/llib-lufs.ln \
		$(SYSLIBDIR)/llib-lnfs.ln \
		$(SYSLIBDIR)/llib-lhsfs.ln \
		$(SYSLIBDIR)/llib-lcachefs.ln \
		$(SYSLIBDIR)/llib-lsa.ln \
		$(LIBPROM_DIR)/llib-lprom.ln \
		$(LIBNAME_DIR)/llib-lnames.ln

#
# Loader flags used to build unified boot
#
UNI_LOADMAP	= loadmap
UNI_MAPFILE	= $(MACH_DIR)/mapfile
UNI_LDFLAGS	= -dn -m -M $(UNI_MAPFILE) -e _start $(UNI_DIRS)
UNI_L_LDFLAGS	= $(UNI_DIRS)

#
# Object files used to build unified boot
#
UNI_SRT0	= $(SRT0_OBJ)
UNI_OBJS	= $(OBJS) fsconf.o
UNI_L_OBJS	= $(UNI_SRT0:%.o=%.ln) $(UNI_OBJS:%.o=%.ln)

$(UNIBOOT): $(ELFCONV) $(UNI_MAPFILE) $(UNI_SRT0) $(UNI_OBJS) $(LIBDEPS)
	$(LD) $(UNI_LDFLAGS) -o $@.elf $(UNI_SRT0) $(UNI_OBJS) $(LIBDEPS) \
		> $(UNI_LOADMAP)
	cp $@.elf $@.strip
	$(STRIP) $@.strip
	$(RM) $@; ./$(ELFCONV) $@.strip $@

$(UNIBOOT)_lint: $(UNI_L_OBJS) $(L_LIBDEPS)
	$(LINT.2) $(UNI_L_LDFLAGS) $(UNI_L_OBJS) $(UNI_LIBS)
	touch boot.bin_lint

#
# The UFS and NFS booters are simply copies of the unified booter.
#
$(UFSBOOT): $(UNIBOOT)
	cp $(UNIBOOT) $@

$(UFSBOOT)_lint: $(UNIBOOT)_lint
	cp $(UNIBOOT)_lint $@

$(NFSBOOT): $(INETCONV) $(UNIBOOT)
	./$(INETCONV) $(UNIBOOT) $@

$(NFSBOOT)_lint: $(UNIBOOT)_lint
	cp $(UNIBOOT)_lint $@

#
# The directory for boot.bin has changed to /boot/solaris, so we have
# created new rules.
#
ROOT_BOOT_DIR = $(ROOT)/boot
ROOT_BOOT_SOL_DIR = $(ROOT_BOOT_DIR)/solaris

$(ROOT_BOOT_DIR): $(ROOT)
	-$(INS.dir.root.sys)

$(ROOT_BOOT_SOL_DIR): $(ROOT_BOOT_DIR)
	-$(INS.dir.root.sys)

$(ROOT_BOOT_SOL_DIR)/%: % $(ROOT_BOOT_SOL_DIR)
	$(INS.file)

#
#	rules for special ACPI files
#

# grammar compiler
$(ACPI_DIR)/acpi_gcom_exc.c: $(ACPI_DIR)/acpi_exc.c 
	$(RM) $@; $(SYMLINK) acpi_exc.c $@

$(ACPI_GCOM_OBJS): $(ACPI_DIR)/$$(@:.o=.c)
	$(NATIVECC) -c -I$(ACPI_UTS_INC) -I$(ACPI_PROTO_INC) -o $@ $(ACPI_DIR)/$(@:.o=.c)

$(ACPI_GCOM): $(ACPI_DIR)/acpi_gcom_exc.c .WAIT $(ACPI_GCOM_OBJS)
	$(NATIVECC) -o $@ $(ACPI_GCOM_OBJS)

# generated grammar rules
$(ACPI_GRAM): $(ACPI_DIR)/acpi_grammar $(ACPI_GCOM)
	./$(ACPI_GCOM) $(ACPI_DIR)/acpi_grammar $(ACPI_DIR)/acpi_elem.h $(ACPI_DIR)/acpi_gram.c

$(ACPI_OBJS) $(ACPI_L_OBJS): $(ACPI_GCOM) .WAIT $(ACPI_GRAM)

# lint rules
ACPI_GCOM_LINT.c = $(LINT) $(LINTFLAGS) $(LINT_DEFS) -I$(ACPI_UTS_INC) -I$(ACPI_PROTO_INC) -c

$(ACPI_GCOM_L_OBJS): $(ACPI_DIR)/$$(@:.ln=.c)
	@($(LHEAD) $(ACPI_GCOM_LINT.c) $(ACPI_DIR)/$(@:.ln=.c) $(LTAIL))

$(ACPI_GCOM)_lint: $(ACPI_GCOM_L_OBJS)
	$(LINT) -sxmu $(LINT_DEFS) $(ACPI_GCOM_L_OBJS)
	touch $(ACPI_GCOM)_lint

#
# expr.o depends on y.tab.o because expr.c includes y.tab.h,
# which is in turn generated by yacc.
#
expr.o: exprgram.o

#
# yacc automatically adds some #includes that aren't right in our
# stand-alone environment, so we use sed to change the generated C.
#
%.o: $(MACH_DIR)/%.y
	$(YACC.y) $<
	sed -e '/^#include.*<malloc.h>/d'\
		-e '/^#include.*<stdlib.h>/d'\
		-e '/^#include.*<string.h>/d' < y.tab.c > y.tab_incfix.c
	$(COMPILE.c) -o $@ y.tab_incfix.c
	$(POST_PROCESS_O)

%.ln: $(MACH_DIR)/%.y
	@($(YACC.y) $<)
	@(sed -e '/^#include.*<malloc.h>/d'\
		-e '/^#include.*<stdlib.h>/d'\
		-e '/^#include.*<string.h>/d' < y.tab.c > y.tab_incfix.c)
	@($(LHEAD) $(LINT.c) -c y.tab_incfix.c $(LTAIL))
	@(mv y.tab_incfix.ln $@)

include $(BOOTSRCDIR)/Makefile.rules

clean:
	$(RM) $(SRT0_OBJ) $(OBJS) $(CONF_OBJS)
	$(RM) $(L_OBJS) $(CONF_L_OBJS)
	$(RM) y.tab.* exprgram.c a.out core y.tmp.c
	$(RM) $(UNIBOOT).elf $(UNIBOOT).strip
	$(RM) $(ACPI_GCOM) $(ACPI_GCOM_OBJS)
	$(RM) $(ACPI_GCOM_L_OBJS)
	$(RM) $(ACPI_DIR)/acpi_elem.h  $(ACPI_DIR)/acpi_gram.c $(ACPI_DIR)/acpi_gcom_exc.c

clobber: clean
	$(RM) $(ELFCONV) $(INETCONV) $(UNIBOOT) $(UNI_LOADMAP)
	$(RM) $(UFSBOOT) $(HSFSBOOT) $(NFSBOOT) y.tab.c y.tab_incfix.c

lint: $(UFSBOOT)_lint $(NFSBOOT)_lint $(ACPI_GCOM)_lint

include $(BOOTSRCDIR)/Makefile.targ

FRC:
