#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)print.spec	1.2	99/05/04 SMI"
#
# lib/libc/spec/print.spec

function	fprintf 
include		<stdio.h>
declaration	int fprintf(FILE *strm, const char *format, ... )
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 \
		ia64=SUNW_0.7
errno		EFBIG 
exception	$return == -1
end

function	printf 
include		<stdio.h>
declaration	int printf(const char *format, ... )
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EFBIG 
exception	$return == -1
end

function	sprintf 
include		<stdio.h>
declaration	int sprintf(char *s, const char *format, ... )
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EFBIG 
exception	$return == -1
end

function	vfprintf 
include		<stdio.h>, <stdarg.h>
declaration	int vfprintf(FILE *stream,   const char *format, va_list ap)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EFBIG 
exception	
end

function	vprintf 
include		<stdio.h>, <stdarg.h>
declaration	int vprintf(const char *format, va_list ap)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EFBIG 
exception	
end

function	vsprintf 
include		<stdio.h>, <stdarg.h>
declaration	int vsprintf(char *s, const char *format, va_list ap)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7  \
		ia64=SUNW_0.7
errno		EFBIG 
exception	
end

function	fwprintf
include		<stdio.h>, <wchar.h>
declaration	int fwprintf(FILE *stream, const wchar_t *format, ...)
version		SUNW_1.18
end

function	wprintf
include		<stdio.h>, <wchar.h>
declaration	int wprintf(const wchar_t *format, ...)
version		SUNW_1.18
end
