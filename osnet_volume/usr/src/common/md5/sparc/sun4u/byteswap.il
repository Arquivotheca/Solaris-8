/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * byteswap.il -- definitions for .inline functions for little-endian loads.
 *
 * "@(#)byteswap.il 1.2     98/02/20 SMI"
 *
 * right now, this file is only used on UltraSPARC architectures,
 * which have a special address space identifier for loading 32 and 16 bit
 * integers in little-endian byte order.
 *
 * note: apparently these files are not preprocessed, so we cannot
 *       use `#pragma ident' or `ASI_PL'.
 *
 */

	.inline	load_little_32, 4
	lduwa	[%o0]0x88, %o0
	.end

	.inline load_little_16, 4
	lduha	[%o0]0x88, %o0
	.end
