/*
 * Copyright (c) 1994-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * X/Open MHS Directory User Package
 */

#ifndef	_XDSMDUP_H
#define	_XDSMDUP_H

#pragma ident	"@(#)xdsmdup.h	1.2	98/11/10 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	XDSMDUP_HEADER
#define	XDSMDUP_HEADER

#ifndef	XMHP_HEADER
/* #include <xmhp.h> */
#endif	/* XMHP_HEADER */

/*
 * MDUP package object identifier
 * { iso(1) identifier-organization(3) icd-ecma(12) member-company(2) dec(1011)
 *   xopen(28) mdup(3) }
 */

#define	OMP_O_DS_MHS_DIR_USER_PKG	"\x2B\x0C\x02\x87\x73\x1C\x03"

/* Intermediate object identifier macros */
#define	dsP_MHSattributeType(X)	"\x56\x5\x2" #X /* joint-iso-ccitt 6 5 2 */

#define	dsP_MHSobjectClass(X)	"\x56\x5\x1" #X /* joint-iso-ccitt 6 5 1 */

#define	dsP_mdup_c(X)		OMP_O_DS_MHS_DIR_USER_PKG #X

/* OM class names (prefixed DS_C_),				*/
/* Directory attribute types (prefixed DS_A_),			*/
/* and Directory object classes (prefixed DS_O_)		*/

/* Every application program which makes use of a class or	*/
/* other Object Identifier must explicitly import it into	*/
/* every compilation unit (C source program) which uses it.	*/
/* Each such class or Object Identifier name must be		*/
/* explicitly exported from just one compilation unit.		*/

/* In the header file, OM class constants are prefixed with	*/
/* the OMP_O prefix to denote that they are OM classes.		*/
/* However, when using the OM_IMPORT and OM_EXPORT macros,	*/
/* the base names (without the OMP_O prefix) should be used.	*/
/* For example:							*/
/*		OM_IMPORT(DS_O_COUNTRY)				*/


/* Directory attribute types */
#define	OMP_O_DS_A_DELIV_CONTENT_LENGTH		dsP_MHSattributeType(\x00)
#define	OMP_O_DS_A_DELIV_CONTENT_TYPES		dsP_MHSattributeType(\x01)
#define	OMP_O_DS_A_DELIV_EITS			dsP_MHSattributeType(\x02)
#define	OMP_O_DS_A_DL_MEMBERS			dsP_MHSattributeType(\x03)
#define	OMP_O_DS_A_DL_SUBMIT_PERMS		dsP_MHSattributeType(\x04)
#define	OMP_O_DS_A_MESSAGE_STORE		dsP_MHSattributeType(\x05)
#define	OMP_O_DS_A_OR_ADDRESSES			dsP_MHSattributeType(\x06)
#define	OMP_O_DS_A_PREF_DELIV_METHODS		dsP_MHSattributeType(\x07)
#define	OMP_O_DS_A_SUPP_AUTO_ACTIONS		dsP_MHSattributeType(\x08)
#define	OMP_O_DS_A_SUPP_CONTENT_TYPES		dsP_MHSattributeType(\x09)
#define	OMP_O_DS_A_SUPP_OPT_ATTRIBUTES		dsP_MHSattributeType(\x0A)

/* Directory object classes */
#define	OMP_O_DS_O_MHS_DISTRIBUTION_LIST	dsP_MHSobjectClass(\x00)
#define	OMP_O_DS_O_MHS_MESSAGE_STORE		dsP_MHSobjectClass(\x01)
#define	OMP_O_DS_O_MHS_MESSAGE_TRANS_AG		dsP_MHSobjectClass(\x02)
#define	OMP_O_DS_O_MHS_USER			dsP_MHSobjectClass(\x03)
#define	OMP_O_DS_O_MHS_USER_AG			dsP_MHSobjectClass(\x04)

/* OM class names */
#define	OMP_O_DS_C_DL_SUBMIT_PERMS		dsP_mdup_c(\x87\x05)

/* OM attribute names */
#define	DS_PERM_TYPE		((OM_type)901)
#define	DS_INDIVIDUAL		((OM_type)902)
#define	DS_MEMBER_OF_DL		((OM_type)903)
#define	DS_PATTERN_MATCH	((OM_type)904)
#define	DS_MEMBER_OF_GROUP	((OM_type)905)

/* DS_Permission_Type */
enum DS_Permission_Type {
	DS_PERM_INDIVIDUAL	= 0,
	DS_PERM_MEMBER_OF_DL	= 1,
	DS_PERM_PATTERN_MATCH	= 2,
	DS_PERM_MEMBER_OF_GROUP	= 3
};

#endif  /* XDSMDUP_HEADER */

#ifdef	__cplusplus
}
#endif

#endif	/* _XDSMDUP_H */
