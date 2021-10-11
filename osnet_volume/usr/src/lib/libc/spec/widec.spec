#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)widec.spec	1.2	99/05/14 SMI"
#
# lib/libc/spec/widec.spec

function	fgetws
include		<stdio.h>, <widec.h>
declaration	wchar_t *fgetws(wchar_t *s, int n, FILE *stream)
version		SUNW_1.1
errno		EOVERFLOW
exception	$return == 0
end

function	getws
include		<stdio.h>, <widec.h>
declaration	wchar_t *getws(wchar_t *s)
version		SUNW_1.1
errno		EOVERFLOW
exception	$return == 0
end

function	putws
include		<stdio.h>, <widec.h>
declaration	int putws(const wchar_t *s)
version		SUNW_1.1
exception	$return == EOF
end

function	wscasecmp
include		<widec.h>
declaration	int wscasecmp(const wchar_t *s1, const wchar_t *s2)
version		SUNW_1.1
end

function	wscol
include		<widec.h>
declaration	int wscol(const wchar_t *s)
version		SUNW_1.1
end

function	wsdup
include		<widec.h>
declaration	wchar_t *wsdup(const wchar_t *s)
version		SUNW_1.1
exception	$return == 0
end

function	wsncasecmp
include		<widec.h>
declaration	int wsncasecmp(const wchar_t *s1, const wchar_t *s2, size_t n)
version		SUNW_1.1
end

function	wsprintf
include		<stdio.h>, <widec.h>
declaration	int wsprintf(wchar_t *s, const char *format, ... )
version		SUNW_1.1
exception	$return < 0
end

function	wsscanf
include		<stdio.h>, <widec.h>
declaration	int wsscanf(wchar_t *s, const char *format, ... )
version		SUNW_1.1
exception	$return < 0
end
