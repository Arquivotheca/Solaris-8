#
#ident	"@(#)Makefile.com	1.7	93/09/21 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/adb/sparc/kadb/Makefile.com
#
# to be included by kernel-architecture makefiles in local subdirectories.
# The builds in those subdirectories permit the NSE to track separate
# kernel-architecture dependencies
#

PROG= 		../$(ARCH)-kadb.o

COMMON=		../../../common
I386=		../..
SYSDIR=		../../../../../uts
ARCHDIR=	${SYSDIR}/${ARCH}
MACHDIR=	$(SYSDIR)/$(MACH)
MMUDIR=		$(SYSDIR)/$(MMU)
HEADDIR=	../../../../../head

OBJ_TARGET=	adb_ptrace.o accessir.o opsetir.o setupir.o \
		runpcs.o opcodetable.o printir.o command_$(MACH).o
OBJ_COM=	access.o command.o expr.o fio.o format.o input.o \
		output.o pcs.o print.o sym.o
OBJS=		$(OBJ_COM) $(OBJ_TARGET)

SRC_TARGET=	$(OBJ_TARGET:%.o=%.c)
SRC_COM=	$(OBJ_COM:%.o=$(COMMON)/%.c)
SRCS=		$(SRC_TARGET) $(SRC_COM)

include ../../../../Makefile.cmd

# override default ARCH value
#ARCH=	$(KARCH)

KREDEFS= -Dprintf=_printf -Dopenfile=_openfile -Dexit=_exit \
	-Dopen=_open -Dclose=_close -Dlseek=_lseek -Dread=_read	\
	-Dwrite=_write -Dsetjmp=_setjmp -Dlongjmp=_longjmp

CPPINCS=	-I${I386} -I${COMMON} -I$(SYSDIR)/i86pc -I$(SYSDIR)/i86 \
		-I${SYSDIR}/common -I$(HEADDIR)

CPPFLAGS=	-D$(MACH) -D$(ARCH) $(CPPINCS) -D_KERNEL -D_MACHDEP \
		-D_KADB -DKADB -D__ELF ${KREDEFS} $(ARCHOPTS) $(CPPFLAGS.master)

# build rules
%.o : $(COMMON)/%.c
	$(COMPILE.c) $<

%.o : $(I386)/%.c
	$(COMPILE.c) $<

.KEEP_STATE:

.PARALLEL:	$(OBJS)

all:	$(PROG)

$(PROG): $(OBJS)
	$(LD) -r -o $@ $(OBJS) 

install:

clean:
	$(RM) ${OBJS}

link:	lint_SRCS

include	../../../../Makefile.targ
