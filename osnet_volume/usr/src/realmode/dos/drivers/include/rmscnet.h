/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */

/* The #ident directive confuses the DOS linker */
/*
#ident "@(#)rmscnet.h	1.4	97/03/07 SMI"
*/

/*
 * Definitions for the network realmode driver interface.
 */

#ifndef _RMSC_NET_H_
#define	_RMSC_NET_H_

#include <rmsc.h>

/* Form of the routine to be called in response to packet reception */
typedef void (*recv_callback_t)(ushort);

/*
 *	Initialization structure passed by the network layer to the
 *	hardware-specific code's initialization structure.
 *
 *	In the following structure definition the comment at the
 *	start of the each line containing structure members indicates
 *	whether the network_driver_init routine is required to initialize 
 *	that member (REQ) or whether initialization is optional (OPT).
 */
typedef struct network_driver_init
{
/* REQ */    char *driver_name;
/* OPT */    void (*legacy_probe)(void);
/* REQ */    int (*configure)(rmsc_handle *, char **);
/* OPT */    int (*modify_dev_info)(rmsc_handle, struct bdev_info *);
/* REQ */    int (*initialize)(rmsc_handle, unchar *);
/* REQ */    void (*device_interrupt)(void);
/* REQ */    void (*open)(void);
/* OPT */    void (*close)(void);
/* REQ */    void (*send_packet)(unchar far *, ushort);
/* REQ */    void (*receive_packet)(unchar far *, ushort, recv_callback_t);
} rmsc_network_driver_init;

extern int network_driver_init(rmsc_network_driver_init *);

/* Definitions for service interrupt: vector number and functions */
#define	SVR_INT_NUM	0xFB
#define	INIT_CALL	0
#define	OPEN_CALL	1
#define	CLOSE_CALL	2
#define	SEND_CALL	3
#define	RECEIVE_CALL	4
#define	GET_ADDR_CALL	5

#endif /* _RMSC_NET_H_ */
