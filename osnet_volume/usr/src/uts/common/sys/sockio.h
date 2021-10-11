/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *	(c) 1986-1989,1991,1993,1995-1999  Sun Microsystems, Inc.
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

#ifndef	_SYS_SOCKIO_H
#define	_SYS_SOCKIO_H

#pragma ident	"@(#)sockio.h	1.25	99/11/07 SMI"	/* SVr4.0 1.8 */

/*
 * General socket ioctl definitions.
 */

#include <sys/ioccom.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* socket i/o controls */
#define	SIOCSHIWAT	_IOW('s',  0, int)		/* set high watermark */
#define	SIOCGHIWAT	_IOR('s',  1, int)		/* get high watermark */
#define	SIOCSLOWAT	_IOW('s',  2, int)		/* set low watermark */
#define	SIOCGLOWAT	_IOR('s',  3, int)		/* get low watermark */
#define	SIOCATMARK	_IOR('s',  7, int)		/* at oob mark? */
#define	SIOCSPGRP	_IOW('s',  8, int)		/* set process group */
#define	SIOCGPGRP	_IOR('s',  9, int)		/* get process group */

/*
 * SIOCADDRT and SIOCDELRT ioctls need to be defined using _IOWN macro to
 * make them datamodel independent.
 */
#define	SIOCADDRT	_IOWN('r', 10, 48)		/* add route */
#define	SIOCDELRT	_IOWN('r', 11, 48)		/* delete route */

/* For multicast routing. These might change in future release */
#define	SIOCGETVIFCNT	_IOWR('r', 20, struct sioc_vif_req)
							/* get vif pkt count */
#define	SIOCGETSGCNT	_IOWR('r', 21, struct sioc_sg_req)
							/* get s,g pkt count */
#define	SIOCGETLSGCNT	_IOWR('r', 21, struct sioc_lsg_req)
							/* get s,g pkt count */

/*
 * Obsolete interface ioctls using struct ifreq that are supported
 * for compatibility. New interface ioctls use struct lifreq.
 */
#define	SIOCSIFADDR	_IOW('i',  12, struct ifreq)	/* set if address */
#define	SIOCGIFADDR	_IOWR('i', 13, struct ifreq)	/* get if address */
#define	SIOCSIFDSTADDR	_IOW('i',  14, struct ifreq)	/* set p-p address */
#define	SIOCGIFDSTADDR	_IOWR('i', 15, struct ifreq)	/* get p-p address */
#define	SIOCSIFFLAGS	_IOW('i',  16, struct ifreq)	/* set if flags */
#define	SIOCGIFFLAGS	_IOWR('i', 17, struct ifreq)	/* get if flags */
#define	SIOCSIFMEM	_IOW('i',  18, struct ifreq)	/* set interface mem */
#define	SIOCGIFMEM	_IOWR('i', 19, struct ifreq)	/* get interface mem */

/*
 * Needs to be defined using _IOWRN macro to make it datamodel independent.
 * Argument is a struct ifconf.
 */
#define	SIOCGIFCONF	_IOWRN('i', 20, 8)		/* get if list */

#define	SIOCSIFMTU	_IOW('i',  21, struct ifreq)	/* set if mtu */
#define	SIOCGIFMTU	_IOWR('i', 22, struct ifreq)	/* get if mtu */

	/* from 4.3BSD */
#define	SIOCGIFBRDADDR	_IOWR('i', 23, struct ifreq)	/* get broadcast addr */
#define	SIOCSIFBRDADDR	_IOW('i',  24, struct ifreq)	/* set broadcast addr */
#define	SIOCGIFNETMASK	_IOWR('i', 25, struct ifreq)	/* get subnetmask */
#define	SIOCSIFNETMASK	_IOW('i',  26, struct ifreq)	/* set subnetmask */
#define	SIOCGIFMETRIC	_IOWR('i', 27, struct ifreq)	/* get if metric */
#define	SIOCSIFMETRIC	_IOW('i',  28, struct ifreq)	/* set if metric */

#define	SIOCSARP	_IOW('i',  30, struct arpreq)	/* set arp entry */
#define	SIOCGARP	_IOWR('i', 31, struct arpreq)	/* get arp entry */
#define	SIOCDARP	_IOW('i',  32, struct arpreq)	/* delete arp entry */
#define	SIOCUPPER	_IOW('i',  40, struct ifreq)	/* attach upper layer */
#define	SIOCLOWER	_IOW('i',  41, struct ifreq)	/* attach lower layer */
#define	SIOCSETSYNC	_IOW('i',  44, struct ifreq)	/* set syncmode */
#define	SIOCGETSYNC	_IOWR('i', 45, struct ifreq)	/* get syncmode */
#define	SIOCSSDSTATS	_IOWR('i', 46, struct ifreq)	/* sync data stats */
#define	SIOCSSESTATS	_IOWR('i', 47, struct ifreq)	/* sync error stats */

#define	SIOCSPROMISC	_IOW('i',  48, int)		/* request promisc */
							/* mode on/off */
#define	SIOCADDMULTI	_IOW('i',  49, struct ifreq)	/* set m/c address */
#define	SIOCDELMULTI	_IOW('i',  50, struct ifreq)	/* clr m/c address */

/* STREAMS based socket emulation */

#define	SIOCGETNAME	_IOR('s',  52, struct sockaddr)	/* getsockname */
#define	SIOCGETPEER	_IOR('s',  53, struct sockaddr)	/* getpeername */
#define	IF_UNITSEL	_IOW('s',  54, int)		/* set unit number */
#define	SIOCXPROTO	_IO('s',   55)			/* empty proto table */

#define	SIOCIFDETACH	_IOW('i',  56, struct ifreq)	/* detach interface */
#define	SIOCGENPSTATS	_IOWR('i', 57, struct ifreq)	/* get ENP stats */
#define	SIOCX25XMT	_IOWR('i', 59, struct ifreq)	/* start a slp proc */
							/* in x25if */
#define	SIOCX25RCV	_IOWR('i', 60, struct ifreq)	/* start a slp proc */
							/* in x25if */
#define	SIOCX25TBL	_IOWR('i', 61, struct ifreq)	/* xfer lun table to */
							/* kernel */
#define	SIOCSLGETREQ	_IOWR('i', 71, struct ifreq)	/* wait for switched */
							/* SLIP request */
#define	SIOCSLSTAT	_IOW('i',  72, struct ifreq)	/* pass SLIP info to */
							/* kernel */
#define	SIOCSIFNAME	_IOW('i',  73, struct ifreq)	/* set interface name */
#define	SIOCGENADDR	_IOWR('i', 85, struct ifreq)	/* Get ethernet addr */
#define	SIOCGIFNUM	_IOR('i',  87, int)		/* get number of ifs */

#define	SIOCGIFMUXID	_IOWR('i', 88, struct ifreq)	/* get if muxid */
#define	SIOCSIFMUXID	_IOW('i',  89, struct ifreq)	/* set if muxid */

#define	SIOCGIFINDEX	_IOWR('i', 90, struct ifreq)	/* get if index */
#define	SIOCSIFINDEX	_IOW('i',  91, struct ifreq)	/* set if index */

/*
 * New interface ioctls that use the struct lifreq. Can be used for
 * both IPv4 and IPv6.
 */
#define	SIOCLIFREMOVEIF	_IOW('i',  110, struct lifreq)	/* delete logical */
#define	SIOCLIFADDIF	_IOWR('i', 111, struct lifreq)	/* create logical */

#define	SIOCSLIFADDR	_IOW('i',  112, struct lifreq)	/* set if address */
#define	SIOCGLIFADDR	_IOWR('i', 113, struct lifreq)	/* get if address */
#define	SIOCSLIFDSTADDR	_IOW('i',  114, struct lifreq)	/* set p-p address */
#define	SIOCGLIFDSTADDR	_IOWR('i', 115, struct lifreq)	/* get p-p address */
#define	SIOCSLIFFLAGS	_IOW('i',  116, struct lifreq)	/* set if flags */
#define	SIOCGLIFFLAGS	_IOWR('i', 117, struct lifreq)	/* get if flags */

/*
 * Needs to be defined using _IOWRN macro to make it datamodel independent.
 * Argument is a struct lifconf.
 */
#define	SIOCGLIFCONF	_IOWRN('i', 120, 16)		/* get if list */
#define	SIOCSLIFMTU	_IOW('i',  121, struct lifreq)	/* set if mtu */
#define	SIOCGLIFMTU	_IOWR('i', 122, struct lifreq)	/* get if mtu */
#define	SIOCGLIFBRDADDR	_IOWR('i', 123, struct lifreq)	/* get broadcast addr */
#define	SIOCSLIFBRDADDR	_IOW('i',  124, struct lifreq)	/* set broadcast addr */
#define	SIOCGLIFNETMASK	_IOWR('i', 125, struct lifreq)	/* get subnetmask */
#define	SIOCSLIFNETMASK	_IOW('i',  126, struct lifreq)	/* set subnetmask */
#define	SIOCGLIFMETRIC	_IOWR('i', 127, struct lifreq)	/* get if metric */
#define	SIOCSLIFMETRIC	_IOW('i',  128, struct lifreq)	/* set if metric */
#define	SIOCSLIFNAME	_IOWR('i',  129, struct lifreq)	/* set interface name */
#define	SIOCGLIFNUM	_IOWR('i', 130, struct lifnum)	/* get number of ifs */
#define	SIOCGLIFMUXID	_IOWR('i', 131, struct lifreq)	/* get if muxid */
#define	SIOCSLIFMUXID	_IOW('i',  132, struct lifreq)	/* set if muxid */

#define	SIOCGLIFINDEX	_IOWR('i', 133, struct lifreq)	/* get if index */
#define	SIOCSLIFINDEX	_IOW('i',  134, struct lifreq)	/* set if index */

#define	SIOCSLIFTOKEN	_IOW('i',  135, struct lifreq)	/* Set token for link */
							/* local address and */
							/* autoconf */
#define	SIOCGLIFTOKEN	_IOWR('i', 136, struct lifreq)	/* Get token for link */
							/* local address and */
							/* autoconf */

#define	SIOCSLIFSUBNET	_IOW('i',  137, struct lifreq)	/* set subnet prefix */
#define	SIOCGLIFSUBNET	_IOWR('i', 138, struct lifreq)	/* get subnet prefix */

#define	SIOCSLIFLNKINFO _IOW('i',  139, struct lifreq)	/* set link info */
#define	SIOCGLIFLNKINFO _IOWR('i', 140, struct lifreq)	/* get link info */

#define	SIOCLIFDELND	_IOW('i',  141, struct lifreq)	/* Delete ND entry */
#define	SIOCLIFGETND	_IOWR('i', 142, struct lifreq)	/* Get ND entry */
#define	SIOCLIFSETND	_IOW('i',  143, struct lifreq)	/* Set ND entry */

/*
 * Address querying ioctls.
 */
#define	SIOCTMYADDR	_IOWR('i', 144, struct sioc_addrreq)
							/* My address? */
#define	SIOCTONLINK	_IOWR('i', 145, struct sioc_addrreq)
							/* Address on-link? */
#define	SIOCTMYSITE	_IOWR('i', 146, struct sioc_addrreq)
							/* In this site? */

#define	SIOCGTUNPARAM	_IOR('i',  147, struct iftun_req)
							/* get tunnel */
							/* parameters */
#define	SIOCSTUNPARAM	_IOW('i',  148, struct iftun_req)
							/* set tunnel */
							/* parameters */

#define	SIOCFIPSECONFIG	_IOW('i',  149, 0)		/* Flush Policy  */
#define	SIOCSIPSECONFIG	_IOW('i',  150, 0)		/* Set Policy */
#define	SIOCDIPSECONFIG	_IOW('i',  151, 0)		/* Delete Policy */
#define	SIOCLIPSECONFIG	_IOW('i',  152, 0)		/* List Policy */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SOCKIO_H */
