/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * X/Open OSI-Abstract-Data-Manipulation API (XOM)
 */

#ifndef	_XOM_H
#define	_XOM_H

#pragma ident	"@(#)xom.h	1.2	97/11/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* BEGIN SERVICE INTERFACE */

/* INTERMEDIATE DATA TYPES */

typedef int		OM_sint;
typedef int		OM_sint16;
typedef int		OM_sint32;
typedef unsigned	OM_uint;
typedef unsigned	OM_uint16;
typedef unsigned	OM_uint32;

/* PRIMARY DATA TYPES */

/* Boolean */
typedef OM_uint32	OM_boolean;

/* String Length */
typedef OM_uint32	OM_string_length;

/* Enumeration */
typedef OM_sint32	OM_enumeration;

/* Exclusions */
typedef OM_uint		OM_exclusions;

/* Integer */
typedef OM_sint32	OM_integer;

/* Modification */
typedef OM_uint		OM_modification;

/* Object */
typedef struct OM_descriptor_struct	*OM_object;

/* String */
typedef struct {
	OM_string_length	length;
	void			*elements;
} OM_string;

#define	OM_STRING(string)  {(OM_string_length)(sizeof (string)-1), (string)}

/* Workspace */
typedef void		*OM_workspace;

/* SECONDARY DATA TYPES */

/* Object Identifier */
typedef OM_string	OM_object_identifier;

/* Private Object */
typedef OM_object	OM_private_object;

/* Public Object */
typedef OM_object	OM_public_object;

/* Return Code */
typedef OM_uint		OM_return_code;

/* Syntax */
typedef OM_uint16	OM_syntax;

/* Type */
typedef OM_uint16	OM_type;

/* Type List */
typedef OM_type		*OM_type_list;

/* Value */
typedef struct {
	OM_uint32	padding;
	OM_object	object;

} OM_padded_object;

typedef union OM_value_union {
	OM_string		string;
	OM_boolean		boolean;
	OM_enumeration		enumeration;
	OM_integer		integer;
	OM_padded_object	object;
} OM_value;

/* Value Length */
typedef OM_uint32	OM_value_length;

/* Value Position */
typedef OM_uint32	OM_value_position;


/* TERTIARY DATA TYPES */

/* Descriptor */
typedef struct OM_descriptor_struct {
	OM_type			type;
	OM_syntax		syntax;
	union OM_value_union	value;
} OM_descriptor;


/* SYMBOLIC CONSTANTS */

/* Boolean */
#define	OM_FALSE			((OM_boolean)0)
#define	OM_TRUE				((OM_boolean)1)

/* Element Position */
#define	OM_LENGTH_UNSPECIFIED		((OM_string_length)0xFFFFFFFF)

/* Exclusions */
#define	OM_NO_EXCLUSIONS		((OM_exclusions)0)
#define	OM_EXCLUDE_ALL_BUT_THESE_TYPES	((OM_exclusions)1)
#define	OM_EXCLUDE_ALL_BUT_THESE_VALUES	((OM_exclusions)2)
#define	OM_EXCLUDE_MULTIPLES		((OM_exclusions)4)
#define	OM_EXCLUDE_SUBOBJECTS		((OM_exclusions)8)
#define	OM_EXCLUDE_VALUES		((OM_exclusions)16)
#define	OM_EXCLUDE_DESCRIPTORS		((OM_exclusions)32)

/* Modification */
#define	OM_INSERT_AT_BEGINNING		((OM_modification)1)
#define	OM_INSERT_AT_CERTAIN_POINT	((OM_modification)2)
#define	OM_INSERT_AT_END		((OM_modification)3)
#define	OM_REPLACE_ALL			((OM_modification)4)
#define	OM_REPLACE_CERTAIN_VALUES	((OM_modification)5)

/* Object Identifiers */

