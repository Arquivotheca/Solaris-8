/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * All the information we need to know about a type specifier.
 */

#ifndef	_STABSPF_IMPL_H
#define	_STABSPF_IMPL_H

#pragma ident	"@(#)stabspf_impl.h	1.1	99/05/14 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <apptrace.h>

/* Type code that describes the kind of type the type node is. */
typedef uint16_t tcode_t;

/* Bit flags for type code. */
#define	TS_UNDEF	0x0000	/* Type is undefined of blank. */
#define	TS_BASIC	0x0001	/* C integral types, floats and void. */
#define	TS_FLOAT	0x0002	/* IEEE 32, 64 and 128 bit. */
#define	TS_ARRAY	0x0004	/* Any array ALWAYS has a range. */
#define	TS_ENUM		0x0008  /* Head of enum list. */
#define	TS_STRUCT	0x0010	/* Head of struct list. */
#define	TS_UNION	0x0020	/* Head of union list. */
#define	TS_MEMBER	0x0040	/* elements of enum, struct or union. */
#define	TS_FUNC		0x0080	/* The type that a function returns ONLY. */
#define	TS_FORWARD	0x0100	/* Forward Reference. */
#define	TS_POINTER	0x0200
#define	TS_TYPEDEF	0x0400	/* Straight typdef or alias. */
#define	TS_VOID		0x0800	/* Void. */
#define	TS_CONST	0x1000	/* Const. */
#define	TS_VOLATILE	0x2000	/* Volatile. */
#define	TS_RESTRICTED	0x4000	/* Restricted. */
#define	TS_EMITTED	0x8000	/* Emitted stab. */

/* These are the bits that are set from a type_copy() operation. */
#define	TS_COPY	(TS_TYPEDEF | TS_FUNC | TS_CONST | TS_VOLATILE | TS_RESTRICTED)

/* A string type so we can use portions of existing strings. */
typedef struct misc_strings {
	char	*ms_str;	/* Pointer to the string. */
	size_t	ms_len;		/* Length of current string. */
	size_t	ms_size;	/* Memory allocated for current string. */
} namestr_t;

/* Offset into our string table. */
typedef uint_t stroffset_t;
#define	SO_NOSTRING 0	/* No string may have an offset of zero */

/* A keypair is the ordered pair in '.stabs'. */
typedef struct keypair {
	int kp_file;	/* File key (0 = .c file, the rest are #includes). */
	int kp_type;	/* Type key (ALWAYS > 0). */
} keypair_t;
#define	KP_EMPTY -1	/* Used for pointer to forward reference. */

/* Type descriptor, indexes into the type table (ALWAYS > 0) */
typedef uint_t typedesc_t;
#define	TD_NOTYPE 0	/* Causes a new type_t to be created. */


typedef struct array_type {
	typedesc_t at_td;	/* Type descriptor for the type of array. */
	uint_t	at_range;	/* Max range of the array. */
} atype_t;

/* A basic type is typically an integral type. */
typedef struct basic_type {
	uint8_t	bt_width;	/* Number of bytes for type container. */
	uint8_t	bt_offset;	/* Bit offset within the container. */
	uint8_t	bt_nbits;	/* Bit width of type */
	char	bt_sign;	/* 's' = Signed, 'u' = Unsigned */
	char	bt_display;	/* 'c' = char, 'b' = boolean, 'v' = varargs */
} btype_t;

/* IEEE Floating point types. */
typedef struct float_type {
	uint8_t	ft_format;	/* single, double or long double */
	uint8_t	ft_nbytes;	/* Number of bytes for type container. */
} ftype_t;

/*
 * Forward referenes ALWAYS have a name, but parser may not have been
 * able to discover the type descriptor.
 * The name will exist and can be looked up in the hash.
 */
typedef struct forward_reference_type {
	stroffset_t	xt_name;	/* Name of type refered to. */
	typedesc_t	xt_td;		/* Type descriptor if known. */
} xtype_t;

typedef struct enum_type {
	int		et_number;	/* Value of an enum member. */
	typedesc_t	et_next;	/* Type descriptor of next member. */
} etype_t;

typedef struct struct_union_type {
	uint_t		st_size;	/* Total size of struct or union. */
	typedesc_t	st_first;	/* Type descriptor of next member. */
} stype_t;

/*
 * Member types represent the members of a struct, union, or enum as
 * well as the struct, union, enum itself.
 *
 * Member type_t's have the TS_MEMBER bit set.
 */
typedef struct member_type {
	/* Bit offset of member from begining of struct or union. */
	uint_t		mt_bitoffset;
	/* Bit width of member of struct or union. */
	uint_t		mt_bitsize;
	/* Type descriptor for the type of the member of a struct or union. */
	typedesc_t	mt_td;
	typedesc_t	mt_next;	/* Type descriptor of next member. */
} mtype_t;


typedef struct pointer_type {
	typedesc_t	pt_td;	/* Type descriptor of the pointer. */
} ptype_t;

/*
 * The real Type Object.
 */
typedef struct type_object {
	tcode_t		t_code;		/* Bits describe the type. */
	typedesc_t	t_alias;	/* Type is an alias to this type. */
	stroffset_t	t_name;		/* Actual name of the type. */
	/* Type Specific information */
	union type_info {
		atype_t	ti_at;		/* Array */
		btype_t	ti_bt;		/* Basic */
		ftype_t	ti_ft;		/* Float */
		xtype_t ti_xt;		/* Forward Reference */
		ptype_t ti_pt;		/* Pointer */
		etype_t	ti_et;		/* Enum Type */
		mtype_t	ti_mt;		/* Member Type */
		stype_t ti_st;		/* Struct/Union Type */
	} t_tinfo;
} type_t;

/* Node in the chained hash table. */
typedef struct hnode {
	/* Type descriptor for the string.  May be zero for members. */
	typedesc_t	hn_td;
	/* Hash string offset into string table. */
	stroffset_t	hn_stroffset;
	struct hnode	*hn_next;
} hnode_t;

/* Hash lookup action */
typedef enum hash_action {
	HASH_FIND,	/* Find element, FAIL if not found. */
	HASH_ENTER	/* Find element, INSERT if not found. */
} haction_t;

/*
 * Library Private Interfaces
 */
/* Keypair table opertations. */
extern stabsret_t keypair_create_table(void);
extern stabsret_t keypair_lookup_type(keypair_t *,  typedesc_t *, namestr_t *);
extern stabsret_t keypair_flush_table(void);
extern stabsret_t keypair_destroy_table(void);

/* Type table operations. */
extern stabsret_t ttable_create_table(void);
extern stabsret_t ttable_get_type(typedesc_t *, namestr_t *);
extern stabsret_t ttable_td2ptr(typedesc_t td, type_t **type);
extern void ttable_destroy_table(void);

/* String table operations. */
extern stabsret_t stringt_create_table(void);
extern stabsret_t stringt_new_str(const char *, size_t, stroffset_t *);
extern stabsret_t string_offset2ptr(stroffset_t, char **);
extern void stringt_destroy_table(void);

/* Hash table operations. */
extern stabsret_t hash_create_table(uint_t);
extern stabsret_t hash_get_name(const char *, hnode_t **, haction_t);
extern void hash_destroy_table(void);

/* Type printing functions. */
extern int print_basic(FILE *, type_t *, char *, int, uint64_t val);
extern int print_float(FILE *, type_t *, char *, void const *);
extern int print_pointer(FILE *, type_t *, int, char *, void const *);
extern int print_array(FILE *, type_t *, char *, void const *);
extern int print_enum(FILE *, type_t *, char *, int *);
extern int print_struct(FILE *, type_t *, char *, int, void const *);

/* Special printers. */
extern int print_strdump(FILE *, size_t, void const *);
extern int print_hexadump(FILE *, size_t, void const *);
extern int print_smartdump(FILE *, size_t, void const *);

/* Address validator */
extern ssize_t check_addr(void const *, size_t);

/* Individual stab processing (Private). */
extern stabsret_t add_stab(int, char *);

/* Used to dump all known types (Private). */
extern void print_all_known(void);
extern void hash_report(void);
extern void keypair_report(void);
extern void ttable_report(void);
extern void stringt_report(void);
extern void memory_report(void);
extern void dump_stabs(void);

#endif	/* _STABSPF_IMPL_H */
