#
#ident	"@(#)Makefile.com	1.4	97/11/20 SMI"
#
# Copyright (c) 1993 - 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/fn/libfn_spf/Makefile.com
#
LIBRARYCCC= libfn_spf.a
VERS = .1

FNLINKS= fn_compound_name_standard.so$(VERS)
FNLINKS64= fn_compound_name_standard.so$(VERS)

TRLOBJS= \
	FN_ctx_svc.o \
	FN_ctx_csvc.o \
	FN_ctx_asvc.o \
	FN_ctx_csvc_strong.o \
	FN_ctx_asvc_strong.o \
	FN_ctx_cnsvc.o \
	FN_ctx_cnsvc_impl.o \
	FN_ctx_cnsvc_weak_static.o \
	FN_ctx_cnsvc_weak_dynamic.o \
	FN_ctx_csvc_weak_static.o \
	FN_ctx_csvc_weak_dynamic.o \
	FN_ctx_asvc_weak.o \
	FN_ctx_asvc_weak_dynamic.o \
	FN_ctx_asvc_weak_static.o \
	FN_namelist_svc.o FN_bindinglist_svc.o \
	FN_multigetlist_svc.o FN_valuelist_svc.o \
	FN_searchlist_svc.o FN_ext_searchlist_svc.o \
	FN_status_svc.o \
	FN_compound_name_standard.o FN_syntax_standard.o \
	fn_links.o FN_ctx_func_info.o

OBJECTS= $(TRLOBJS)

# include library definitions
include ../../Makefile.libfn

ROOTLIBDIR=	$(ROOT)/usr/lib
ROOTLIBDIR64=	$(ROOT)/usr/lib/$(MACH64)
FNLINKTARG=	../$(DYNLIBCCC)
FNLINKTARG64=	../../$(MACH64)/$(DYNLIBCCC)

LIBS = $(DYNLIBCCC)

# xfn for client lib
LDLIBS += -lxfn -lc 

.KEEP_STATE:

all: $(LIBS)

analyse:
	$(CODEMGR_WS)/test.fns/bin/analyse

# include library targets
include ../../Makefile.targ

objs/%.o profs/%.o pics/%.o: ../common/%.cc
	$(COMPILE.cc) -o $@ $<
	$(POST_PROCESS_O)


