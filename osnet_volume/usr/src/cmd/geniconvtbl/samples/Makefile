# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.6	99/05/29 SMI"
#
# cmd/geniconvtbl/samples/Makefile


INPUTFILES = ISO8859-1_to_UTF-8.src UTF-8_to_ISO8859-1.src\
		eucJP_to_ISO-2022-JP.src ISO-2022-JP_to_eucJP.src \
		ISO646_to_ISO8859-1.src ISO8859-1_to_ISO646.src
		
OUTPUTFILES	= ISO646%ISO8859-1.bt ISO8859-1%ISO646.bt
# ONLY TWO binarytables are deribalables

#		ISO8859-1%UTF-8.bt UTF-8%ISO8859-1.bt
#		eucJP%ISO-2022-JP.bt ISO-2022-JP%eucJP.bt \
#		ISO646%ISO8859-1.bt ISO8859-1%ISO646.bt 
# OUTPUTFILES	= $(INPUTFILES:%.src=%.bt)   NEVER USE ( WATCH OUT FILE NAME )


GENICONVTBL= ../$(MACH)/geniconvtbl


# include ../../../lib/Makefile.lib
include ../../Makefile.cmd


ROOTICONVDIRS	=	$(ROOTLIB)/iconv
ROOTDIRS	=	$(ROOTICONVDIRS)/geniconvtbl
SRCSDIRS	=	$(ROOTDIRS)/srcs
BTSDIRS		=	$(ROOTDIRS)/binarytables
ROOTINPUTFILES	=	$(INPUTFILES:%=$(SRCSDIRS)/%)
ROOTOUTPUTFILES =	$(OUTPUTFILES:%=$(BTSDIRS)/%)

$(ROOTINPUTFILES) :=	FILEMODE = 444
$(ROOTOUTPUTFILES) :=	FILEMODE = 444


.KEEP_STATE:

.PARALLEL: $(OUTPUTFILES)


all:	$(OUTPUTFILES)

install: all $(ROOTOUTPUTFILES) $(ROOTINPUTFILES)

clean clobber:
	$(RM) $(OUTPUTFILES)


ISO8859-1%UTF-8.bt: ISO8859-1_to_UTF-8.src
	$(GENICONVTBL) -o $@ -f ISO8859-1_to_UTF-8.src

UTF-8%ISO8859_1.bt: UTF-8_to_ISO8859_1.src
	$(GENICONVTBL) -o $@ -f UTF-8_to_ISO8859_1.src

eucJP%ISO-2022-JP.bt: eucJP_to_ISO-2022-JP.src
	$(GENICONVTBL) -o $@ -f eucJP_to_ISO-2022-JP.src

ISO-2022-JP%eucJP.bt: ISO-2022-JP_to_eucJP.src 
	$(GENICONVTBL) -o $@ -f ISO-2022-JP_to_eucJP.src 

ISO646%ISO8859-1.bt: ISO646_to_ISO8859-1.src
	$(GENICONVTBL) -o $@ -f ISO646_to_ISO8859-1.src

ISO8859-1%ISO646.bt: ISO8859-1_to_ISO646.src
	$(GENICONVTBL) -o $@ -f ISO8859-1_to_ISO646.src


# install rule
# 
$(SRCSDIRS)/%: % $(SRCSDIRS)
	$(INS.file)

$(BTSDIRS)/%: % $(BTSDIRS)
	$(INS.file)

$(SRCSDIRS) $(BTSDIRS): $(ROOTDIRS) 
	$(INS.dir)

$(ROOTDIRS): $(ROOTICONVDIRS)
	$(INS.dir)

$(ROOTICONVDIRS):
	$(INS.dir)

$(SRCSDIRS)/%: $(SRCSDIRS) %
	$(INS.file)

$(BTSDIRS)/%: $(BTSDIRS) %
	$(INS.file)

# rule of making BinaryTable
# ( must be placed after install rule )
#
# .SUFFIXES: $(SUFFIXES) .src
# .SUFFIXES: $(SUFFIXES) .bt

#
# %.bt:	%.src
#	$(GENICONVTBL) -o $@ -f $< 

#   include ../../Makefile.targ
