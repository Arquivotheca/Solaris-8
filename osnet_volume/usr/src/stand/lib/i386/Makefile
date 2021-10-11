#
#pragma ident	"@(#)Makefile	1.23	99/05/04 SMI"
#
# Copyright (c) 1990-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# stand/lib/i386/Makefile
#
# Sun/i386-architecture makefile for Standalone Library
#

TOPDIR	=	../../..

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/lib/Makefile.lib

LIBSA=		libsa.a
LIBSA_L=	llib-lsa.ln

LIBKADB=	libsadb.a
LIBKADB_L=	llib-lsadb.ln

LIBUFS=		libufs.a
LIBUFS_L=	llib-lufs.ln

LIBCOMPFS=	libcompfs.a
LIBCOMPFS_L=	llib-lcompfs.ln

LIBPCFS=	libpcfs.a
LIBPCFS_L=	llib-lpcfs.ln

LIBNFS=		libnfs.a
LIBNFS_L=	llib-lnfs.ln

LIBHSFS=	libhsfs.a
LIBHSFS_L=	llib-lhsfs.ln

LIBCACHEFS=	libcachefs.a
LIBCACHEFS_L=	llib-lcachefs.ln

ALL_LIBS=	$(LIBSA) $(LIBUFS) $(LIBCOMPFS) $(LIBPCFS) $(LIBNFS) \
			$(LIBHSFS) $(LIBKADB) $(LIBCACHEFS)

ALL_LIBS_L=	$(LIBSA_L) $(LIBUFS_L) $(LIBCOMPFS_L) $(LIBPCFS_L) \
		$(LIBNFS_L) $(LIBHSFS_L) $(LIBKADB_L) $(LIBCACHEFS_L)

# KARCH is used in place of KERNARCH to override the value in Makefile.master
KARCH=		i86pc
MMU=		i86
MACHDIR=	../i386
STANDDIR=	../..
SYSDIR	=	$(TOPDIR)/uts

CPPDEFS	= 	$(ARCHOPTS) -D$(KARCH) -D_BOOT -D_KADB -D_KERNEL -D_MACHDEP
CPPINCS	= 	$(LOC_CPP) -I. -I$(STANDDIR) -I$(STANDDIR)/$(MACH) \
		-I$(SYSDIR)/$(KARCH) -I$(SYSDIR)/$(MMU) -I$(SYSDIR)/common 
ASINCS	=	$(CPPINCS)
CPPFLAGS=	$(CPPDEFS) $(CPPINCS) $(CPPBOOT) $(CPPFLAGS.master)
AS_CPPFLAGS=	$(CPPFLAGS)
ASFLAGS=	-P -D__STDC__ -D_ASM

FSDIR=		../fs
FSCMNDIR=	$(FSDIR)/common
UFSDIR=		$(FSDIR)/ufs
COMPFSDIR=	$(FSDIR)/compfs
PCFSDIR=	$(FSDIR)/pcfs
NFSDIR=		$(FSDIR)/nfs
HSFSDIR=	$(FSDIR)/hsfs
CACHEFSDIR=	$(FSDIR)/cachefs
DHCP_CMNDIR=	$(TOPDIR)/common/net/dhcp

include $(COMPFSDIR)/Makefile.compfs
include $(UFSDIR)/Makefile.ufs
include $(NFSDIR)/Makefile.nfs
include $(PCFSDIR)/Makefile.pcfs
include $(HSFSDIR)/Makefile.hsfs
include $(CACHEFSDIR)/Makefile.cachefs

CMNDIR=		../common
CMNOBJ=		bcmp.o bcopy.o bzero.o errno.o memlist.o printf.o sprintf.o \
		standalloc.o strcat.o strchr.o strcmp.o strcpy.o \
		strlen.o strncat.o strncmp.o strncpy.o strrchr.o \
		strstr.o ctype.o
CMNOBJ_L=       bcmp.ln bcopy.ln bzero.ln errno.ln memlist.ln printf.ln \
		sprintf.ln standalloc.ln strcat.ln strchr.ln strcmp.ln \
		strcpy.ln strlen.ln strncat.ln strncmp.ln strncpy.ln \
		strrchr.ln strstr.ln ctype.ln
CMNSRC=		$(CMNOBJ:%.o=$(CMNDIR)/%.c)

FSCMNOBJ=	cache.o diskread.o fsswitch.o
FSCMNOBJ_L=	cache.ln diskread.ln fsswitch.ln
FSCMNSRC=	$(FSCMNOBJ:%.o=$(FSCMNDIR)/%.c)

MACHOBJ=	get.o map_prog.o _setjmp.o samuldiv64.o
MACHOBJ_L=	$(MACHOBJ:%.o=%.ln)
MACHSRC=	get.c map_prog.c _setjmp.s samuldiv64.s

GENOBJS=	$(CMNOBJ) $(FSCMNOBJ) $(MACHOBJ)
GENOBJS_L=	$(CMNOBJ_L) $(FSCMNOBJ_L) $(MACHOBJ_L)
PAROBJS=	$(GENOBJS:%=$(OBJSDIR)/%)
PAROBJS_L=	$(GENOBJS_L:%=$(OBJSDIR)/%)

#
# This is to allow parallel make to work ... the current
# version doesn't handle the dynamic dependency in Makefile.lib
#
.PARALLEL:	$(GENOBJS:%.o=objs/%.o)
.PARALLEL:	$(GENOBJS:%.o=kadbobjs/%.o)
.PARALLEL:	$(GENOBJS_L:%.ln=objs/%.ln)
.PARALLEL:	$(GENOBJS_L:%.ln=kadbobjs/%.ln)
.PARALLEL:	$(ALL_LIBS) $(ALL_LIBS_L)

# Arch-specific device configuration as listed in conf.c
CONFOBJ=

#
# OBJECTS are the union of .o's to build the libraries.
# SOURCES are the union of source files to build the lint libraries.
# OBJECTS_L  are the union of .ln's for lint pass 1 on the sources.
#
# The fs/* Makefiles include their own OBJECTS and SOURCES
# in these lists.
#
# SPRINTF is defined in Makefile.lib, depending
# on the library being built.
#
OBJECTS +=	$(GENOBJS) $(SPRINTF.o) 
OBJECTS_L +=	$(GENOBJS_L) $(SPRINTF.ln) 
SOURCES +=	$(CMNSRC) $(FSCMNSRC) $(MACHSRC) $(SPRINTF.c)

include ../Makefile.com

.KEEP_STATE:
