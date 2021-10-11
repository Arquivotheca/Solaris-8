#
#ident	"@(#)Makefile	1.2	99/10/17 SMI"

PROG=	zip zipnote zipsplit zipcloak

OBJECTS= zip.o zipfile.o zipup.o fileio.o util.o globals.o crypt.o ttyio.o \
	 unix.o crc32.o crctab.o deflate.o trees.o bits.o

UTILOBJECTS=	zipfile_.o fileio_.o util_.o unix_.o crypt_.o
ZIPNOTEOBJS=	zipnote.o zipfile_.o fileio_.o util_.o globals.o unix_.o 
ZIPSPLITOBJS=	zipsplit.o zipfile_.o fileio_.o util_.o globals.o unix_.o
ZIPCLOAKOBJS=	zipcloak.o zipfile_.o fileio_.o util_.o globals.o unix_.o \
		crctab.o crypt_.o ttyio.o 

MAN1FILES=	zip.1 zipcloak.1 zipnote.1 zipsplit.1


include ../Makefile.cmd

CFLAGS += -I. -DUNIX -DHAVE_DIRENT_H -DHAVE_TERMIOS_H \
    -errtags=yes -erroff=E_NON_CONST_INIT
$(UTILOBJECTS) := CFLAGS += -DUTIL

CLOBBERFILES += $(PROG) zipfile_.c fileio_.c util_.c unix_.c crypt_.c
ROOTSRC= $(ROOT)/usr/share/src/zip
DISTFILES:sh= cat ./distfiles
ROOTDISTFILES= $(DISTFILES:%=$(ROOTSRC)/%)
DISTDIRS= acorn amiga aosvs atari beos cmsmvs human68k man msdos os2 proginfo \
	  qdos tandem tops20 unix unix/Packaging vms win32 windll \
	  windll/visualc windll/visualc/dll windll/visualc/lib windll/borland \
	  windll/borland/dll windll/borland/lib
ROOTSRCDIRS=$(DISTDIRS:%=$(ROOTSRC)/%)
$(ROOTDISTFILES) := FILEMODE= 0444

.KEEP_STATE:

.PARALLEL: $(OBJECTS) $(UTILOBJECTS) $(ZIPNOTEOBJS) $(ZIPSPLITOBJS) \
		$(ZIPCLOAKOBJS)
.NO_PARALLEL: $(ROOTSRCDIRS)

all: $(PROG)

zip: $(OBJECTS)
	$(LINK.c) -o zip $(OBJECTS)
	$(POST_PROCESS)

zipnote: $(ZIPNOTEOBJS)
	$(LINK.c) -o zipnote $(ZIPNOTEOBJS)
	$(POST_PROCESS)

zipsplit: $(ZIPSPLITOBJS)
	$(LINK.c) -o zipsplit $(ZIPSPLITOBJS)
	$(POST_PROCESS)

zipcloak: $(ZIPCLOAKOBJS)
	$(LINK.c) -o zipcloak $(ZIPCLOAKOBJS)
	$(POST_PROCESS)

install: $(ROOTPROG) install_src install_man

install_src: $(ROOTSRC) .WAIT $(ROOTSRCDIRS) .WAIT $(ROOTDISTFILES)

install_man: $(ROOTMAN1FILES)

install_h: $(ROOTHDRS)

$(ROOTSRC) $(ROOTSRCDIRS):
	$(INS.dir)

$(ROOTSRC)/%: %
	$(INS.file)

unix.o: unix/unix.c
	$(COMPILE.c) unix/unix.c
	$(POST_PROCESS_O)

unix_.c: unix/unix.c
	$(RM) unix_.c
	$(SYMLINK) unix/unix.c unix_.c

%_.o: %_.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	$(POST_PROCESS_O)

%_.c: %.c
	$(RM) $@
	$(SYMLINK) $< $@

clean:
	$(RM) $(OBJECTS) $(UTILOBJECTS) $(ZIPNOTEOBJS) $(ZIPSPLITOBJS) \
		$(ZIPCLOAKOBJS)

include ../Makefile.targ

