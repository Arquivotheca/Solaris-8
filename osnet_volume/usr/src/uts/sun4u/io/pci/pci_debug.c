/*
 * Copyright (c) 1991-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pci_debug.c	1.5	99/04/23 SMI"

/*
 * PCI nexus driver general debug support
 */
#include <sys/promif.h>		/* prom_printf */
#include <sys/async.h>
#include <sys/sunddi.h>		/* dev_info_t */
#include <sys/ddi_impldefs.h>
#include <sys/pci/pci_obj.h>

/*LINTLIBRARY*/

#ifdef DEBUG
extern uint64_t pci_debug_flags;
void
pci_debug(uint64_t flag, dev_info_t *dip, char *fmt,
	uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5)
{
	char *s = NULL;
	u_int cont = 0;

	if (flag & DBG_CONT) {
		flag &= ~DBG_CONT;
		cont = 1;
	}
	if ((pci_debug_flags & flag) == flag) {
		switch (flag) {
		case DBG_ATTACH:	s = "attach";			break;
		case DBG_DETACH:	s = "detach";			break;

		case DBG_MAP:		s = "map";			break;

		case DBG_G_ISPEC:	s = "get_intrspec";		break;
		case DBG_A_ISPEC:	s = "add_intrspec";		break;
		case DBG_R_ISPEC:	s = "remove_intrspec";		break;
		case DBG_INIT_CLD:	s = "init_child";		break;

		case DBG_CTLOPS:	s = "ctlops";			break;
		case DBG_INTR:		s = "intr_wrapper";		break;
		case DBG_ERR_INTR:	s = "pbm_error_intr";		break;
		case DBG_BUS_FAULT:	s = "pci_fault";		break;

		case DBG_DMA_ALLOCH:	s = "dma_alloc_handle";		break;
		case DBG_DMA_FREEH:	s = "dma_free_handle";		break;
		case DBG_DMA_BINDH:	s = "dma_bind_handle";		break;
		case DBG_DMA_UNBINDH:	s = "dma_unbind_handle";	break;

		case DBG_DMA_MAP:	s = "dma_map";			break;
		case DBG_CHK_MOD:	s = "check_dma_mode";		break;
		case DBG_BYPASS:	s = "bypass";			break;
		case DBG_IOMMU:		s = "iommu";			break;

		case DBG_DMA_WIN:	s = "dma_win";			break;
		case DBG_MAP_WIN:	s = "map_window";		break;
		case DBG_UNMAP_WIN:	s = "unmap_window";		break;
		case DBG_DMA_CTL:	s = "dma_ctl";			break;

		case DBG_DMA_FLUSH:	s = "dma_flush";		break;
		case DBG_DMASYNC_FLUSH:	s = "sabre/simba dvma";		break;
		case DBG_FAST_DVMA:	s = "fast_dvma";		break;

		case DBG_SC:		s = "sc";			break;
#ifndef lint
		case DBG_IB:		s = "ib";			break;
		case DBG_CB:		s = "cb";			break;
		case DBG_PBM:		s = "pbm";			break;

		case DBG_OPEN:		s = "open";			break;
		case DBG_CLOSE:		s = "close";			break;
		case DBG_IOCTL:		s = "ioctl";			break;
#endif
		default:		s = "PCI debug unknown";	break;
		}

		if (s && cont == 0) {
			prom_printf("%s(%d): %s: ", ddi_driver_name(dip),
			    ddi_get_instance(dip), s);
		}
		prom_printf(fmt, a1, a2, a3, a4, a5);
	}
}
#endif
