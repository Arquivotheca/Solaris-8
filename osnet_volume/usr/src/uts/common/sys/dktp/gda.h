/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_DKTP_GDA_H
#define	_SYS_DKTP_GDA_H

#pragma ident	"@(#)gda.h	1.7	99/03/12 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	GDA_RTYCNT	3

#define	GDA_BP_PKT(bp)	((struct cmpkt *)(bp)->av_back)

#ifdef  _KERNEL

#ifdef  __STDC__
extern void 	gda_inqfill(char *p, int l, char *s);
extern void	gda_log(dev_info_t *, char *, uint_t, const char *, ...);
extern void	gda_errmsg(struct scsi_device *, struct cmpkt *, char *,
			int, int, int, char **, char **);
extern struct 	cmpkt *gda_pktprep(opaque_t objp, struct cmpkt *in_pktp,
			opaque_t dmatoken, int (*func)(), caddr_t arg);
extern void	gda_free(opaque_t objp, struct cmpkt *pktp, struct buf *bp);

#else   /* __STDC__ */

extern void 	gda_inqfill();
extern void 	gda_log();
extern void	gda_errmsg();
extern struct 	cmpkt *gda_pktprep();
extern void	gda_free();

#endif  /* __STDC__ */

#endif  /* _KERNEL */

#define	GDA_GETGEOM_HEAD(X) (((X) >> 16) & 0xff)
#define	GDA_GETGEOM_SEC(X)  ((X) & 0xff)
#define	GDA_SETGEOM(hd, sec) (((hd) << 16) | (sec))

#define	GDA_KMFLAG(callback) (((callback) == DDI_DMA_SLEEP) ? \
				KM_SLEEP: KM_NOSLEEP)

#define	GDA_ALL			0
#define	GDA_UNKNOWN		1
#define	GDA_INFORMATIONAL	2
#define	GDA_RECOVERED		3
#define	GDA_RETRYABLE		4
#define	GDA_FATAL		5

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_GDA_H */
