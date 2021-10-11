/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * X/Open OSI-Abstract-Data-Manipulation API (XOM)
 * Workspace Interface
 */

#ifndef	_XOMI_H
#define	_XOMI_H

#pragma ident	"@(#)xomi.h	1.1	96/03/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	__cplusplus
#define	class clasS	/* 'class' is reserved in C++ */
#endif

/* TYPES AND MACROS */

/* Standard Internal Representation of an Object */

typedef OM_descriptor		OMP_object_header[2];
typedef OMP_object_header	*OMP_object;

/* Standard Internal Representation of a Workspace */

typedef OM_return_code
(*OMP_copy) (
	OM_private_object	original,
	OM_workspace		workspace,
	OM_private_object	*copy
);

typedef OM_return_code
(*OMP_copy_value) (
	OM_private_object	source,
	OM_type			source_type,
	OM_value_position	source_value_position,
	OM_private_object	destination,
	OM_type			destination_type,
	OM_value_position	destination_value_position
);

typedef OM_return_code
(*OMP_create) (
	OM_object_identifier	class,
	OM_boolean		initialise,
	OM_workspace		workspace,
	OM_private_object	*object
);

typedef OM_return_code
(*OMP_decode) (
	OM_private_object	encoding,
	OM_private_object	*original
);

typedef OM_return_code
(*OMP_delete) (
	OM_object		subject
);

typedef OM_return_code
(*OMP_encode) (
	OM_private_object	original,
	OM_object_identifier	rules,
	OM_private_object	*encoding
);

typedef OM_return_code
(*OMP_get) (
	OM_private_object	original,
	OM_exclusions		exclusions,
	OM_type_list		included_types,
	OM_boolean		local_strings,
	OM_value_position	initial_value,
	OM_value_position	limiting_value,
	OM_public_object	*copy,
	OM_value_position	*total_number
);

typedef OM_return_code
(*OMP_instance) (
	OM_object		subject,
	OM_object_identifier	class,
	OM_boolean		*instance
);

typedef OM_return_code
(*OMP_put) (
	OM_private_object	destination,
	OM_modification		modification,
	OM_object		source,
	OM_type_list		included_types,
	OM_value_position	initial_value,
	OM_value_position	limiting_value
);

typedef OM_return_code
(*OMP_read) (
	OM_private_object	subject,
	OM_type			type,
	OM_value_position	value_position,
	OM_boolean		local_string,
	OM_string_length   	*string_offset,
	OM_string		*elements
);

typedef OM_return_code
(*OMP_remove) (
	OM_private_object	subject,
	OM_type			type,
	OM_value_position	initial_value,
	OM_value_position	limiting_value
);

typedef OM_return_code
(*OMP_write) (
	OM_private_object	subject,
	OM_type			type,
	OM_value_position	value_position,
	OM_syntax		syntax,
	OM_string_length   	*string_offset,
	OM_string		elements
);

typedef struct OMP_functions_body {
	OM_uint32		function_number;
	OMP_copy		omp_copy;
	OMP_copy_value		omp_copy_value;
	OMP_create		omp_create;
	OMP_decode		omp_decode;
	OMP_delete		omp_delete;
	OMP_encode		omp_encode;
	OMP_get			omp_get;
	OMP_instance		omp_instance;
	OMP_put			omp_put;
	OMP_read		omp_read;
	OMP_remove		omp_remove;
	OMP_write		omp_write;
} OMP_functions;

typedef struct OMP_workspace_body {
	struct OMP_functions_body	*functions;
} *OMP_workspace;

/* Useful Macros */

#define	OMP_EXTERNAL(internal) \
	((OM_object)((OM_descriptor *)(internal)+1))

#define	OMP_INTERNAL(external) \
	((OM_object)((OM_descriptor *)(external)-1))

#define	OMP_TYPE(external) \
	(((OM_descriptor *)(external))->type)

#define	OMP_WORKSPACE(external) \
	((OMP_workspace)(OMP_INTERNAL(external)->value.string.elements))

#define	OMP_FUNCTIONS(external) \
	(OMP_WORKSPACE(external)->functions)


/* DISPATCHER MACROS */

#define	om_copy(ORIGINAL, WORKSPACE, COPY) \
	(((OMP_workspace)(WORKSPACE))->functions->omp_copy( \
		(ORIGINAL), (WORKSPACE), (COPY)))

#define	om_copy_value(SOURCE, SOURCE_TYPE, SOURCE_POSITION, \
		DEST, DEST_TYPE, DEST_POSITION) \
	(OMP_FUNCTIONS(DEST)->omp_copy_value( \
		(SOURCE), (SOURCE_TYPE), (SOURCE_POSITION), \
		(DEST), (DEST_TYPE), (DEST_POSITION)))

#define	om_create(CLASS, INITIALISE, WORKSPACE, OBJECT) \
	(((OMP_workspace)(WORKSPACE))->functions->omp_create( \
		(CLASS), (INITIALISE), (WORKSPACE), (OBJECT)))

#define	om_decode(ENCODING, ORIGINAL) \
	(OMP_FUNCTIONS(ENCODING)->omp_decode((ENCODING), (ORIGINAL)))

#define	om_delete(SUBJECT) \
	((SUBJECT) == NULL ? OM_NO_SUCH_OBJECT: \
	(((SUBJECT)->syntax & OM_S_SERVICE_GENERATED)? \
	OMP_FUNCTIONS(SUBJECT)->omp_delete((SUBJECT)): \
	OM_NOT_THE_SERVICES))

#define	om_encode(ORIGINAL, RULES, ENCODING) \
	(OMP_FUNCTIONS(ORIGINAL)->omp_encode((ORIGINAL), (RULES), (ENCODING)))

#define	om_get(ORIGINAL, EXCLUSIONS, TYPES, LOCAL_STRINGS, \
		INITIAL, LIMIT, COPY, TOTAL_NUMBER) \
	(OMP_FUNCTIONS(ORIGINAL)->omp_get( \
		(ORIGINAL), (EXCLUSIONS), (TYPES), (LOCAL_STRINGS), \
		(INITIAL), (LIMIT), (COPY), (TOTAL_NUMBER)))

#define	om_instance(SUBJECT, CLASS, INSTANCE) \
	(((SUBJECT)->syntax & OM_S_SERVICE_GENERATED)? \
	OMP_FUNCTIONS(SUBJECT)->omp_instance((SUBJECT), (CLASS), (INSTANCE)): \
	OM_NOT_THE_SERVICES)

#define	om_put(DESTINATION, MODIFICATION, SOURCE, TYPES, INITIAL, LIMIT) \
	(OMP_FUNCTIONS(DESTINATION)->omp_put( \
		(DESTINATION), (MODIFICATION), (SOURCE), (TYPES), (INITIAL), \
		(LIMIT)))

#define	om_read(SUBJECT, TYPE, VALUE_POS, LOCAL_STRING, STRING_OFFSET, \
	ELEMENTS) \
	(OMP_FUNCTIONS(SUBJECT)->omp_read( \
		(SUBJECT), (TYPE), (VALUE_POS), (LOCAL_STRING), \
		(STRING_OFFSET), (ELEMENTS)))

#define	om_remove(SUBJECT, TYPE, INITIAL, LIMIT) \
	(OMP_FUNCTIONS(SUBJECT)->omp_remove( \
		(SUBJECT), (TYPE), (INITIAL), (LIMIT)))

#define	om_write(SUBJECT, TYPE, VALUE_POS, SYNTAX, STRING_OFFSET, ELEMENTS) \
	(OMP_FUNCTIONS(SUBJECT)->omp_write( \
		(SUBJECT), (TYPE), (VALUE_POS), (SYNTAX), \
		(STRING_OFFSET), (ELEMENTS)))

#ifdef	__cplusplus
#undef	class
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _XOMI_H */
