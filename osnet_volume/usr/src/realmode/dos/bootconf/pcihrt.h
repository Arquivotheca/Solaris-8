/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * pcihrt.h -- PCI Hot-Plug Resource Table
 */

#ifndef	_PCIHRT_H
#define	_PCIHRT_H

#ident "@(#)pcihrt.h   1.1   99/04/01 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct hrt_hdr { /* PCI Hot-Plug Configuration Resource Table header */
	u_long	hrt_sig;	/* $HRT 				*/
	u_short	hrt_avail_imap;	/* Bitmap of unused IRQs		*/
	u_short hrt_used_imap;	/* Bitmap of IRQs used by PCI 		*/
	u_char	hrt_entry_cnt;	/* no. of PCI hot-plug slot entries	*/
	u_char	hrt_ver;	/* version no. = 1			*/
	u_char	hrt_resv0;	/* reserved				*/
	u_char	hrt_resv1;	/* reserved				*/
	u_char	hrt_resv2;	/* reserved				*/
	u_char	hrt_resv3;	/* reserved				*/
	u_char	hrt_resv4;	/* reserved				*/
	u_char	hrt_resv5;	/* reserved				*/
};

struct php_entry {	/* PCI hot-plug slot entry */
	u_char	php_devno;	/* PCI dev/func no. of the slot		*/
	u_char	php_pri_bus;	/* Primary bus of this slot		*/
	u_char	php_sec_bus;	/* Secondary bus of this slot		*/
	u_char	php_subord_bus;	/* Max Subordinate bus of this slot	*/
	u_short	php_io_start;	/* allocated I/O space starting addr	*/
	u_short	php_io_size;	/* allocated I/O space size in bytes	*/
	u_short	php_mem_start;	/* allocated Memory space start addr 	*/
	u_short	php_mem_size;	/* allocated Memory space size in 64k	*/
	u_short	php_pfmem_start; /* allocated Prefetchable Memory start	*/
	u_short	php_pfmem_size;	/* allocated Prefetchable size in 64k	*/
};

#ifdef	__cplusplus
}
#endif

#endif	/* _PCIHRT_H */
