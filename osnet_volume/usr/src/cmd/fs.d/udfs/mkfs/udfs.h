/*
 * Copyright (c) 1989 by Sun Microsystem, Inc.
 */

#ifndef	_UDFS_H
#define	_UDFS_H

#pragma ident	"@(#)udfs.h	1.2	99/02/13 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Tag structure errors
 */
#define	TAGERR_CKSUM	1	/* Invalid checksum on tag */
#define	TAGERR_ID	2	/* Unknown tag id */
#define	TAGERR_VERSION	3	/* Version > ecma_version */
#define	TAGERR_TOOBIG	4	/* CRC length is too large */
#define	TAGERR_CRC	5	/* Bad CRC */
#define	TAGERR_LOC	6	/* Location does not match tag location */

#ifdef	__cplusplus
}
#endif

#endif	/* _UDFS_H */
