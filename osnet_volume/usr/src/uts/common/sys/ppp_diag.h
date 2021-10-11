/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#ifndef	_PPP_DIAG_H
#define	_PPP_DIAG_H

#pragma ident	"@(#)ppp_diag.h	1.5	94/02/16 SMI"


#ifdef __cplusplus
extern "C" {
#endif

#define	PPP_DG_MOD_ID 2001

enum ppp_diag_ioctls {
	PPP_DIAG_GET_CONF = 0x300,
	PPP_DIAG_SET_CONF
};

typedef struct {
	int media_type;
	int outputdest;
	int tracelevel;
	int debuglevel;
	int ifid;
} ppp_diag_conf_t;

enum ppp_diag_outdest {
	PPP_DG_STRLOG_DEST
};


typedef struct {
	int mid;
	int sid;
	int tracelevel;
	int flags;
} strlog_struct_t;

#define	PPP_DG_MAX_OUTPUT 1000

#define	PPP_DG_ERR 0x1	/* Error msgs about bad frames, ie. FCS */
#define	PPP_DG_INF 0x2	/* Information for each frame */
#define	PPP_DG_OPT 0x4	/* Expand options for LCP, NCP */
#define	PPP_DG_EXT 0x8	/* Report header compression, NB */
#define	PPP_DG_RAW 0x10 /* Print the raw data */
#define	PPP_DG_NDA 0x20 /* Print hex dump of IP data */
#define	PPP_DG_RFR 0x40 /* Print raw frame */
#define	PPP_DG_ERRREP 0x80	/* Print extended error info */

#define	PPP_DG_STAND (PPP_DG_ERR | PPP_DG_INF | PPP_DG_OPT)
#define	PPP_DG_MIN (PPP_DG_ERR | PPP_DG_INF)
#define	PPP_DG_ALL (PPP_DG_ERR | PPP_DG_INF | PPP_DG_OPT | \
	PPP_DG_EXT | PPP_DG_NDA | PPP_DG_ERRREP)

#define	DEFAULT_MEDIA pppAsync
#define	DEFAULT_OUT PPP_DG_STRLOG_DEST
#define	DEFAULT_TRACE 10
#define	DEFAULT_DEBUGLEVEL PPP_DG_STAND

#ifdef __cplusplus
}
#endif

#endif	/* _PPP_DIAG_H */
