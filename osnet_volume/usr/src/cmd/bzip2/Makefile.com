#
#ident	"@(#)Makefile.com	1.1	99/10/08 SMI"

PROG1=	bzip2
PROG2=	bzip2recover
PROG1LINK1= bunzip2
PROG1LINK2= bzcat
PROG=	$(PROG1) $(PROG2)
LIBRARY=	libbz2.a
VERS=.1

OBJECTS= blocksort.o  \
      huffman.o    \
      crctable.o   \
      randtable.o  \
      compress.o   \
      decompress.o \
      bzlib.o

include ../../Makefile.cmd
include ../../../lib/Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
LIBS=	$(DYNLIB) $(LINTLIB)
LDLIBS += -L. -lc
CLOBBERFILES += $(PROG1) $(PROG2) $(PROG1).o $(PROG2).o $(DYNLIB) $(LINTLIB) \
    $(LINTOUT) $(PROG1LINK1) $(PROG1LINK2) $(MAPFILE)
DYNFLAGS += -M $(MAPFILE)

$(ROOTPROG) := FILEMODE= 0555
$(ROOTLIBDIR)/$(DYNLIB) \
$(ROOTLIBDIR64)/$(DYNLIB) :=      FILEMODE= 755

CFLAGS += -I.. \
	-errtags=yes -erroff=E_NON_CONST_INIT -erroff=E_STATEMENT_NOT_REACHED
CFLAGS64 += -I.. \
	-errtags=yes -erroff=E_NON_CONST_INIT -erroff=E_STATEMENT_NOT_REACHED

# definitions for lint

LINTFLAGS=      -u -I..
LINTFLAGS64=    -u -I..
LINTOUT=        lint.out

LINTSRC=        $(LINTLIB:%.ln=%)
ROOTLINTDIR=    $(ROOTLIBDIR)
ROOTLINT=       $(LINTSRC:%=$(ROOTLINTDIR)/%)
$(LINTLIB):= SRCS=../llib-lbz2
$(LINTLIB):= LINTFLAGS=-nvx -I..

CLEANFILES +=   $(LINTOUT) $(LINTLIB)

.KEEP_STATE:

.PARALLEL: $(PICS)

$(PROG1LINK1):	$(PROG1)
	@$(RM) $(PROG1LINK1); $(LN) $(PROG1) $(PROG1LINK1)

$(PROG1LINK2):	$(PROG1)
	@$(RM) $(PROG1LINK2); $(LN) $(PROG1) $(PROG1LINK2)

$(ROOTBIN)/$(PROG1LINK1) $(ROOTBIN)/$(PROG1LINK2): $(ROOTBIN)/$(PROG1)
	$(RM) $@
	$(LN) $(ROOTBIN)/$(PROG1) $@

$(PROG1): $(DYNLIB) bzip2.o
	$(LINK.c) -o $(PROG1) bzip2.o $(DYNLIB) $(LDLIBS)

$(PROG2): $(PROG2).o
	$(LINK.c) -o $(PROG2) $(PROG2).o $(LDLIBS)

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

test: bzip2
	@cat ../words1
	env LD_LIBRARY_PATH=. ./bzip2 -1 < ../sample1.ref > sample1.rb2
	env LD_LIBRARY_PATH=. ./bzip2 -2 < ../sample2.ref > sample2.rb2
	env LD_LIBRARY_PATH=. ./bzip2 -d < ../sample1.bz2 > sample1.tst
	env LD_LIBRARY_PATH=. ./bzip2 -d < ../sample2.bz2 > sample2.tst
	@cat ../words2
	cmp ../sample1.bz2 sample1.rb2 
	cmp ../sample2.bz2 sample2.rb2
	cmp sample1.tst ../sample1.ref
	cmp sample2.tst ../sample2.ref
	@cat ../words3


tarfile:
	tar cvf interim.tar *.c *.h Makefile manual.texi manual.ps LICENSE bzip2.1 bzip2.1.preformatted bzip2.txt words1 words2 words3 sample1.ref sample2.ref sample1.bz2 sample2.bz2 *.html README CHANGES libbz2.def libbz2.dsp dlltest.dsp 

clean:
	$(RM) $(PICS)
	$(RM) sample1.rb2 sample2.rb2 sample1.tst sample2.tst


include ../../Makefile.targ

objs profs pic_profs pics libp:
	-@mkdir -p $@

$(DYNLIB): pics .WAIT $$(PICS)
	$(BUILD.SO)
	$(POST_PROCESS_SO)

pics/%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

%.o: ../%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../%
	$(INS.file)

$(LINTLIB): $$(SRCS)
	$(LINT.c) -o $(LIBNAME) $(SRCS) > $(LINTOUT) 2>&1

$(ROOTLIBDIR)/$(LIBLINKS): $(ROOTLIBDIR)/$(LIBLINKS)$(VERS)
	$(INS.liblink)

$(ROOTLIBDIR64)/%: %
	$(INS.file)

$(ROOTLIBDIR64)/$(LIBLINKS): $(ROOTLIBDIR64)/$(LIBLINKS)$(VERS)
	$(INS.liblink64)
