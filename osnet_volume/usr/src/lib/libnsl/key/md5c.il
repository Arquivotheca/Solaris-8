/*
 * md5c.il -- definitions for .inline functions called from md5c.c
 *
 * "@(#)md5c.il 1.1     97/11/19 SMI"
 *
 * right now, this file is only used on UltraSPARC architectures,
 * which have a special address space identifier for loading 32-bit
 * integers in little-endian byte order.  if other assembly-level
 * optimizations become available, they should be placed in here.
 *
 * note: apparently these files are not preprocessed, so we cannot
 *       use `#pragma ident' or `ASI_PL'.
 *
 */

	.inline	load_little_32, 4
	lda	[%o0]0x88, %o0
	.end
