#
#pragma ident	"@(#)Makefile	1.3	97/09/23 SMI"
#
LIBRARY= methods_l10n.a
VERS= .1

INCLIST = -I. -Iinc -I../../../inc -I../../../../../cmd/localedef/include

OBJECTS= \
__fgetwc_dense_pck.o	__mbtowc_dense_euckr.o    __wctomb_dense_euckr.o \
__mbtowc_dense_euctw.o    __wctomb_dense_euctw.o \
__mbftowc_dense_pck.o     __mbtowc_dense_pck.o      __wctomb_dense_pck.o \
__mblen_dense_eucjp.o     __wcstombs_dense_eucjp.o  __wcwidth_dense_eucjp.o \
__mblen_dense_euckr.o     __wcstombs_dense_euckr.o  __wcwidth_dense_euckr.o \
__mblen_dense_euctw.o     __wcstombs_dense_euctw.o  __wcwidth_dense_euctw.o \
__mblen_dense_pck.o       __wcstombs_dense_pck.o    __wcwidth_dense_pck.o \
__mbstowcs_dense_eucjp.o  __wcswidth_dense_eucjp.o \
__mbstowcs_dense_euckr.o  __wcswidth_dense_euckr.o \
__mbstowcs_dense_euctw.o  __wcswidth_dense_euctw.o \
__mbstowcs_dense_pck.o    __wcswidth_dense_pck.o \
__mbtowc_dense_eucjp.o    __wctomb_dense_eucjp.o

include ../../../../Makefile.lib

LIBS = $(DYNLIB)
CPPFLAGS=	$(INCLIST) $(CPPFLAGS.master) -D_REENTRANT

LDLIBS += -lc
	
.KEEP_STATE:

all: $(DYNLIB)

#install:	$(ROOTLINKS) $(DYNLIB)

install: all

pics/%.o : ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

include ../../../../Makefile.targ
