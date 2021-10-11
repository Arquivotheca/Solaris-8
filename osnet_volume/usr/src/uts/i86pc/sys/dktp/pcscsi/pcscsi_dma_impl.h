/*
 * Copyright (c) 1999 Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)pcscsi_dma_impl.h	99/03/08 SMI"


#ifndef _SYS_DKTP_PCSCSI_DMA_IMPL_H
#define	_SYS_DKTP_PCSCSI_DMA_IMPL_H


#ifdef	__cplusplus
extern "C" {
#endif


/* ---------------------------------------------------------------------- */
/*	Includes	*/



/* ---------------------------------------------------------------------- */
/*	Defines		*/

#ifndef MAX_SG_LIST_ENTRIES
#define	MAX_SG_LIST_ENTRIES	32
#endif


/* ---------------------------------------------------------------------- */
/*	Structure definitions	*/


/* Scatter Gather list entry */
typedef struct {
	paddr_t data_addr;
	ulong   data_len;
} sg_list_entry_t;


typedef struct {
	sg_list_entry_t		sg_entries[MAX_SG_LIST_ENTRIES];
} sg_list_t;

typedef caddr_t	sg_list_virtaddrs_t[MAX_SG_LIST_ENTRIES];


typedef struct 	{
	/* Following field is generally used for a back-pointer to the ccb. */
	opaque_t		*dma_driver_priv;	/* For HBA driver use */

	/* DMA resources.	*/
	ulong			dma_flags;	/* flags from tran_init_pkt */
	struct buf		*dma_buf_p;	/* buf pointer */

	/* Initialized/used by dma_impl_init. */
	ulong			dma_seg_xfer_cnt; /* size of THIS transfer */
	ulong			dma_totxfer;	/* running xfer cnt for pkt */

	sg_list_t		*dma_sg_list_p;	/* s/g entries list (DMAable) */
	int			dma_sg_nbr_entries; /* Used entries in sglist */

	/* Initialized/used by dma_impl_setup */
	ddi_dma_handle_t	dma_handle;	/* dma handle */
	union {
		ddi_dma_win_t	d_win;		/* dma window		*/
		caddr_t		d_addr;		/* transfer address	*/
	} cm;
	ddi_dma_seg_t		dma_seg;	/* Current DMA segment	*/

#ifdef SAVE_DMA_SEG_VIRT_ADDRS
	/* Virtual addresses corresponding to the start of each DMA segment */
	sg_list_virtaddrs_t	dma_sg_list_virtaddrs;
#endif /* SAVE_DMA_SEG_VIRT_ADDRS */

} dma_blk_t;

#define	dma_dmawin		cm.d_win
#define	dma_addr		cm.d_addr


/* ---------------------------------------------------------------------- */
/*	Function prototypes	*/
int
dma_impl_setup(
	dev_info_t		*dip,
	dma_blk_t		*dma_state_p,
	struct buf		*bp,
	int			flags,
	opaque_t		driver_private,
	ddi_dma_lim_t		*dma_lim_p,
	int			(*callback)(),
	caddr_t			arg,
	boolean_t		new_transfer);

/* ---------------------------------------------------------------------- */
#ifdef	__cplusplus
}
#endif

#endif  /* _SYS_DKTP_PCSCSI_DMA_IMPL_H */
