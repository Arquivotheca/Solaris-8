#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.spec	1.5	99/10/27 SMI"
#
# lib/Makefile.spec

.KEEP_STATE:

#
# This file depends on the macro $(OBJECTS) containing a .o file for every
# spec file and $(SRC)/usr/lib/Makefile.lib(.64) being included.
#

ABIBASE=	$(SRC)/cmd/abi
SPECTRANS=	$(ABIBASE)/spectrans
#
# If the shell variable SPECWS is defined the spectrans tools from the current
# workspace will be used
#
SPECWS:sh=	echo \\043

#
# Support object for the link editor to cull the stabs sections.
#
sparc_LDCULLSTABS=		-Wl,-S,/usr/lib/abi/ldcullstabs.so.1
i386_LDCULLSTABS=		-Wl,-S,/usr/lib/abi/ldcullstabs.so.1
sparcv9_LDCULLSTABS=		-Wl,-S,/usr/lib/abi/$(MACH64)/ldcullstabs.so.1
ia64_LDCULLSTABS=		-Wl,-S,/usr/lib/abi/$(MACH64)/ldcullstabs.so.1
ABICULLING=			$($(TRANSMACH)_LDCULLSTABS) -xildoff

# Map OBJECTS to .spec files
SPECS=		$(OBJECTS:%.o=../%.spec)

# Name of shared object to actually build
ABILNROOT=	$(LIBRARY:%.a=%)
ABILIBNAME=	$(ABILNROOT:%=abi_%)
ABILIB=		$(ABILIBNAME).so$(VERS)

# change some stuff for DYNFLAGS to be correct
ZDEFS=
SONAME=		 $(ABILIB)

# Where to find spec files that this spec may depend on
TRANSCPP +=	-I$(SRC)/lib
# Fall back to parent workspace if spec file is not in this Workspace
TRANSCPP +=	$(ENVCPPFLAGS2:%/proto/root_$(MACH)/usr/include=%/usr/src/lib)
TRANSFLAGS=	-a $(TRANSMACH) -l $(ABILNROOT) $(TRANSCPP)

PFOBJ=		$(ABILNROOT:%=pics/%_pf$(SPECVERS).o)
PFSRC=		$(PFOBJ:pics/%.o=%.c)
PFFILES=	$(OBJECTS:%.o=%.pf)
PFVERSFILES=	$(OBJECTS:%.o=%-vers)
PFVERS=		$(PFVERSFILES:%=-M %)
LINK.pf=	sort -u -o $@ $(PFFILES)

SPECMAP=	mapfile$(SPECVERS)
VERSFILE=	../versions$(SPECVERS)

CLEANFILES +=	$(SRCS) $(PFFILES) $(PFSRC) $(PFOBJ) $(PFVERSFILES)
CLOBBERFILES +=	$(ABILIB) $(SPECMAP)

$(ABILIB) :=	CPPFLAGS += $(SPECCPP)
$(SPECMAP) :=	TRANSFLAGS += -v $(VERSFILE)
$(PICS) := COPTFLAG= -g -O -xs
$(PICS) := COPTFLAG64= -g -O -xs
$(PFOBJ)  :=	sparc_CFLAGS += -xregs=no%appl $(sparc_C_PICFLAGS)
$(PFOBJ)  :=	i386_CFLAGS += $(i386_C_PICFLAGS)
$(PFOBJ)  :=	sparcv9_CFLAGS += -xregs=no%appl $(sparcv9_C_PICFLAGS)

SPEC2TRACE=	/usr/lib/abi/spec2trace
SPEC2MAP=	/usr/lib/abi/spec2map

$(SPECWS)SPEC2TRACE=	$(SPECTRANS)/spec2trace/$(MACH)/spec2trace
$(SPECWS)SPEC2MAP=	$(SPECTRANS)/spec2map/$(MACH)/spec2map

all:		$(SPECMAP) $(ABILIB)

# .pf files have a one-to-one mapping with spec files
%.pf:		../%.spec
	$(SPEC2TRACE) $(TRANSFLAGS) -o $@ $<

%-vers:		%.pf

$(PFSRC):	$(PFFILES)
	$(LINK.pf)

$(ABILIB):	$(PFSRC) $(SRCS) pics .WAIT	\
		$(PICS) $(PFOBJ)
	$(BUILD.SO) $(PFVERS) $(ABICULLING) $(PFOBJ)
	$(MCS) -d -n .symtab $@
	$(POST_PROCESS_SO)

$(SPECMAP):	$(VERSFILE) $(SPECS)
	$(SPEC2MAP) $(TRANSFLAGS) -o $@ $(SPECS)

include         $(SRC)/lib/Makefile.targ

#
# usr/lib/abi targets
#

FILEMODE=	755
ROOTABIDIR=	$(ROOTLIBDIR)/abi
ROOTABIDIR64=	$(ROOTABIDIR)/$(MACH64)
ROOTABILIB=	$(ROOTABIDIR)/$(ABILIB)
ROOTABILIB64=	$(ROOTABIDIR64)/$(ABILIB)

INS.abilib=	$(INS.file) $(ABILIB)

$(ROOTABILIB) $(ROOTABILIB64):	$(SPECMAP) $(ABILIB)
	$(INS.abilib)

#
# usr/xpg4/lib/abi targets
#

XPG4_ABILIB=	$(ROOT)/usr/xpg4/lib/abi/$(ABILIB)
XPG4_ABILIB64=	$(ROOT)/usr/xpg4/lib/abi/$(MACH64)/$(ABILIB)

$(XPG4_ABILIB) $(XPG4_ABILIB64): $(SPECMAP) $(ABILIB)
	$(INS.abilib)

#
# usr/ucblib/abi targets
#

UCBLIB_ABILIB=		$(ROOT)/usr/ucblib/abi/$(ABILIB)
UCBLIB_ABILIB64=	$(ROOT)/usr/ucblib/abi/$(MACH64)/$(ABILIB)

$(UCBLIB_ABILIB) $(UCBLIB_ABILIB64): $(SPECMAP) $(ABILIB)
	$(INS.abilib)

#
# usr/lib/lwp/abi targets
#

LWPLIB_ABILIB=		$(ROOT)/usr/lib/lwp/abi/$(ABILIB)
LWPLIB_ABILIB64=	$(ROOT)/usr/lib/lwp/abi/$(MACH64)/$(ABILIB)

$(LWPLIB_ABILIB):
	$(RM) $@
	$(SYMLINK) ../../abi/$(ABILIB) $@

$(LWPLIB_ABILIB64):
	$(RM) $@
	$(SYMLINK) ../../../abi/sparcv9/$(ABILIB) $@

FRC:
