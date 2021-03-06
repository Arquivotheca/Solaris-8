/*
 * Copyright (c) 1996 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _ATA_DISK_H
#define	_ATA_DISK_H

#pragma ident	"@(#)ata_disk.h	1.15	99/02/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * ATA disk commands.
 */

#define	ATC_SEEK	0x70    /* seek cmd, bottom 4 bits step rate 	*/
#define	ATC_RDVER	0x40	/* read verify cmd			*/
#define	ATC_RDSEC	0x20    /* read sector cmd			*/
#define	ATC_RDLONG	0x23    /* read long without retry		*/
#define	ATC_WRSEC	0x30    /* write sector cmd			*/
#define	ATC_SETMULT	0xc6	/* set multiple mode			*/
#define	ATC_RDMULT	0xc4	/* read multiple			*/
#define	ATC_WRMULT	0xc5	/* write multiple			*/
#define	ATC_READ_DMA	0xc8	/* read (multiple) w/DMA		*/
#define	ATC_WRITE_DMA	0xca	/* write (multiple) w/DMA		*/
#define	ATC_SETPARAM	0x91	/* set parameters command 		*/
#define	ATC_ID_DEVICE	0xec    /* IDENTIFY DEVICE command 		*/
#define	ATC_ACK_MC	0xdb	/* acknowledge media change		*/

/*
 * Low bits for Read/Write commands...
 */
#define	ATCM_ECCRETRY	0x01    /* Enable ECC and RETRY by controller 	*/
				/* enabled if bit is CLEARED!!! 	*/
#define	ATCM_LONGMODE	0x02    /* Use Long Mode (get/send data & ECC) 	*/

#ifdef  DADKIO_RWCMD_READ
#define	RWCMDP(pktp)  ((struct dadkio_rwcmd *)((pktp)->cp_bp->b_back))
#endif

/* useful macros */

#define	CPKT2GCMD(cpkt)	((gcmd_t *)(cpkt)->cp_ctl_private)
#define	CPKT2APKT(cpkt)  (GCMD2APKT(CPKT2GCMD(cpkt)))

#define	GCMD2CPKT(cmdp)	((struct cmpkt *)((cmdp)->cmd_pktp))
#define	APKT2CPKT(apkt) (GCMD2CPKT(APKT2GCMD(apkt)))

/* public function prototypes */

int	ata_disk_attach(ata_ctl_t *ata_ctlp);
void	ata_disk_detach(ata_ctl_t *ata_ctlp);
int	ata_disk_init_drive(ata_drv_t *ata_drvp);
void	ata_disk_uninit_drive(ata_drv_t *ata_drvp);
int	ata_disk_id(ddi_acc_handle_t io_hdl1, caddr_t ioaddr1,
		ddi_acc_handle_t io_hdl2, caddr_t ioaddr2,
		struct ata_id *ata_idp);
int	ata_disk_bus_ctl(dev_info_t *d, dev_info_t *r, ddi_ctl_enum_t o,
		void *a, void *v);
int	ata_disk_setup_parms(ata_ctl_t *ata_ctlp, ata_drv_t *ata_drvp);

#ifdef	__cplusplus
}
#endif

#endif /* _ATA_DISK_H */
