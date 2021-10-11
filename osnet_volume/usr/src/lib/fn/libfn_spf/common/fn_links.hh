/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FN_LINKS_HH
#define	_FN_LINKS_HH

#pragma ident	"@(#)fn_links.hh	1.1	96/03/31 SMI"

#include <xfn/xfn.hh>
#include <xfn/fn_spi.h>
#include "FN_ctx_func_info.hh"

extern int
fn_process_link(FN_status &s,
    unsigned int authoritative,
    unsigned int continue_code = FN_E_SPI_CONTINUE,
    FN_ref **answer = 0);

extern int
fn_attr_process_link(FN_status &s,
    unsigned int authoritative,
    unsigned int follow_link,
    FN_ctx_func_info_t *packet);

#endif /* _FN_LINKS_HH */
