#ident	"@(#)Makefile	1.1	99/10/08 SMI"
#
# Generated automatically from Makefile.in by configure.
## -*- text -*- ####################################################
#								   #
# Makefile for the GNU Glob Library.				   #
#								   #
####################################################################

srcdir = .
VPATH = .:.
topdir = ../..
BUILD_DIR = ../..

include ../../../Makefile.cmd

SHELL = /bin/sh
LOCAL_CFLAGS = 
CFLAGS += -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
LDFLAGS =   

DEFS = -DHAVE_CONFIG_H
LOCAL_DEFS = -DSHELL

INCLUDES = -I. -I../.. -I$(topdir) -I$(topdir)/lib

CFLAGS += $(DEFS) $(LOCAL_DEFS) ${INCLUDES} $(LOCAL_CFLAGS)

# The name of the library target.
LIBRARY_NAME = libglob.a

# The C code source files for this library.
CSOURCES = $(srcdir)/glob.c $(srcdir)/fnmatch.c

# The header files for this library.
HSOURCES = $(srcdir)/fnmatch.h

OBJECTS = glob.o fnmatch.o

# The texinfo files which document this library.
DOCSOURCE = doc/glob.texi
DOCOBJECT = doc/glob.dvi
DOCSUPPORT = doc/Makefile
DOCUMENTATION = $(DOCSOURCE) $(DOCOBJECT) $(DOCSUPPORT)

SUPPORT = Makefile ChangeLog $(DOCSUPPORT)

SOURCES  = $(CSOURCES) $(HSOURCES) $(DOCSOURCE)

THINGS_TO_TAR = $(SOURCES) $(SUPPORT)

######################################################################

.KEEP_STATE:

all: $(LIBRARY_NAME)

$(LIBRARY_NAME): $(OBJECTS)
	$(RM) -f $@
	$(AR) $(ARFLAGS) $@ $(OBJECTS)

what-tar:
	@for file in $(THINGS_TO_TAR); do \
	  echo $(selfdir)$$file; \
	done

documentation: force
	-(cd doc; $(MAKE) $(MFLAGS))
force:

# The rule for 'includes' is written funny so that the if statement
# always returns TRUE unless there really was an error installing the
# include files.
install:

clean:
	rm -f $(OBJECTS) $(LIBRARY_NAME)
	-(cd doc && $(MAKE) $(MFLAGS) $@ )

realclean distclean maintainer-clean: clean
	-( cd doc && $(MAKE) $(MFLAGS) $@ )
	$(RM) -f Makefile

mostlyclean: clean
	-( cd doc && $(MAKE) $(MFLAGS) $@ )

######################################################################
#								     #
#  Dependencies for the object files which make up this library.     #
#								     #
######################################################################

fnmatch.o: fnmatch.h
fnmatch.o: $(BUILD_DIR)/config.h

glob.o: $(BUILD_DIR)/config.h
glob.o: $(topdir)/bashtypes.h $(topdir)/ansi_stdlib.h $(topdir)/bashansi.h
glob.o: $(topdir)/posixstat.h $(topdir)/memalloc.h
glob.o: fnmatch.h

# Rules for deficient makes, like SunOS and Solaris
fnmatch.o: fnmatch.c
glob.o: glob.c

include ../../../Makefile.targ
