#
#pragma ident	"@(#)Makefile	1.56	99/09/14 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/packages/Makefile

include		$(SRC)/Makefile.master

BUILDLOG=	package_build.$(MACH).log

MACHS:sh=	if [ "$MACH" = "sparc" ]; \
		then echo "sparc sparcv9"; \
		elif [ "$MACH" = "i386" ]; \
		then echo "i386"; \
		else echo "unknown"; \
		fi

RDBDIRS=	$(MACHS:%=../librtld_db/rdb_demo/%)
LDMAKEDIRS=	$(MACHS:%=../libldmake/%)


SUBDIRS1= 	../libdl \
		../libelf \
		../libconv \
		.WAIT \
		../liblddbg \
		../librtld_db \
		.WAIT \
		../librtld \
		../libcrle

SUBDIRS2 =	../rtld \
		../rtld.4.x \
		../libldstab \
		../ldprof \
		../lddstub \
		../link_audit \
		$(LDMAKEDIRS) \
		../libldfeature \
		../0@0 \
		../gprof \
		../lorder \
		../tools \
		$(RDBDIRS)

INTLDIRS=	../libld \
		../dump \
		../elfdump \
		../ar \
		../crle \
		../ld \
		../ldd \
		../mcs \
		../nm \
		../pvs \
		../size

$(SUBDIRS1) $(SUBDIRS2) :=	TARGET = install
$(INTLDIRS) :=	TARGET = package


M32:sh=		if [ "$MACH" = "sparc" ]; \
		then echo sparc; \
		elif [ "$MACH" = "i386" ]; \
		then echo "intel"; \
		else echo "unknown"; \
		fi

../%:		FRC
		@ cd $@; pwd; $(MAKE) $(TARGET)

.KEEP_STATE_FILE: .make.state.$(MACH)
.KEEP_STATE:
.PARALLEL:	$(SUBDIRS1) $(INTLDIRS) $(SUBDIRS2)

all clean clobber install: FRC
		@ cd $(MACH); pwd; $(MAKE) $@

# 
# invoke `package' to build this package from scratch.  It will 
# perform make install in the directories that contains components 
# that this package is dependent upon 
# 
package:	FRC
	@/bin/time $(MAKE) package_build 2>&1 | tee $(BUILDLOG)
	@ echo "===BUILD COMPLETED  checking for warnings...==="
	@-grep -i "warning:" $(BUILDLOG) | grep -v PSTAMP
	@ echo "==================done========================="

package_build:	package_dep .WAIT package_chkmsg .WAIT install

package_chkmsg:	FRC
		@ cd ..; pwd; $(MAKE) chkmsg

package_dep:	package_dep_ .WAIT $(SUBDIRS1) .WAIT \
		$(INTLDIRS) .WAIT $(SUBDIRS2)

package_dep_:	FRC
		@ cd ../tools; pwd; $(MAKE) proto && ./proto
		@ cd $(SRC)/head; pwd; $(MAKE) \
			$(ROOT)/usr/include/dlfcn.h \
			$(ROOT)/usr/include/gelf.h \
			$(ROOT)/usr/include/libelf.h \
			$(ROOT)/usr/include/proc_service.h \
			$(ROOT)/usr/include/rtld_db.h \
			$(ROOT)/usr/include/link.h \
			$(ROOT)/usr/include/apptrace.h \
			$(ROOT)/usr/include/apptrace_impl.h
		@ cd $(SRC)/uts/common/sys; pwd; $(MAKE) \
			$(ROOT)/usr/include/sys/elf.h \
			$(ROOT)/usr/include/sys/elf_386.h \
			$(ROOT)/usr/include/sys/elf_ia64.h \
			$(ROOT)/usr/include/sys/elf_M32.h \
			$(ROOT)/usr/include/sys/elf_SPARC.h \
			$(ROOT)/usr/include/sys/elf_notes.h \
			$(ROOT)/usr/include/sys/elftypes.h \
			$(ROOT)/usr/include/sys/auxv.h \
			$(ROOT)/usr/include/sys/link.h \
			$(ROOT)/usr/include/sys/note.h \
			$(ROOT)/usr/include/sys/systeminfo.h
		@ cd $(SRC)/uts/$(M32)/sys; pwd; $(MAKE) \
			$(ROOT)/usr/include/sys/machelf.h
		@ cd ../tools/$(MACH); pwd; $(MAKE) sgsmsg jive
		@ cd ..; pwd; $(MAKE) _msg
		@ cd ..; pwd; $(MAKE) native-add
		@ cd ../link_audit; pwd; $(MAKE) install_onld
		@ cd ../messages/$(MACH); pwd; $(MAKE) test
		@ cd ../sgsdemangler; pwd; $(MAKE) all

lint: FRC
	$(MAKE) -f Makefile.lint lint

FRC:
