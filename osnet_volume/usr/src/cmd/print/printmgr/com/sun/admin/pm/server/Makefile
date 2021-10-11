#
# ident	"@(#)Makefile	1.4	99/06/16 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# Makefile for Java Print Manager server
#

LIBRARY =	libpmgr.a
VERS =		.1

JNIHDR_FILES =	com_sun_admin_pm_server_DoPrinterNS.h

OBJECTS =	DoPrinterNS.o NS.o

CLASSFILES = 	Printer.class \
		Host.class \
		SysCommand.class \
		NameService.class \
		PrinterUtil.class \
		DoPrinterView.class \
		DoPrinterAdd.class \
		DoPrinterDelete.class \
		DoPrinterUtil.class \
		DoPrinterMod.class \
		DoPrinterNS.class \
		Valid.class \
		Test.class \
		Debug.class \
		PrinterDebug.class \
		pmException.class \
		pmAuthException.class \
		pmAuthRhostException.class \
		pmCmdFailedException.class \
		pmInternalErrorException.class \
		pmHostNotPingableException.class \
		pmNSNotConfiguredException.class

JNICLASSFILES =	DoPrinterNS.class

include		$(SRC)/lib/Makefile.lib

CLASSPATH=	$(SRC)/cmd/print/printmgr

JAVAFILES =	$(CLASSFILES:.class=.java)

ROOTDIRS =	$(ROOT)/usr/sadm/admin \
		$(ROOT)/usr/sadm/admin/printmgr \
		$(ROOT)/usr/sadm/admin/printmgr/lib

# override ROOTLIBDIR and ROOTLINKS
ROOTLIBDIR =	$(ROOT)/usr/sadm/admin/printmgr/lib
ROOTLIBS=       $(LIBS:%=$(ROOTLIBDIR)/%)

$(ROOTLINKS):=	OWNER = root
$(ROOTLINKS):=	GROUP = lp

# Following variables define where to find header files
CPPFLAGS +=	-I$(JAVA_ROOT)/include -I$(JAVA_ROOT)/include/solaris -I.

LDLIBS =	-lprint -lxfn -lnsl -lsocket -lgen -lm -lmp -lc

CLEANFILES=	*.class $(LINTLIB) $(LINTOUT)
CLOBBERFILES=	$(JNIHDR_FILES) $(LIBLINKS)

all: $(CLASSFILES) $(JNIHDR_FILES) $(DYNLIB)

install: all $(ROOTDIRS) $(ROOTLINKS)

#
# Build jni header file
# Use $@ instead of the "unreliable" $*
#
$(JNIHDR_FILES): $(JNICLASSFILES)
	$(JAVAH) -jni -classpath $(CLASSPATH) \
		`echo $@ | sed 's/.h$$//' | tr _ .`

#
# Build standalone programs for testing
#
Fns: Fns.c
	$(CC) -DMAIN Fns.c -o Fns -lprint -lxfn -lnsl

NS: NS.c
	$(CC) NS.c -o NS -lprint -lxfn -lnsl -lsocket

lint: $(JNIHDR_FILES) $(LINTLIB)

cstyle:
	cstyle $(SRCS)

jstyle:
	jstyle $(JAVAFILES)

_msg:

$(ROOTDIRS):
	$(INS.dir)

include $(SRC)/lib/Makefile.targ
