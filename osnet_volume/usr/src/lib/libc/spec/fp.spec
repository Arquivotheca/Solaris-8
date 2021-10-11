#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)fp.spec	1.3	99/05/14 SMI"
#
# lib/libc/spec/fp.spec

function	decimal_to_double 
include		<floatingpoint.h>
declaration	void decimal_to_double(double *px, decimal_mode *pm, \
			decimal_record *pd, fp_exception_field_type *ps)
version		SUNW_0.7
errno		
end

function	decimal_to_extended 
include		<floatingpoint.h>
declaration	void decimal_to_extended(extended *px, decimal_mode *pm, \
			decimal_record *pd, fp_exception_field_type *ps)
version		SUNW_0.7
errno		
end

function	decimal_to_quadruple 
include		<floatingpoint.h>
declaration	void decimal_to_quadruple(quadruple *px, decimal_mode *pm, \
			decimal_record *pd, fp_exception_field_type *ps)
version		SUNW_0.7
errno		
end

function	decimal_to_single 
include		<floatingpoint.h>
declaration	void decimal_to_single(single *px, decimal_mode *pm, \
			decimal_record *pd, fp_exception_field_type *ps)
version		SUNW_0.7
errno		
end

function	double_to_decimal 
include		<floatingpoint.h>
declaration	void double_to_decimal(double *px, decimal_mode *pm, \
			decimal_record *pd, fp_exception_field_type *ps)
version		SUNW_0.7
errno		
end

function	extended_to_decimal 
include		<floatingpoint.h>
declaration	void extended_to_decimal(extended *px, decimal_mode *pm, \
			decimal_record *pd, fp_exception_field_type *ps)
version		SUNW_0.7
errno		
end

function	file_to_decimal 
include		<floatingpoint.h>, <stdio.h>
declaration	void file_to_decimal(char **pc, int nmax, \
			int fortran_conventions, decimal_record *pd, \
			enum decimal_string_form *pform, char **pechar, \
			FILE *pf, int *pnread)
version		SUNW_0.7
errno		
end

function	fpgetmask 
include		<ieeefp.h>
declaration	fp_except fpgetmask(void)
version		SUNW_0.7
exception	false 
end

function	fpgetround 
include		<ieeefp.h>
declaration	fp_rnd fpgetround(void)
version		SUNW_1.1
end

function	fpgetsticky 
include		<ieeefp.h>
declaration	fp_except fpgetsticky(void)
version		SUNW_0.7
end

function	fpsetmask 
include		<ieeefp.h>
declaration	fp_except fpsetmask(fp_except mask)
version		SUNW_0.7
end

function	fpsetround 
include		<ieeefp.h>
declaration	fp_rnd fpsetround(fp_rnd rnd_dir)
version		SUNW_1.1
end

function	fpsetsticky 
include		<ieeefp.h>
declaration	fp_except fpsetsticky(fp_except sticky)
version		SUNW_0.7
end

function	func_to_decimal
include		<floatingpoint.h>, <stdio.h>
declaration	void func_to_decimal(char **pc, int nmax, \
			int fortran_conventions, \
			decimal_record *pd, \
			enum decimal_string_form *pform, \
			char **pechar, \
			int (*pget)(void), int *pnread, \
			int (*punget)(int c))
version		SUNW_0.7 
errno		
end

function	nextafter
declaration	double nextafter(double x, double y)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	_nextafter
weak		nextafter
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	quadruple_to_decimal 
include		<floatingpoint.h>
declaration	void quadruple_to_decimal(quadruple *px, decimal_mode *pm, \
			decimal_record *pd, fp_exception_field_type *ps)
version		SUNW_0.7 
errno		
end

function	scalb
include		<math.h>
declaration	double scalb(double x, double n)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		ERANGE
end

function	_scalb
weak		scalb
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	single_to_decimal 
include		<floatingpoint.h>
declaration	void single_to_decimal(single *px, decimal_mode *pm, \
			decimal_record *pd, fp_exception_field_type *ps)
version		SUNW_0.7 
errno		
end

function	_fpstart
declaration	void _fpstart(void)
arch		i386
version		SYSVABI_1.3
end

function	__fpstart
weak		_fpstart 
arch		i386
version		SYSVABI_1.3 
end

function	string_to_decimal 
include		<floatingpoint.h>, <stdio.h>
declaration	void string_to_decimal(char **pc, int nmax, \
			int fortran_conventions, decimal_record *pd, \
			enum decimal_string_form *pform, char **pechar)
version		SUNW_0.7 
errno		
end

function	isnan
include		<math.h>
declaration	int isnan(double dsrc)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	_isnan
weak		isnan
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	isnand
include		<ieeefp.h>
declaration	int isnand(double dsrc)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
exception	$return == 0
end

function	_isnand
weak		isnand
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
end

function	isnanf 
include		<ieeefp.h>
declaration	int isnanf(float fsrc)
version		SUNW_0.7 
exception	$return == 0
end
