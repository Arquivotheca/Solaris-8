/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * X/Open Federated Naming (XFN) Package
 */

#ifndef	_XDSXFNP_H
#define	_XDSXFNP_H

#pragma ident	"@(#)xdsxfnp.h	1.1	96/03/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* { iso(1) member-body(2) ansi(840) sun(113536) ... } */

/* X/Open Federated Naming (XFN) package object identifier */
#define	OMP_O_DS_XFN_PKG		"\x2a\x86\x48\x86\xf7\x00\x11"

/* Directory object classes */
#define	OMP_O_DS_O_XFN			"\x2a\x86\x48\x86\xf7\x00\x18"
#define	OMP_O_DS_O_XFN_SUPPLEMENT	"\x2a\x86\x48\x86\xf7\x00\x19"

/* Directory attribute types */
#define	OMP_O_DS_A_OBJECT_REF_IDENT	"\x2a\x86\x48\x86\xf7\x00\x1a"
#define	OMP_O_DS_A_OBJECT_REF_ADDRESSES	"\x2a\x86\x48\x86\xf7\x00\x1b"
#define	OMP_O_DS_A_NNS_REF_IDENT	"\x2a\x86\x48\x86\xf7\x00\x1c"
#define	OMP_O_DS_A_NNS_REF_ADDRESSES	"\x2a\x86\x48\x86\xf7\x00\x1d"
#define	OMP_O_DS_A_OBJECT_REF_STRING	"\x2a\x86\x48\x86\xf7\x00\x1e"
#define	OMP_O_DS_A_NNS_REF_STRING	"\x2a\x86\x48\x86\xf7\x00\x1f"

/* OM class names */
#define	OMP_O_DS_C_REF_IDENT		"\x2a\x86\x48\x86\xf7\x00\x11\xce\x11"
#define	OMP_O_DS_C_REF_ADDRESSES	"\x2a\x86\x48\x86\xf7\x00\x11\xce\x12"

/* OM attribute names */
#define	DS_OBJECT_IDENT			((OM_type)10001)
#define	DS_UU_IDENT			((OM_type)10002)
#define	DS_STRING_IDENT			((OM_type)10003)
#define	DS_ADDRESS_IDENT		((OM_type)10004)
#define	DS_ADDRESS_VALUE		((OM_type)10005)

#ifdef	__cplusplus
}
#endif

#endif	/* _XDSXFNP_H */
