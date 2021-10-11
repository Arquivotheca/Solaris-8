#
#
#ident @(#)Makefile	1.3	99/04/09
#
# Copyright (c) 1989,1998 by Sun Microsystems, Inc.
#

include ../../Makefile.cmd


TXTS=	devconfig.info cui.cui


ROOTUSR=			$(ROOT)/usr
ROOTUSRLIB=			$(ROOTUSR)/lib
ROOTUSRLIB_DEVCONFIG=		$(ROOTUSRLIB)/devconfig
DEVCONFIG_LIB=			$(ROOTUSRLIB_DEVCONFIG)/lib
DEVCONFIG_LIB_LOCALE=		$(DEVCONFIG_LIB)/locale
DEVCONFIG_LIB_LOCALE_C=		$(DEVCONFIG_LIB_LOCALE)/C
DEVCONFIG_LIB_LOCALE_C_HELP=	$(DEVCONFIG_LIB_LOCALE_C)/help

DEVCONFIG_INFO=			$(DEVCONFIG_LIB_LOCALE_C_HELP)/devconfig.info
CUI_CUI=			$(DEVCONFIG_LIB)/cui.cui


DIRS=						\
	$(ROOTUSR)				\
	$(ROOTUSRLIB)				\
	$(ROOTUSRLIB_DEVCONFIG)			\
	$(DEVCONFIG_LIB)			\
	$(DEVCONFIG_LIB_LOCALE)			\
	$(DEVCONFIG_LIB_LOCALE_C)		\
	$(DEVCONFIG_LIB_LOCALE_C_HELP)


OWNER=root
GROUP=bin
FILEMODE=0444
DIRMODE=0755


.KEEP_STATE:

all: $(TXTS)

install: all $(DIRS) $(DEVCONFIG_INFO) $(CUI_CUI)

$(DIRS):
	$(INS.dir)

$(DEVCONFIG_LIB_LOCALE_C_HELP)/% $(DEVCONFIG_LIB)/%: %
	$(INS.file)


clean:

lint:

include ../../Makefile.targ
