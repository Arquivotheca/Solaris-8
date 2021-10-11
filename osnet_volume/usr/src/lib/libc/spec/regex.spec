#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)regex.spec	1.1	99/01/25 SMI"
#
# lib/libc/spec/regex.spec

function	glob 
include		<glob.h>
declaration	int glob(const char *pattern, int flags, int (*errfunc)(const char *epath, int eerrno), glob_t *pglob)
version		SUNW_0.8 
exception	$return != 0
end		

function	globfree 
include		<glob.h>
declaration	void globfree(glob_t *pglob)
version		SUNW_0.8 
end		

function	re_comp 
include		<re_comp.h>
declaration	char *re_comp(const char *string)
version		SUNW_0.9 
exception	$return != 0
end		

function	re_exec 
include		<re_comp.h>
declaration	int re_exec(const char *string)
version		SUNW_0.9 
exception	$return == 1 || $return == -1
end		

function	wordexp 
include		<wordexp.h>
declaration	int wordexp(const	char  *words,  			wordexp_t  *pwordexp,  int flags)
version		SUNW_0.8 
exception	$return == 0
end		

function	wordfree 
include		<wordexp.h>
declaration	void wordfree(wordexp_t *pwordexp)
version		SUNW_0.8 
end		

