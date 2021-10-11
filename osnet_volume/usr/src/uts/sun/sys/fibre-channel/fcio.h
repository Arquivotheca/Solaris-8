/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FIBRE_CHANNEL_FCIO_H
#define	_SYS_FIBRE_CHANNEL_FCIO_H

#pragma ident	"@(#)fcio.h	1.2	99/09/28 SMI"

#include <sys/note.h>
#include <sys/fibre-channel/fc_types.h>
#include <sys/fibre-channel/fc_appif.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * ioctl definitions
 */
#define	FCTIO				('F'<< 8)

/*
 * New ioctl definitions
 */
#define	FCIO_CMD			(FCTIO | 1998)
#define	FCIO_SUB_CMD			('Z' << 8)
#define	FCIO_GET_NUM_DEVS		(FCIO_SUB_CMD + 0x01)
#define	FCIO_GET_DEV_LIST		(FCIO_SUB_CMD + 0x02)
#define	FCIO_GET_SYM_PNAME		(FCIO_SUB_CMD + 0x03)
#define	FCIO_GET_SYM_NNAME		(FCIO_SUB_CMD + 0x04)
#define	FCIO_SET_SYM_PNAME		(FCIO_SUB_CMD + 0x05)
#define	FCIO_SET_SYM_NNAME		(FCIO_SUB_CMD + 0x06)
#define	FCIO_GET_LOGI_PARAMS		(FCIO_SUB_CMD + 0x07)
#define	FCIO_DEV_LOGIN			(FCIO_SUB_CMD + 0x08)
#define	FCIO_DEV_LOGOUT			(FCIO_SUB_CMD + 0x09)
#define	FCIO_GET_STATE			(FCIO_SUB_CMD + 0x0A)
#define	FCIO_DEV_REMOVE			(FCIO_SUB_CMD + 0x0B)
#define	FCIO_GET_FCODE_REV		(FCIO_SUB_CMD + 0x0C)
#define	FCIO_GET_FW_REV			(FCIO_SUB_CMD + 0x0D)
#define	FCIO_GET_DUMP_SIZE		(FCIO_SUB_CMD + 0x0E)
#define	FCIO_FORCE_DUMP			(FCIO_SUB_CMD + 0x0F)
#define	FCIO_GET_DUMP			(FCIO_SUB_CMD + 0x10)
#define	FCIO_GET_TOPOLOGY		(FCIO_SUB_CMD + 0x11)
#define	FCIO_RESET_LINK			(FCIO_SUB_CMD + 0x12)
#define	FCIO_RESET_HARD			(FCIO_SUB_CMD + 0x13)
#define	FCIO_RESET_HARD_CORE		(FCIO_SUB_CMD + 0x14)
#define	FCIO_DIAG			(FCIO_SUB_CMD + 0x15)
#define	FCIO_NS				(FCIO_SUB_CMD + 0x16)
#define	FCIO_DOWNLOAD_FW		(FCIO_SUB_CMD + 0x17)
#define	FCIO_GET_HOST_PARAMS		(FCIO_SUB_CMD + 0x18)
#define	FCIO_LINK_STATUS		(FCIO_SUB_CMD + 0x19)
#define	FCIO_DOWNLOAD_FCODE		(FCIO_SUB_CMD + 0x1A)


/*
 * Fixed diag_codes for FCIO_DIAG. These is supported by all FCAs.
 * No FCA should define ioctls in this range.
 */
#define	FCIO_DIAG_PORT_DISABLE		(FCIO_SUB_CMD + 0x80)
#define	FCIO_DIAG_PORT_ENABLE		(FCIO_SUB_CMD + 0x81)

/* cmd_flags for FCIO_LINK_STATUS ioctl */
#define	FCIO_CFLAGS_RLS_DEST_NPORT	0x0000
#define	FCIO_CFLAGS_RLS_DEST_FPORT	0x0001

typedef struct fc_port_dev {
	uchar_t		dev_dtype;		/* SCSI device type */
	uint32_t	dev_type[8];		/* protocol specific */
	uint32_t	dev_state;		/* port state */
	fc_portid_t	dev_did;		/* Destination Identifier */
	fc_hardaddr_t	dev_hard_addr;		/* Hard address */
	la_wwn_t	dev_pwwn;		/* port WWN */
	la_wwn_t	dev_nwwn;		/* node WWN */
} fc_port_dev_t;

typedef struct fc_port_dev fc_ns_map_entry_t;

/*
 * fcio_xfer definitions
 */
#define	FCIO_XFER_NONE		0x00
#define	FCIO_XFER_READ		0x01
#define	FCIO_XFER_WRITE		0x02
#define	FCIO_XFER_RW		(FCIO_XFER_READ | FCIO_XFER_WRITE)

typedef struct fcio {
	uint16_t	fcio_xfer;	/* direction */
	uint16_t	fcio_cmd;	/* sub command */
	uint16_t	fcio_flags;	/* flags */
	uint16_t	fcio_cmd_flags;	/* command specific flags */
	size_t		fcio_ilen;	/* Input buffer length */
	caddr_t		fcio_ibuf;	/* Input buffer */
	size_t		fcio_olen;	/* Output buffer length */
	caddr_t		fcio_obuf;	/* Output buffer */
	size_t		fcio_alen;	/* Auxillary buffer length */
	caddr_t		fcio_abuf;	/* Auxillary buffer */
	int		fcio_errno;	/* FC internal error code */
} fcio_t;

#if defined(_SYSCALL32)
/*
 * 32 bit varient of fcio_t; to be used
 * only in the driver and NOT applications
 */
struct fcio32 {
	uint16_t	fcio_xfer;	/* direction */
	uint16_t	fcio_cmd;	/* sub command */
	uint16_t	fcio_flags;	/* flags */
	uint16_t	fcio_cmd_flags;	/* command specific flags */
	size32_t	fcio_ilen;	/* Input buffer length */
	caddr32_t	fcio_ibuf;	/* Input buffer */
	size32_t	fcio_olen;	/* Output buffer length */
	caddr32_t	fcio_obuf;	/* Output buffer */
	size32_t	fcio_alen;	/* Auxillary buffer length */
	caddr32_t	fcio_abuf;	/* Auxillary buffer */
	int		fcio_errno;	/* FC internal error code */
};

#if	!defined(lint)
_NOTE(SCHEME_PROTECTS_DATA("unique per request", fcio32))
#endif /* lint */

#endif /* _SYSCALL32 */

#if	!defined(lint)
_NOTE(SCHEME_PROTECTS_DATA("unique per request", fcio fc_port_dev))
#endif /* lint */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FIBRE_CHANNEL_FCIO_H */
