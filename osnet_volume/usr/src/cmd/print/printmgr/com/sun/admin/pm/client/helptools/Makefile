#
# ident	"@(#)Makefile	1.1	99/03/17 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# Makefile for Java Print Manager help tools
#
include         $(SRC)/Makefile.master

CLASSPATH=	$(JAVA_ROOT)/jre/lib/rt.jar

CLASSFILES = 	parseMain.class

CLEANFILES=	*.class
CLOBBERFILES=

install all: $(CLASSFILES)
	@$(CHMOD) +x ./parsehelp

clobber: clean
clean:
	$(RM) $(CLEANFILES)
