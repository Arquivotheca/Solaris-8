#
#pragma ident	"@(#)Makefile.com	1.5	99/03/21 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# lib/nsswitch/dns/Makefile
#
LIBRARY= libnss_dns.a
VERS= .1

OBJECTS= gethostent.o gethostent6.o dns_mt.o dns_common.o

# include library definitions, do not change order of include and DYNLIB1
include ../../../Makefile.lib

MAPFILE=	../common/mapfile-vers

DYNLIB1=	nss_dns.so$(VERS)

LINTFLAGS=
# Appropriate libresolv loaded at runtime. This is the default, to be dlopened
# if no libresolv was provided by the application.
NSS_DNS_LIBRESOLV = libresolv.so.2
LDLIBS += -lnsl -ldl -lc
CPPFLAGS += -D_REENTRANT -DSYSV -DNSS_DNS_LIBRESOLV=\"$(NSS_DNS_LIBRESOLV)\"
DYNFLAGS += -M $(MAPFILE)

LIBS += $(DYNLIB1)

.KEEP_STATE:

all: $(LIBS)

# if we call this "DYNLIB", it will automagically be expanded to
# libnss_dns.so*, and we don't have a target for that.
$(DYNLIB1):	$(MAPFILE)

$(ROOTLIBDIR)/$(DYNLIB1) :=      FILEMODE= 755    
$(ROOTLIBDIR64)/$(DYNLIB1) :=    FILEMODE= 755

$(DYNLIB1):	pics .WAIT $$(PICS)
	$(BUILD.SO)
	$(POST_PROCESS_SO)

# include library targets
include ../../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
