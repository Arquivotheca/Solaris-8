/*
 *	Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma	ident	"@(#)sparcdep.c	1.3	94/09/20 SMI"

#include "libtnf.h"

tnf_uint32_t
_tnf_swap32(tnf_uint32_t x)
{
	return (((x<<24) | (((x>>8)<<24)>>8) | (((x<<8)>>24)<<8) | (x>>24)));
}

tnf_uint16_t
_tnf_swap16(tnf_uint16_t x)
{
	return (((x<<8) | (x>>8)));
}
