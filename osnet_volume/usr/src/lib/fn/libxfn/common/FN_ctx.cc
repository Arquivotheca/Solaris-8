/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_ctx.cc	1.8	97/10/16 SMI"

#include <xfn/xfn.hh>
#include <stdlib.h>

extern "C"
void _pure_error_(void)
{
        abort();
}

FN_namelist::~FN_namelist()
{
}

FN_bindinglist::~FN_bindinglist()
{
}

FN_valuelist::~FN_valuelist()
{
}

FN_multigetlist::~FN_multigetlist()
{
}

FN_searchlist::~FN_searchlist()
{
}

FN_ext_searchlist::~FN_ext_searchlist()
{
}

FN_ctx::~FN_ctx()
{
}
