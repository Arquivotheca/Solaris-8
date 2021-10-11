/*
 * Copyright (c) 1997, Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef _ATAPI_FSM_H
#define	_ATAPI_FSM_H

#ident "@(#)atapi.h   1.1   97/10/01 SMI\n"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 *
 * The interrupt reason can be interpreted from other bits as follows:
 *
 *  IO  CoD  DRQ
 *  --  ---  ---
 *   0    0    1  == 1 Data to device
 *   0    1    0  == 2 Idle
 *   0    1    1  == 3 Send ATAPI CDB to device
 *   1    0    1  == 5 Data from device
 *   1    1    0  == 6 Status ready
 *   1    1    1  == 7 Future use
 *
 */

/*
 * This macro encodes the interrupt reason into a one byte
 * event code which is used to index the FSM tables
 */
#define ATAPI_EVENT(drq,intr)	\
	((((drq) & ATS_DRQ) >> 3) | (((intr) & (ATI_IO | ATI_COD)) << 1))

/*
 * These are the names for the encoded ATAPI events
 */
#define	ATAPI_EVENT_0		0
#define	ATAPI_EVENT_IDLE	ATAPI_EVENT(0, ATI_COD)
#define	ATAPI_EVENT_2		2
#define	ATAPI_EVENT_STATUS	ATAPI_EVENT(0, ATI_IO | ATI_COD)
#define	ATAPI_EVENT_PIO_OUT	ATAPI_EVENT(ATS_DRQ, 0)
#define	ATAPI_EVENT_CDB		ATAPI_EVENT(ATS_DRQ, ATI_COD)
#define	ATAPI_EVENT_PIO_IN	ATAPI_EVENT(ATS_DRQ, ATI_IO)
#define	ATAPI_EVENT_UNKNOWN	ATAPI_EVENT(ATS_DRQ, (ATI_IO | ATI_COD))

#define ATAPI_NEVENTS		8

/*
 * Actions for the ATAPI PIO FSM
 *
 */

#define	A_UNK	0	/* invalid event detected */
#define	A_NADA	1	/* do nothing */
#define	A_CDB	2	/* send the CDB */
#define	A_IN	3	/* transfer data out to the device */
#define	A_OUT	4	/* transfer data in from the device */
#define	A_RE	5	/* read the error code register */
#define	A_REX	6	/* alternate read the error code register */
#define	A_IDLE	7	/* unexpected idle phase */

/*
 * States for the ATAPI PIO FSM
 */

#define	S_IDLE	0		/* idle or fatal error state */
#define	S_X	0
#define	S_CMD	1		/* command byte sent */
#define	S_CDB	2		/* CDB sent */
#define	S_IN	3		/* transferring data in from device */
#define	S_OUT	4		/* transferring data out to device */

#define	ATAPI_NSTATES		5


#ifdef	__cplusplus
}
#endif

#endif /* _ATAPI_FSM_H */
