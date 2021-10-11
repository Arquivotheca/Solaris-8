/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _DLCOMMON_H
#define	_DLCOMMON_H

#pragma ident	"@(#)dlcommon.h	1.1	98/02/07 SMI"

/*
 * Common (shared) DLPI test routines.
 */

#include <sys/types.h>
#include <sys/stropts.h>
#include <sys/dlpi.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern void dlinforeq(int fd);
extern void dlinfoack(int fd, char *bufp);
extern void dlattachreq(int fd, ulong_t ppa);
extern void dlenabmultireq(int fd, char *addr, int length);
extern void dldisabmultireq(int fd, char *addr, int length);
extern void dlpromisconreq(int fd, ulong_t level);
extern void dlpromiscoff(int fd, ulong_t level);
extern void dlphysaddrreq(int fd, ulong_t addrtype);
extern void dlsetphysaddrreq(int fd, char *addr, int length);
extern void dldetachreq(int fd);
extern void dlbindreq(int fd, ulong_t sap, ulong_t max_conind,
    ulong_t service_mode, ulong_t conn_mgmt, ulong_t xidtest);
extern void dlunitdatareq(int fd, uchar_t *addrp, int addrlen, ulong_t minpri,
    ulong_t maxpri, uchar_t *datap, int datalen);
extern void dlunbindreq(int fd);
extern void dlokack(int fd, char *bufp);
extern void dlerrorack(int fd, char *bufp);
extern void dlbindack(int fd, char *bufp);
extern void dlphysaddrack(int fd, char *bufp);
extern void strgetmsg(int fd, struct strbuf *ctlp, struct strbuf *datap,
    int *flagsp, char *caller);
extern void expecting(int prim, union DL_primitives *dlp);
extern void printdlprim(union DL_primitives *dlp);
extern void printdlinforeq(union DL_primitives *dlp);
extern void printdlinfoack(union DL_primitives *dlp);
extern void printdlattachreq(union DL_primitives *dlp);
extern void printdlokack(union DL_primitives *dlp);
extern void printdlerrorack(union DL_primitives *dlp);
extern void printdlenabmultireq(union DL_primitives *dlp);
extern void printdldisabmultireq(union DL_primitives *dlp);
extern void printdlpromisconreq(union DL_primitives *dlp);
extern void printdlpromiscoffreq(union DL_primitives *dlp);
extern void printdlphysaddrreq(union DL_primitives *dlp);
extern void printdlphysaddrack(union DL_primitives *dlp);
extern void printdlsetphysaddrreq(union DL_primitives *dlp);
extern void printdldetachreq(union DL_primitives *dlp);
extern void printdlbindreq(union DL_primitives *dlp);
extern void printdlbindack(union DL_primitives *dlp);
extern void printdlunbindreq(union DL_primitives *dlp);
extern void printdlsubsbindreq(union DL_primitives *dlp);
extern void printdlsubsbindack(union DL_primitives *dlp);
extern void printdlsubsunbindreq(union DL_primitives *dlp);
extern void printdlunitdatareq(union DL_primitives *dlp);
extern void printdlunitdataind(union DL_primitives *dlp);
extern void printdluderrorind(union DL_primitives *dlp);
extern void printdltestreq(union DL_primitives *dlp);
extern void printdltestind(union DL_primitives *dlp);
extern void printdltestres(union DL_primitives *dlp);
extern void printdltestcon(union DL_primitives *dlp);
extern void printdlxidreq(union DL_primitives *dlp);
extern void printdlxidind(union DL_primitives *dlp);
extern void printdlxidres(union DL_primitives *dlp);
extern void printdlxidcon(union DL_primitives *dlp);
extern void printdludqosreq(union DL_primitives *dlp);
extern void addrtostring(uchar_t *addr, ulong_t length, uchar_t *s);
extern int stringtoaddr(char *sp, char *addr);
extern char *dlprim(ulong_t prim);
extern char *dlstate(ulong_t state);
extern char *dlerrno(ulong_t errno);
extern char *dlpromisclevel(ulong_t level);
extern char *dlservicemode(ulong_t servicemode);
extern char *dlstyle(long style);
extern char *dlmactype(ulong_t media);
extern int strioctl(int fd, int cmd, int timout, int len, char *dp);

#ifdef	__cplusplus
}
#endif

#endif /* _DLCOMMON_H */
