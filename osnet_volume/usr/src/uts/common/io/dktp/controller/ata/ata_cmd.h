/*
 * Copyright (c) 1996 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _ATA_CMD_H
#define	_ATA_CMD_H

#pragma ident	"@(#)ata_cmd.h	1.1	97/09/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Common ATA commands.
 */
#define	ATC_DIAG	0x90    /* diagnose command 			*/
#define	ATC_RECAL	0x10	/* restore cmd, bottom 4 bits step rate */
#define	ATC_FORMAT	0x50	/* format track command 		*/
#define	ATC_SET_FEAT	0xef	/* set features				*/
#define	ATC_IDLE_IMMED	0xe1	/* idle immediate			*/
#define	ATC_STANDBY_IM	0xe0	/* standby immediate			*/
#define	ATC_DOOR_LOCK	0xde	/* door lock				*/
#define	ATC_DOOR_UNLOCK	0xdf	/* door unlock				*/
#define	ATC_IDLE	0xe3	/* idle					*/

/*
 * ATA/ATAPI-4 disk commands.
 */
#define	ATC_DEVICE_RESET	0x08    /* ATAPI device reset */
#define	ATC_EJECT		0xed	/* media eject */
#define	ATC_FLUSH_CACHE		0xe7	/* flush write-cache */
#define	ATC_ID_DEVICE		0xec    /* IDENTIFY DEVICE */
#define	ATC_ID_PACKET_DEVICE	0xa1	/* ATAPI identify packet device */
#define	ATC_INIT_DEVPARMS	0x91	/* initialize device parameters */
#define	ATC_PACKET		0xa0	/* ATAPI packet */
#define	ATC_RDMULT		0xc4	/* read multiple */
#define	ATC_RDSEC		0x20    /* read sector */
#define	ATC_RDVER		0x40	/* read verify */
#define	ATC_READ_DMA		0xc8	/* read (multiple) w/DMA */
#define	ATC_SEEK		0x70    /* seek */
#define	ATC_SERVICE		0xa2	/* queued/overlap service */
#define	ATC_SETMULT		0xc6	/* set multiple mode */
#define	ATC_WRITE_DMA		0xca	/* write (multiple) w/DMA */
#define	ATC_WRMULT		0xc5	/* write multiple */
#define	ATC_WRSEC		0x30    /* write sector */

/*
 * Low bits for Read/Write commands...
 */
#define	ATCM_ECCRETRY	0x01    /* Enable ECC and RETRY by controller 	*/
				/* enabled if bit is CLEARED!!! 	*/
#define	ATCM_LONGMODE	0x02    /* Use Long Mode (get/send data & ECC) 	*/


/*
 * Obsolete ATA commands.
 */

#define	ATC_RDLONG	0x23    /* read long without retry	*/
#define	ATC_ACK_MC	0xdb	/* acknowledge media change		*/

#ifdef	__cplusplus
}
#endif

#endif /* _ATA_CMD_H */
