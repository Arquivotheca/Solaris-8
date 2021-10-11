#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)private.spec	1.1	99/04/07 SMI"
#
# lib/libsecdb/spec/private.spec

function	_argv_to_csl
include		<secdb.h>
declaration	char *_argv_to_csl(char **src)
execption	$return == NULL
version		SUNWprivate_1.1
end		

function	_csl_to_argv
include		<secdb.h>
declaration	char **_csl_to_argv(char *csl)
execption	$return == NULL
version		SUNWprivate_1.1
end		

function	_do_unescape
include		<secdb.h>
declaration	char *_do_unescape(char *src)
execption	$return == NULL
version		SUNWprivate_1.1
end		

function	_free_argv
include		<secdb.h>
declaration	void _free_argv()
version		SUNWprivate_1.1
end		

function	_insert2kva
include		<secdb.h>
declaration	int _insert2kva(kva_t *kva, char *key, char *value)
execption	$return == 0
version		SUNWprivate_1.1
end		

function	_kva2str
include		<secdb.h>
declaration	int _kva2str(kva_t *kva, char *buf, int buflen, char *ass, \
	char *del)
execption	$return == 0
version		SUNWprivate_1.1
end		

function	_kva_dup
include		<secdb.h>
declaration	kva_t *_kva_dup(kva_t *old_kva)
version		SUNWprivate_1.1
end		

function	_kva_free
include		<secdb.h>
declaration	void _kva_free(kva_t *kva)
version		SUNWprivate_1.1
end		

function	_new_kva
include		<secdb.h>
declaration	kva_t *_new_kva(int size)
execption	$return == NULL
version		SUNWprivate_1.1
end		

function	_str2kva
include		<secdb.h>
declaration	kva_t *_str2kva(char *s, char *ass, char *del)
execption	$return == NULL
version		SUNWprivate_1.1
end		
