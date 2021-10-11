/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_LIBGENIO_H
#define	_LIBGENIO_H

#pragma ident	"@(#)libgenIO.h	1.7	92/07/14 SMI"	/* SVr4.0 1.2	*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *  The following device types and device table are specific to
 *  the Sun machines and are static. The correspondence between the
 *  device type and the major number is static and would disappear
 *  if a dynamic configuration takes place.
 */

/* device types */

#define	G_TM_TAPE	1	/* Tapemaster controller    */
#define	G_XY_DISK	3	/* xy disks		*/
#define	G_SD_DISK	7	/* scsi sd disk		*/
#define	G_XT_TAPE	8	/* xt tapes		*/
#define	G_SF_FLOPPY	9	/* sf floppy		*/
#define	G_XD_DISK	10	/* xd disks		*/
#define	G_ST_TAPE	11	/* scsi tape		*/
#define	G_NS		12	/* noswap pseudo-dev	*/
#define	G_RAM		13	/* ram pseudo-dev	*/
#define	G_FT		14	/* tftp			*/
#define	G_HD		15	/* 386 network disk	*/
#define	G_FD		16	/* 386 AT disk		*/
#define	G_FILE		28	/* file, not a device	*/
#define	G_NO_DEV	29	/* device does not require special treatment */
#define	G_DEV_MAX	30	/* last valid device type */

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBGENIO_H */