/*
 * NOTE: These macros rely on the ## token-pasting operator of ANSI C.
 * On many pre-ansi compilers the same effect can be obtained by
 * replacing ## with slash asterisk asterisk slash.
 */

/* Private macro to calculate length of an object identifier.  */
#define	OMP_LENGTH(oid_string)	(sizeof (OMP_O_##oid_string)-1)

/* Macro to initialise the syntax and value of an object identifier. */
#define	OM_OID_DESC(type, oid_name) \
	{(type), OM_S_OBJECT_IDENTIFIER_STRING, \
		{{OMP_LENGTH(oid_name), OMP_D_##oid_name}}}

/* Macro to mark the end of a public object.  */
#define	OM_NULL_DESCRIPTOR \
	{OM_NO_MORE_TYPES, OM_S_NO_MORE_SYNTAXES, \
		{0, OM_ELEMENTS_UNSPECIFIED}}

/* Macro to make class constants available within a compilation unit */
#define	OM_IMPORT(class_name) \
	extern char OMP_D_##class_name[]; \
	extern OM_string class_name;

/* Macro to allocate memory for class constants within a compilation unit */
#define	OM_EXPORT(class_name) \
	char OMP_D_##class_name[] = OMP_O_##class_name; \
	OM_string class_name = \
		{OMP_LENGTH(class_name), OMP_D_##class_name};

/* Constant for the OM package */
#define	OMP_O_OM_OM			"\x56\x06\x01\x02\x04"

/* Constant for the Encoding class */
#define	OMP_O_OM_C_ENCODING		"\x56\x06\x01\x02\x04\x01"

/* Constant for the External class */
#define	OMP_O_OM_C_EXTERNAL		"\x56\x06\x01\x02\x04\x02"

/* Constant for the Object class */
#define	OMP_O_OM_C_OBJECT		"\x56\x06\x01\x02\x04\x03"

/* Constant for the BER Object Identifier */
#define	OMP_O_OM_BER			"\x51\x01"

/* Constant for the Canonical-BER Object Identifier */
#define	OMP_O_OM_CANONICAL_BER		"\x56\x06\x01\x02\x04\x04"

/* Return code */
#define	OM_SUCCESS			((OM_return_code)0)
#define	OM_ENCODING_INVALID		((OM_return_code)1)
#define	OM_FUNCTION_DECLINED		((OM_return_code)2)
#define	OM_FUNCTION_INTERRUPTED		((OM_return_code)3)
#define	OM_MEMORY_INSUFFICIENT		((OM_return_code)4)
#define	OM_NETWORK_ERROR		((OM_return_code)5)
#define	OM_NO_SUCH_CLASS		((OM_return_code)6)
#define	OM_NO_SUCH_EXCLUSION		((OM_return_code)7)
#define	OM_NO_SUCH_MODIFICATION		((OM_return_code)8)
#define	OM_NO_SUCH_OBJECT		((OM_return_code)9)
#define	OM_NO_SUCH_RULES		((OM_return_code)10)
#define	OM_NO_SUCH_SYNTAX		((OM_return_code)11)
#define	OM_NO_SUCH_TYPE			((OM_return_code)12)
#define	OM_NO_SUCH_WORKSPACE		((OM_return_code)13)
#define	OM_NOT_AN_ENCODING		((OM_return_code)14)
#define	OM_NOT_CONCRETE			((OM_return_code)15)
#define	OM_NOT_PRESENT			((OM_return_code)16)
#define	OM_NOT_PRIVATE			((OM_return_code)17)
#define	OM_NOT_THE_SERVICES		((OM_return_code)18)
#define	OM_PERMANENT_ERROR		((OM_return_code)19)
#define	OM_POINTER_INVALID		((OM_return_code)20)
#define	OM_SYSTEM_ERROR			((OM_return_code)21)
#define	OM_TEMPORARY_ERROR		((OM_return_code)22)
#define	OM_TOO_MANY_VALUES		((OM_return_code)23)
#define	OM_VALUES_NOT_ADJACENT		((OM_return_code)24)
#define	OM_WRONG_VALUE_LENGTH		((OM_return_code)25)
#define	OM_WRONG_VALUE_MAKEUP		((OM_return_code)26)
#define	OM_WRONG_VALUE_NUMBER		((OM_return_code)27)
#define	OM_WRONG_VALUE_POSITION		((OM_return_code)28)
#define	OM_WRONG_VALUE_SYNTAX		((OM_return_code)29)
#define	OM_WRONG_VALUE_TYPE		((OM_return_code)30)

/* String (Elements component) */
#define	OM_ELEMENTS_UNSPECIFIED		((void *)0)

/* Syntax */
#define	OM_S_NO_MORE_SYNTAXES		((OM_syntax)0)
#define	OM_S_BIT_STRING			((OM_syntax)3)
#define	OM_S_BOOLEAN			((OM_syntax)1)
#define	OM_S_ENCODING			((OM_syntax)8)
#define	OM_S_ENCODING_STRING		((OM_syntax)8)
#define	OM_S_ENUMERATION		((OM_syntax)10)
#define	OM_S_GENERAL_STRING		((OM_syntax)27)
#define	OM_S_GENERALISED_TIME_STRING	((OM_syntax)24)
#define	OM_S_GRAPHIC_STRING		((OM_syntax)25)
#define	OM_S_IA5_STRING			((OM_syntax)22)
#define	OM_S_INTEGER			((OM_syntax)2)
#define	OM_S_NULL			((OM_syntax)5)
#define	OM_S_NUMERIC_STRING		((OM_syntax)18)
#define	OM_S_OBJECT			((OM_syntax)127)
#define	OM_S_OBJECT_DESCRIPTOR_STRING	((OM_syntax)7)
#define	OM_S_OBJECT_IDENTIFIER_STRING	((OM_syntax)6)
#define	OM_S_OCTET_STRING		((OM_syntax)4)
#define	OM_S_PRINTABLE_STRING		((OM_syntax)19)
#define	OM_S_TELETEX_STRING		((OM_syntax)20)
#define	OM_S_UTC_TIME_STRING		((OM_syntax)23)
#define	OM_S_VIDEOTEX_STRING		((OM_syntax)21)
#define	OM_S_VISIBLE_STRING		((OM_syntax)26)

#define	OM_S_LONG_STRING		((OM_syntax)0x8000)
#define	OM_S_NO_VALUE			((OM_syntax)0x4000)
#define	OM_S_LOCAL_STRING		((OM_syntax)0x2000)
#define	OM_S_SERVICE_GENERATED		((OM_syntax)0x1000)
#define	OM_S_PRIVATE			((OM_syntax)0x0800)
#define	OM_S_SYNTAX			((OM_syntax)0x03FF)

/* Type */
#define	OM_NO_MORE_TYPES		((OM_type)0)
#define	OM_ARBITRARY_ENCODING		((OM_type)1)
#define	OM_ASN1_ENCODING		((OM_type)2)
#define	OM_CLASS			((OM_type)3)
#define	OM_DATA_VALUE_DESCRIPTOR	((OM_type)4)
#define	OM_DIRECT_REFERENCE		((OM_type)5)
#define	OM_INDIRECT_REFERENCE		((OM_type)6)
#define	OM_OBJECT_CLASS			((OM_type)7)
#define	OM_OBJECT_ENCODING		((OM_type)8)
#define	OM_OCTET_ALIGNED_ENCODING	((OM_type)9)
#define	OM_PRIVATE_OBJECT		((OM_type)10)
#define	OM_RULES			((OM_type)11)

/* Value Position */
#define	OM_ALL_VALUES			((OM_value_position)0xFFFFFFFF)

/* WORKSPACE INTERFACE */

#include <xomi.h>

/* END SERVICE INTERFACE */

#ifdef	__cplusplus
}
#endif

#endif	/* _XOM_H */
