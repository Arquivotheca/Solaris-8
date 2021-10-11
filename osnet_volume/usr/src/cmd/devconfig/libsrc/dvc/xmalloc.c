/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)xmalloc.c 1.2 93/09/10 SMI"

#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <libintl.h>

#include "dvc.h"
#include "dvc_msgs.h"
#include "conf.h"
#include "util.h"

typedef struct bfr_struct {
	char*	ptr;
	int	idx;
	int	siz;
} bfr_t;

static void
bfr_add(bfr_t* bfr, char ch)
{
	if ( bfr->idx == bfr->siz )
		bfr->ptr = (char*)xrealloc(bfr->ptr, bfr->siz *= 2);

	bfr->ptr[bfr->idx++] = ch;
}


static void
bfr_add_str(bfr_t* bfr, char* str)
{
	int n = strlen(str);

	if ( bfr->idx + n >= bfr->siz )
		bfr->ptr = (char*)xrealloc(bfr->ptr, bfr->siz = bfr->siz*2 + n);

	memcpy(bfr->ptr + bfr->idx, str, n);
	bfr->idx += n;
}

static void
bfr_free(bfr_t* bfr)
{
	xfree(bfr->ptr);
	xfree(bfr);
}


static bfr_t*
bfr_new()
{
	bfr_t* bfr = (bfr_t*)xmalloc(sizeof(bfr_t));
	bfr->ptr = (char*)xmalloc(bfr->siz = 30);
	bfr->idx = 0;
	return bfr;
}


static char*
bfr_str(bfr_t* bfr)
{
	bfr_add(bfr, '\0');
	return bfr->ptr;
}

void*
xmalloc(size_t n)
{
	void* p = malloc(n);
	if ( p == NULL )
		ui_error_exit( MSG(MEMERR) );
	return p;
}

void*
xzmalloc(size_t n)
{
	void* p = xmalloc(n);
	memset(p, 0, n);
	return p;
}

void
xfree(void* p)
{
	free(p);
}

void*
xrealloc(void* p, size_t n)
{
	if ( p == NULL )
		return xmalloc(n);

	p = realloc(p, n);
	if ( p == NULL )
		ui_error_exit( MSG(MEMERR) );
	return p;
}

char*
xstrdup(char* str)
{
	return strcpy((char*)xmalloc(strlen(str)+1), str);
}

char*
strcats(char* s, ...)
{
	va_list ap;
	char*   cp = s;
	char*   ret_cp;
	bfr_t*  ret;

	ret = bfr_new();

	va_start(ap, s);
	for ( cp=s; cp; cp=va_arg(ap, char*) ) {
		bfr_add_str(ret, cp);
	}
	va_end(ap);

	ret_cp = xstrdup(bfr_str(ret));
	bfr_free(ret);

	return ret_cp;
}
