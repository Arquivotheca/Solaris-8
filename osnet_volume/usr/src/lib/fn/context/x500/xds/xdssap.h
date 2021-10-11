/*
 * Copyright (c) 1994-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * X/Open Strong Authentication Package
 */

#ifndef	_XDSSAP_H
#define	_XDSSAP_H

#pragma ident	"@(#)xdssap.h	1.2	98/11/10 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef XDSSAP_HEADER
#define	XDSSAP_HEADER

/* X/Open Strong Authentation package object identifier */
#define	OMP_O_DS_STRONG_AUTHENT_PKG	"\x2B\x0C\x02\x87\x73\x1C\x02"

/* Intermediate object identifier macros */
#ifndef dsP_attributeType
#define	dsP_attributeType(X)	"\x55\x4" #X	/* joint-iso-ccitt 5 4 */
#endif

#ifndef dsP_objectClass
#define	dsP_objectClass(X)	"\x55\x6" #X	/* joint-iso-ccitt 5 6 */
#endif

#define	dsP_sap_c(X)		OMP_O_DS_STRONG_AUTHENT_PKG #X

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
#define	OMP_O_DS_A_AUTHORITY_REVOC_LIST		dsP_attributeType(\x26)
#define	OMP_O_DS_A_CA_CERT			dsP_attributeType(\x25)
#define	OMP_O_DS_A_CERT_REVOC_LIST		dsP_attributeType(\x27)
#define	OMP_O_DS_A_CROSS_CERT_PAIR		dsP_attributeType(\x28)
#define	OMP_O_DS_A_USER_CERT			dsP_attributeType(\x24)

/* Directory object classes */
#define	OMP_O_DS_O_CERT_AUTHORITY		dsP_objectClass(\x10)
#define	OMP_O_DS_O_STRONG_AUTHENT_USER		dsP_objectClass(\x0F)

/* OM class names */
#define	OMP_O_DS_C_ALGORITHM_IDENT		dsP_sap_c(\x86\x35)
#define	OMP_O_DS_C_CERT				dsP_sap_c(\x86\x36)
#define	OMP_O_DS_C_CERT_LIST			dsP_sap_c(\x86\x37)
#define	OMP_O_DS_C_CERT_PAIR			dsP_sap_c(\x86\x38)
#define	OMP_O_DS_C_CERT_SUBLIST			dsP_sap_c(\x86\x39)
#define	OMP_O_DS_C_SIGNATURE			dsP_sap_c(\x86\x3A)

/* OM attribute names */
#define	DS_ALGORITHM				821
#define	DS_FORWARD				822
#define	DS_ISSUER				823
#define	DS_LAST_UPDATE				824
#define	DS_ALGORITHM_PARAMETERS			825
#define	DS_REVERSE				826
#define	DS_REVOC_DATE				827
#define	DS_REVOKED_CERTS			828
#define	DS_SERIAL_NBR				829
#define	DS_SERIAL_NBRS				830
#define	DS_SIGNATURE				831
#define	DS_SIGNATURE_VALUE			832
#define	DS_SUBJECT				833
#define	DS_SUBJECT_ALGORITHM			834
#define	DS_SUBJECT_PUBLIC_KEY			835
#define	DS_VALIDITY_NOT_AFTER			836
#define	DS_VALIDITY_NOT_BEFORE			837
#define	DS_VERSION				838

/* DS_Version */
#define	DS_V1988				((OM_enumeration)1)

/* upper bounds on string lengths and number of repeated OM attribute values */
#define	DS_VL_LAST_UPDATE			((OM_value_length)17)
#define	DS_VL_REVOC_LIST			((OM_value_length)17)
#define	DS_VL_VALIDITY_NOT_AFTER		((OM_value_length)17)
#define	DS_VL_VALIDITY_NOT_BEFORE		((OM_value_length)17)
#define	DS_VN_REVOC_DATE			((OM_value_number)2)

#endif	/* XDSSAP_HEADER */

#ifdef	__cplusplus
}
#endif

#endif	/* _XDSSAP_H */
