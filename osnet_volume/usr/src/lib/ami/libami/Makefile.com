#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile.com	1.2 99/07/11 SMI"
#
#	Common Makefile for libami
#

# include all ami objects
include $(SRC)/lib/ami/libami/ami.objs

# Flags for INTEL architecture
BSAFE_INTEL_FLAGS:sh=	MACH=`uname -p`; \
	if [ "${MACH}" = "i386" ]; \
		then echo -DBIG_ENDIAN=0; \
	fi
#
# Debug flag
#
debug :=	CPPFLAGS += $(DEBUG)
debug :=	CPPFLAGS64 += $(DEBUG)
debug :=	COPTFLAG = -g
debug :=	COPTFLAG64 = -g
debug :=	CCOPTFLAG = -g
debug :=	CCOPTFLAG64 = -g

#
# Messages text domain
#
TEXT_DOMAIN= SUNW_OST_SYSOSAMI

#
# CFLAGS for all objects
#
CPPFLAGS += -D_REENTRANT -D$(MACH) $(BSAFE_INTEL_FLAGS)

#
# Common include flags for libami objects
#
INCDIR=		$(SRC)/lib/ami/libami/include
INCFLAGS += -I$(INCDIR) -I$(INCDIR)/bsafe3 -I../amiserv \
	-I$(JAVA_ROOT)/include -I$(JAVA_ROOT)/include/solaris
CFLAGS += $(INCFLAGS)
CFLAGS64 += $(INCFLAGS)

# We have a requirement to build an international version
# of the libami.so.1 and libami.a where certain symbols are renamed
# to more obscure names.
# symbols renamed into obscurity follow:
INTLMAP += -D__ami_encrypt=kvdsaivqa
INTLMAP += -D__ami_decrypt=kdlsdkspd
INTLMAP += -D__ami_encrypt_private_key=aksksfdq
INTLMAP += -D__ami_decrypt_private_key=glhrahhi
INTLMAP += -D__ami_wrap_key=vfeqjfkkj

# CFLAGS for export objects (function mangling)
INTLPICS= $(INTLOBJS:%=pics/%)
$(INTLPICS) :=	CPPFLAGS += $(INTLMAP)

# include flags for bsafe objects
BSAFEPICS=	$(BSAFEOBJS:%=pics/%)
$(BSAFEPICS) := CFLAGS += -I../bsafe/algae/c -I../bsafe/bsource/include
$(BSAFEPICS) := CFLAGS64 += -I../bsafe/algae/c -I../bsafe/bsource/include

# include flags for lint
LINTSRC =	llib-lami
$(LINTLIB) :=	SRCS = ../../common/$(LINTSRC)
$(LINTLIB) :=	CPPFLAGS += $(INCFLAGS)
ROOTLINT= $(LINTSRC:%=$(ROOTLIBDIR)/%)
ROOTLINTLIBS= $(LINTLIB:%=$(ROOTLIBDIR)/%)
ROOTLINTLIBS64= $(LINTLIB:%=$(ROOTLIBDIR64)/%)
LINTEXT= .ln

#
# Mapfiles
#
MAPFILE=	$(MAPDIR)/mapfile

#
# Define global pics objects for subdirs
#
OBJECTS= $(GLOOBJS)
LGLOPICS = $(GLOOBJS:%=../pics/%)
LDLIBS += $(LGLOPICS) -lm -lxfn -lnsl -lmd5 -lthread -lc
ZTEXT=	
ZCOMBRELOC=	
SONAME = $(AMILINKS)$(VERS)
DYNFLAGS +=	-M $(MAPFILE)

#
# Build rules
#
# Sub dirs will define the target for all
all:	target_all

$(MAPFILE): $(MAPDIR)/../ami.spec
	@cd $(MAPDIR); $(MAKE) mapfile

objs/%.o profs/%.o pics/%.o: ../utils/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O) 

objs/%.o profs/%.o pics/%.o: ../../utils/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O) 

objs/%.o profs/%.o pics/%.o: ../keygen/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O) 

objs/%.o profs/%.o pics/%.o: ../sign/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../cert/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../encrypt/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../digest/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O) 

objs/%.o profs/%.o pics/%.o: ../../keygen/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O) 

objs/%.o profs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O) 

objs/%.o profs/%.o pics/%.o: ../../encrypt/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O) 

objs/%.o profs/%.o pics/%.o: ../cryptoki/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O) 

objs/%.o profs/%.o pics/%.o: ../keymgnt/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O) 

objs/%.o profs/%.o pics/%.o: ../amiserv/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O) 

objs/%.o profs/%.o pics/%.o: ../asn1/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O) 

objs/%.o profs/%.o pics/%.o: ../bsafe/bsource/algs/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../bsafe/bsource/keys/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../bsafe/bsource/support/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../bsafe/algae/c/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

# Set the LINK rules
ROOTAMILINKS=	$(ROOTLIBDIR)/$(AMILINKS)
ROOTAMILINKS64=	$(ROOTLIBDIR64)/$(AMILINKS)

LINTLIBLINKS=	$(ROOTLIBDIR)/$(LINTSRC)$(LINTEXT)
LINTLIBLINKS64=	$(ROOTLIBDIR64)/$(LINTSRC)$(LINTEXT)

$(ROOTLIBDIR):
	$(INS.dir)

$(ROOTLIBDIR64):
	$(INS.dir)

$(ROOTLIBDIR)/$(AMILINKS): $(ROOTLIBDIR)/$(AMILINKS)$(VERS)
	$(RM) $@
	$(SYMLINK) $(AMILINKS)$(VERS) $@

$(ROOTLIBDIR)/$(AMILINKS)$(VERS): $(ROOTLIBDIR)/$(DYNLIB)
	$(RM) $@
	$(SYMLINK) $(DYNLIB) $@

$(ROOTLIBDIR64)/$(AMILINKS): $(ROOTLIBDIR64)/$(AMILINKS)$(VERS)
	$(RM) $@
	$(SYMLINK) $(AMILINKS)$(VERS) $@

$(ROOTLIBDIR64)/$(AMILINKS)$(VERS): $(ROOTLIBDIR64)/$(DYNLIB)
	$(RM) $@
	$(SYMLINK) $(DYNLIB) $@

$(ROOTLIBDIR)/$(LINTSRC)$(LINTEXT): $(ROOTLIBDIR)/$(LINTLINKS)
	$(RM) $@
	$(RM) $(LINTSRC)$(LINTEXT)
	$(SYMLINK) $(LINTLINKS) $@

$(ROOTLIBDIR64)/$(LINTSRC)$(LINTEXT): $(ROOTLIBDIR64)/$(LINTLINKS)
	$(RM) $@
	$(RM) $(LINTSRC)$(LINTEXT)
	$(SYMLINK) $(LINTLINKS) $@

# install rule for lint library target
$(ROOTLIBDIR)/%: ../../common/%
	$(INS.file)
