#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident "@(#)makefile	1.11	99/10/25 SMI\n"
#
#
#  Solaris x86 Realmode Driver Libraries:
#
#  We build three libraries here:
#
#     $(BLIB)  ...  A library containing driver-specific routines
#     $(CLIB)  ...  A truncated C library for use by drivers
#     $(OLIB)  ...  A "oldnames" library to shut the linker up!
#
#  The oldnames library isn't really used, but if it's not there the linker
#  complains.  It contains a single module: crt0.obj.  This is the C startup
#  module for realmode drivers and we have to build it anyway, so it doesn't
#  really hurt to stick it in a library.

DRIVERS=..\..
!include $(DRIVERS)\makefile.inc

#  The contents of the libraries named above:

BOBJ = callback.obj  delay.obj    irq_mask.obj     \
       low.obj       netsubs.obj  pciconf.obj  repio.obj \
       util.obj	     get_conf.obj __ntohs.obj  intsubs.obj	\
       scsisubs.obj  is_eisa.obj

BMOD = callback+delay+irq_mask+low+netsubs+pciconf+repio+ \
       util+get_conf+__ntohs+intsubs+scsisubs+is_eisa

COBJ = fsprintf.obj ldiv.obj      memcmp.obj   memcpy.obj    memset.obj  \
       printf.obj   putc.obj      strchr.obj   strcmp.obj    strcpy.obj  \
       strlen.obj   strncpy.obj   vfprintf.obj ll_shift.obj  us_div.obj	 \
       us_mod.obj   ul_mul.obj    rl_shift.obj ra_shift.obj  pcopy.obj

CMOD = fsprintf+ldiv+memcmp+memcpy+memset+pcopy+printf+putc+\
	strchr+strcmp+strcpy+strlen+strncpy+vfprintf+ll_shift+\
	us_div+us_mod+ul_mul+rl_shift+ra_shift

OOBJ = crt0.obj
OMOD = crt0

#  Default production is to build all 3 libraries:
install all: $(OLIB) $(CLIB) $(BLIB) $(BCRT0) $(DBCRT0)

# C startup object for driver entry
$(BCRT0): crt0.obj
	copy crt0.obj $(BCRT0)

# Extra C startup object for debug versions of drivers
$(DBCRT0): dbgcrt0.s
	ml /Fo$(DBCRT0) /c $(AFLAGS) dbgcrt0.s

# C startup library
$(OLIB): $(OOBJ)
	if exist $(OLIB) erase $(OLIB)
	LIB /NOLOGO @<<olib.rsp
	$(OLIB) $(OMOD), ;
<<

#  Stripped-down C library
$(CLIB): $(COBJ)
	if exist $(CLIB) erase $(CLIB)
	LIB /NOLOGO @<<clib.rsp
	$(CLIB) $(CMOD), ;
<<

#  Driver (application) library
$(BLIB): $(BOBJ)
	if exist $(BLIB) erase $(BLIB)
	LIB /NOLOGO @<<blib.rsp
	$(BLIB) $(BMOD), ;
<<

#  Clean up the intermediate files
clean:
	-if exist *.obj del *.obj
	-if exist *.cod del *.cod
	-if exist $(OLIB)   del $(OLIB)
	-if exist $(CLIB)   del $(CLIB)
	-if exist $(BLIB)   del $(BLIB)
	-if exist $(BCRT0)  del $(BCRT0)
	-if exist $(DBCRT0) del $(DBCRT0)
