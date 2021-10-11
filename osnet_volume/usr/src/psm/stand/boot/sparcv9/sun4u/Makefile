#
#ident	"@(#)Makefile	1.28	99/10/07 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#

TOPDIR =	../../../../..

include $(TOPDIR)/uts/Makefile.uts

SUBDIRS = conf
MODULE	= fs
PLATLINKS	= SUNW,Ultra-1 SUNW,Ultra-250 SUNW,Ultra-4 SUNW,Ultra-Enterprise SUNW,Ultra-Enterprise-10000
LINKED_DIRS	= $(PLATLINKS:%=$(USR_PLAT_DIR)/%)
LINKED_LIB_DIRS	= $(PLATLINKS:%=$(USR_PLAT_DIR)/%/lib)
LINKED_LIB_FS_DIRS	= $(PLATLINKS:%=$(USR_PLAT_DIR)/%/lib/fs)

all	:=	TARGET = all
install	:=	TARGET = install
clean	:=	TARGET = clean

all install clean: $(SUBDIRS)

$(SUBDIRS):	FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

TARG_MACH =	sparcv9
TARG_MACH_DIR =	sparcv9
ARCHVERS =	v9
PLATFORM =	sun4u
ARCHMMU  =	sfmmu
PROMVERS =	ieee1275
ARCHOPTS =	-D__sparcv9cpu
ASFLAGS +=	$(sparcv9_XARCH)

PLAT_C_SRC =	machdep.c
ARCH_C_SRC =	policy.o sun4u_memlist.c sun4x_standalloc.c
ARCH_S_SRC =	sparcv9_subr.s boot_1275entry.s
SRT0_S =	sun4u_srt0.s
INLINES =

LDFLAGS +=	-L$(TOPDIR)/psm/stand/lib/promif/$(TARG_MACH)/$(PROMVERS)/common

#
# The following libraries are build in LIBPLAT_DIR
#
LIBPLAT_DIR =	$(TOPDIR)/psm/stand/lib/promif/$(TARG_MACH)/$(PROMVERS)/$(PLATFORM)
LIBPLAT_LIBS =  libplat.a
LIBPLAT_L_LIBS=	$(LIBPLAT_LIBS:lib%.a=llib-l%.ln)
LIBPLAT_DEP = 	$(LIBPLAT_DIR)/$(LIBPLAT_LIBS)
LIBPLAT_DEP_L =	$(LIBPLAT_DIR)/$(LIBPLAT_L_LIBS)

#
# Platform specific libraries
#
PSMLIBS +=      $(LIBPLAT_LIBS:lib%.a=-l%)
PSMLIB_DIRS +=  $(LIBPLAT_DIR)


#	MPSAS support
MPSAS_BUILD:sh=		echo \\043
$(MPSAS_BUILD)ARCHOPTS += -DMPSAS

include ../Makefile.com

# re-define ELFCONV for 4u--- sun4u can boot ELF directly
ELFCONV = /usr/bin/cp

# Don't need mapfile.inet for 4u inetboot
NFS_MAPFILE = $(MACH_DIR)/mapfile

#
# Set the choice of compiler. 

include $(TOPDIR)/psm/Makefile.psm.64

#
# The cscope.out file is made in the current directory and spans all
# sparc platforms and boot fs types.
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
UTSDIR=		../../../../../uts
STANDLIBDIR=	../../../../../stand/lib
STANDSYSDIR=	../../../../../stand/sys
PROMDIRS=	../../../../promif
NAMESDIRS=	../../../lib/names/sparcv9 ../../../lib/names/sparc/common
CSMACH=		sparc
CSDIR=		.
CSDIRS=		../../sparc/common ../../common $(STANDLIBDIR)/common \
		$(STANDLIBDIR)/fs \
		$(STANDLIBDIR)/$(CSMACH) \
		$(STANDSYSDIR) $(PROMDIRS) $(NAMESDIRS) $(CSDIR)
CSPATHS=	$(CSDIRS:%=$(CSDIR)/%)
CSENVS=		$(ENVCPPFLAGS1) $(ENVCPPFLAGS2) $(ENVCPPFLAGS3) $(ENVCPPFLAGS4)
#CSINCS=	$(CSPATHS:%=-I%) -I$(ROOT:%$(MACH)=%$(CSMACH))/usr/include \
#		$(CSENVS:%$(MACH)/usr/include=%$(CSMACH)/usr/include)
CSINCS=		$(CPPINCS)
CSCOPE=		cscope
CSFLAGS=	-b
CTAGS=		ctags

cscope.out: cscope.files FRC
	${CSCOPE} ${CSFLAGS}

cscope.files:   FRC
	@-$(RM) cscope.files
	echo $(CSINCS) > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
	    \( -type d -name '.del*' \) -prune -o -type f \
	    \( -name '*.[chs]' -o -name 'Makefile*' -o \
	    -name 'mapfile*' -o -name '*.il' \) \
	    -a ! -name '.del*' -print | grep -v prep | \
	    grep -v sun4m | grep -v obp | \
	    grep -v sun4x_srt0 | grep -v sun4x_memlist >> cscope.files
	@wc -l cscope.files

#
# Create a tags data base, similar to above.
# Since many assembler files now contain C fragments for lint,
# the lint fragments will allow ctags to "work" on assembler.
#
# Things to note:
#	1. We put plain files before headers, and .c before .s (because
#	   ctags doesn't understand assembly comments).
#	2. We *don't* sort the output of find, as we want functions in
#	   architecture directories to take precedence over those in
#	   sun, and those over those in common.
#
tags: cscope.files
	${CTAGS} -wt `sed 1d cscope.files`

FRC:
