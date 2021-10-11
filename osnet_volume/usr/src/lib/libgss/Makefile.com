#
# Copyright (c) 1992-1995,1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.6	00/09/14 SMI"
#
# lib/libgss/Makefile

LIBRARY= libgss.a
VERS = .1

OBJECTS	= g_acquire_cred.o \
	  g_rel_cred.o \
	  g_init_sec_context.o \
	  g_accept_sec_context.o \
	  g_process_context.o \
	  g_delete_sec_context.o \
	  g_imp_sec_context.o \
	  g_exp_sec_context.o \
	  g_context_time.o \
	  g_sign.o \
	  g_verify.o \
	  g_seal.o \
	  g_unseal.o \
	  g_dsp_status.o \
	  g_compare_name.o \
	  g_dsp_name.o \
	  g_imp_name.o \
	  g_rel_name.o \
	  g_rel_buffer.o \
	  g_rel_oid_set.o \
	  g_oid_ops.o \
	  g_inquire_cred.o \
	  g_inquire_context.o \
	  g_inquire_names.o \
	  g_initialize.o \
	  g_glue.o \
	  gssd_pname_to_uid.o \
	  gen_oids.o \
	  oid_ops.o \
	  g_canon_name.o \
	  g_dup_name.o \
	  g_export_name.o \
	  g_utils.o \
	  gsscred_xfn.o \
	  gsscred_utils.o \
	  gsscred_file.o


# defines the duplicate sources we share with gsscred
GSSCRED_DIR = ../../../cmd/gss/gsscred

# include library definitions
include ../../Makefile.lib
SRCS=	$(OBJECTS:%.o=../%.c)
objs/%.o pics/%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

#
# This target is for lint
#

LINTFLAGS=      -u -I..
LINTFLAGS64=    -u -I..
LINTOUT=        lint.out

LINTSRC=        $(LINTLIB:%.ln=%)
ROOTLINTDIR=    $(ROOTLIBDIR)
# ROOTLINT=       $(LINTSRC:%=$(ROOTLINTDIR)/%)

#override INS.liblink
INS.liblink=	-$(RM) $@; $(SYMLINK) $(LIBLINKPATH)$(LIBLINKS)$(VERS) $@

GSSCRED_DIR = ../../../cmd/gss/gsscred
#add the gsscred directory as part of the include path
CPPFLAGS += -I$(GSSCRED_DIR)


CPPFLAGS += -I$(SRC)/uts/common/gssapi/include -DHAVE_STDLIB_H

$(ROOTDIRS)/%: %
LIBS = $(DYNLIB) # $(LINTLIB)
LDLIBS += -ldl -lc -lxfn

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES += $(MAPFILE)

DYNFLAGS +=	-M $(MAPFILE)

.KEEP_STATE:

lint: $(LINTLIB)


# include library targets
include ../../Makefile.targ

#additional dependencies

$(LIBRARY) : $(OBJS)
$(DYNLIB) : $(MAPFILE) $(PICS) 

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile
