#
#pragma ident	"@(#)Makefile.com	1.39	99/06/23 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.

RTLD=		ld.so.1

BLTOBJ=		msg.o
OBJECTS=	$(BLTOBJ) \
		$(P_ASOBJS)   $(P_COMOBJS)   $(P_MACHOBJS)   $(G_MACHOBJS)  \
		$(S_ASOBJS)   $(S_COMOBJS)   $(S_MACHOBJS)   $(CP_MACHOBJS)

COMOBJS=	$(P_COMOBJS)  $(S_COMOBJS)
ASOBJS=		$(P_ASOBJS)   $(S_ASOBJS)
MACHOBJS=	$(P_MACHOBJS) $(S_MACHOBJS)

ADBGEN1=	/usr/lib/adb/adbgen1
ADBGEN3=	/usr/lib/adb/adbgen3
ADBGEN4=	/usr/lib/adb/adbgen4
ADBSUB=		/usr/lib/adb/adbsub.o

ADBSRCS=	fmap.adb	link_map.adb	rt_map.adb	permit.adb \
		fct.adb		pnode.adb	maps.adb	maps.next.adb \
		dyn.adb		dyn.next.adb

include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

MAPFILE=	../common/mapfile-vers

ADBSCRIPTS=	$(ADBSRCS:%.adb=adb/%)
ROOTADB=	$(ADBSRCS:%.adb=$(SGSONLD)/lib/adb/%)

# A version of this library needs to be placed in /etc/lib to allow
# dlopen() functionality while in single-user mode.

ETCLIBDIR=	$(ROOT)/etc/lib
ETCDYNLIB=	$(RTLD:%=$(ETCLIBDIR)/%)

ROOTDYNLIB=	$(RTLD:%=$(ROOTLIBDIR)/%)
ROOTDYNLIB64=	$(RTLD:%=$(ROOTLIBDIR64)/%)


FILEMODE =	755

# Add -DPRF_RTLD to allow ld.so.1 to profile itself

CPPFLAGS=	-I. -I../common -I../../include -I../../include/$(MACH) \
		-I$(SRCBASE)/lib/libc/inc \
		-I$(SRCBASE)/uts/common/krtld \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		-D__EXTENSIONS__ $(CPPFLAGS.master)
ASFLAGS=	-P -D_ASM $(CPPFLAGS)
SONAME=		/usr/lib/$(RTLD)
DYNFLAGS +=	-e _rt_boot -Bsymbolic -zcombreloc -zlazyload \
		-M $(MAPFILE) '-R$$ORIGIN'

LDLIB =		-L ../../libld/$(MACH)
DBGLIB =	-L ../../liblddbg/$(MACH)
RTLDLIB =	-L ../../librtld/$(MACH)
CONVLIB =	-L ../../libconv/$(MACH)
CPICLIB =	-L $(ROOT)/usr/lib/pics
CLIB =		-lc_pic

LDLIBS=		$(LDLIBS.lib) \
		$(CONVLIB) -lconv \
		$(CPICLIB) $(CLIB) \
		$(LDLIB) $(LD_LIB) \
		$(DBGLIB) $(LDDBG_LIB) \
		$(RTLDLIB) -lrtld

BUILD.s=	$(AS) $(ASFLAGS) $< -o $@
LD=		$(SGSPROTO)/ld

BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/rtld

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGCOM=	../common/rtld.msg
SGSMSG32=	../common/rtld.32.msg
SGSMSG64=	../common/rtld.64.msg
SGSMSGSPARC=	../common/rtld.sparc.msg
SGSMSGSPARC32=	../common/rtld.sparc32.msg
SGSMSGSPARC64=	../common/rtld.sparc64.msg
SGSMSGINTEL=	../common/rtld.intel.msg
SGSMSGINTEL32=	../common/rtld.intel32.msg
SGSMSGCHK=	../common/rtld.chk.msg
SGSMSGTARG=	$(SGSMSGCOM)
SGSMSGALL=	$(SGSMSGCOM) $(SGSMSG32) $(SGSMSG64) \
		$(SGSMSGSPARC) $(SGSMSGSPARC32) $(SGSMSGSPARC64) \
		$(SGSMSGINTEL) $(SGSMSGINTEL32)

SGSMSGFLAGS1=	$(SGSMSGFLAGS) -m $(BLTMESG)
SGSMSGFLAGS2=	$(SGSMSGFLAGS) -h $(BLTDEFS) -d $(BLTDATA) -n rtld_msg

SRCS=		$(COMOBJS:%.o=../common/%.c)  $(MACHOBJS:%.o=%.c) $(BLTDATA) \
		$(G_MACHOBJS:%.o=$(SRCBASE)/uts/$(PLAT)/krtld/%.c) \
		$(CP_MACHOBJS:%.o=../$(MACH)/%.c) \
		$(ASOBJS:%.o=%.s)
LINTSRCS=	$(SRCS) ../common/lintsup.c

LINTFLAGS =	-mu -Dsun -erroff=E_SUPPRESSION_DIRECTIVE_UNUSED \
		-erroff=E_EMPTY_TRANSLATION_UNIT -errtags=yes
LINTFLAGS64 =	-mu -errchk=longptr64 -errtags=yes \
		-erroff=E_EMPTY_TRANSLATION_UNIT \
		-erroff=E_CAST_INT_TO_SMALL_INT

CLEANFILES +=	$(LINTOUTS)  $(CRTS)  $(BLTFILES)  $(ADBSCRIPTS)
CLOBBERFILES +=	$(RTLD)

.PARALLEL:	$(ADBSCRIPTS)
