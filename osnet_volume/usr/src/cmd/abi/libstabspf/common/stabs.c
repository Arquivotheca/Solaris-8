/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)stabs.c	1.1	99/05/14 SMI"

#include <stdio.h>

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stab.h>
#include "stabspf_impl.h"

/*
 * BIG STAB WALKING STATEMENT
 * --------------------------
 *
 * "string"-->	= a hashed string
 * "string"<--	= an offset string
 * (A,B)	= a stab keypair
 * [Xtype_t]	= a struct representing the information of that type.
 *
 * The leaf in a type_t list or tree is any type that does not depend on
 * another.  An example is 'char'.
 *
 *	"char"<--->[btype_t]<-----(0,1)
 *	"int"<---->[btype_t]<-----(0,3)
 *
 * Any integral (basic, float or void) type is a leaf.
 *
 * Complex types such as a 'typedef char *string_t;' require two nodes.
 *
 *
 *	"string_t"<->[ptype_t]<----(0,21)
 *			|
 *			v
 *	"char"<----->[btype_t]<-----(0,1)
 *
 * Types with members, such as structs, require even more nodes.
 *	struct foo {
 *		int	foo_int;
 *		char	foo_char;
 *		char	*foo_str;
 *	};
 *
 * Is structured in this way (some keypairs omitted for readability).
 *
 *	"struct foo"<->[mtype_t]<--(0,22)
 *			   |
 *			   |              +---->"foo_char"
 *			   v              |
 *	"foo_int"<-----[mtype_t]------->[mtype_t]-------->[mtype_t]-->"foo_str"
 *			   |		      |             |
 *			   |		      |		    v
 *			   |		      |		[ptype_t]
 *			   |		      |             |
 *			   |		      |  +----------+
 *			   |		      |  |
 *			   |		      v  v
 *			   | 	"char"<--->[btype_t]<-----(0,1)
 *			   v
 *	   "int"<----->[btype_t]<-----(0,3)
 *
 *
 * And on and on....
 *
 * Other notes:
 *  - Variable 'namestr' contains the type name of an
 *    entire stab, once we have to resolve an embedded type we stop
 *    passing it because we do not want the embedded type to be
 *    directly associated with that name.  It is used to handle
 *    multiple compilation units where both 'foo.c' and 'bar.c' have
 *    stabs for 'struct baz'.
 *
 *  - Though we may discover we are dealing with a previously defined
 *    named type, it is usually necessary to continue processing or
 *    at least parse past the type information so we may continue
 *    processing the embedded types that follow.
 *
 *  - Forward references may not have a type descriptor associated with them
 *    but will always have a name we can look up later.
 *
 *  - Struct, enums and unions (ESU) members have the TS_MEMBER bit set
 *    in the type_t, however the root nodes (ie. the actual struct
 *    node) does not have the TS_MEMBER bit set.
 *
 *  - Names of ESU members cannot be referred to by the type descriptor
 *    from a hash lookup. If a type descriptor is defined from a hash lookup
 *    then it is the type descriptor of a real type of that name.  The same
 *    string is used for members in order to keep the elements of the string
 *    table as unique aas possible.
 */


/* Prototype for the work horse. */
static stabsret_t resolve_type(typedesc_t *, char **, namestr_t *);

/*
 * A lot of the parsing is performed by checking single chars and
 * returning STAB_FAIL or skiping it.  This macro eases that test.
 * Unfortunately assert() does not suit our purposes.
 */

#define	SKIPCHAR(cp, c)	\
	if (*(cp) != (c)) {	\
	    return (STAB_FAIL);	\
	}			\
	++(cp)

/* Do not call resolve_range() unless we are processing an array. */
static int processing_array;

/* Index to const array of handy strings. */
typedef enum misc_str_index {
	MSI_NONE = -1,
	MSI_STRUCT,
	MSI_ENUM,
	MSI_UNION
} msi_t;

/* The handy strings. */
#define	STR_STRUCT	"struct "
#define	STR_ENUM	"enum "
#define	STR_UNION	"union "

/*
 * Handy strings.
 * 	Remember: sizeof a string literal includes the '\0'.
 */
static namestr_t misc_str[] = {
	{ STR_STRUCT,	sizeof (STR_STRUCT) - 1,	sizeof (STR_STRUCT)},
	{ STR_ENUM,	sizeof (STR_ENUM) - 1,		sizeof (STR_ENUM)},
	{ STR_UNION,	sizeof (STR_UNION) - 1,		sizeof (STR_UNION)},
};

/* String buffer to expand into. */
static namestr_t exp_name;

/*
 * expand_the_name() - Expands a type name.
 *
 * In the case of struct, enum or union we want the struct typename "foo"
 * to be placed in our string table as "struct foo".
 *
 * NOTE:
 *	If <namelen> is zero then strlen() will be used instead.
 *
 */
static stabsret_t
expand_the_name(namestr_t *e, msi_t index, const char *name,
    size_t namelen)
{
	char *new_str;
	size_t new_size;
	char *insert;

	if (name == NULL) {
		return (STAB_FAIL);
	}

	if (namelen == 0) {
		namelen = strlen(name);
	}

	new_size = misc_str[index].ms_size + namelen;

	/* Make e bigger if needed. */
	if (new_size > e->ms_size) {
		size_t alloc_size;

		if (e->ms_size == 0) {
			alloc_size = MAX_STAB_STR_LEN;
		} else {
			/* Double current size. */
			alloc_size = e->ms_size * 2;
		}

		/* Double again until big enough. */
		while (alloc_size < new_size)
			alloc_size *= 2;


		if ((new_str = realloc(e->ms_str, alloc_size)) == NULL) {
			return (STAB_NOMEM);
		}
		e->ms_str = new_str;
		e->ms_size = alloc_size;
	}
	e->ms_len = new_size - 1;

	/* Append the misc_str. */
	(void) strcpy(e->ms_str, misc_str[index].ms_str);
	insert = e->ms_str + misc_str[index].ms_len;
	(void) strncpy(insert, name, namelen);
	/* Just copied part of a string so termination is necessary. */
	insert[namelen] = '\0';

	return (STAB_SUCCESS);
}

/*
 * get_keypair() - Parse the string <*s> for a keypair.
 *
 * If no keypair exists then DO NOT update <*s>.
 */
static stabsret_t
get_keypair(char **s, keypair_t *kp)
{
	/* If we fail s should not change. */
	char *p = *s;

	if (*p == '(') {
		++p;
		kp->kp_file = (int)strtol(p, &p, 10);

		SKIPCHAR(p, ',');

		kp->kp_type = (int)strtol(p, &p, 10);

		SKIPCHAR(p, ')');

		*s = p;
		return (STAB_SUCCESS);
	}

	return (STAB_FAIL);
}

/*
 * resolve_range() - Assigns the at_range of an array type that belongs to
 *	type descriptor <td>.
 *
 * Parsing:
 *	r Type ; MinValue ; MaxValue
 */
static stabsret_t
resolve_range(typedesc_t *td, char **s)
{
	int maxvalue;
	keypair_t range_kp;
	type_t *type;
	atype_t *at;
	stabsret_t ret;

	SKIPCHAR(*s, 'r');

	/* This will always be of type int, just skip it. */
	if ((ret = get_keypair(s, &range_kp)) != STAB_SUCCESS) {
		return (ret);
	}

	SKIPCHAR(*s, ';');

	/* Get the MinValue that we do not care about but better be zero. */

	if (strtol(*s, s, 10) != 0) {
		return (STAB_FAIL);
	}

	SKIPCHAR(*s, ';');

	/* Get the Max Value. */
	maxvalue = (int)strtol(*s, s, 10);
	if (maxvalue < 0) {
		return (STAB_FAIL);
	}

	/* Get a pointer to the type from the type table. */
	if (ttable_td2ptr(*td, &type) != STAB_SUCCESS) {
		return (STAB_FAIL);
	}

	/* Get the array type. */
	at = &type->t_tinfo.ti_at;

	/* Update the array type information. */
	at->at_range = maxvalue;

	return (STAB_SUCCESS);
}

/*
 * resolve_array() - Create/get an array type and assign <*td> to it.
 *
 * Parsing:
 *	a IndexType ; Type
 */
static stabsret_t
resolve_array(typedesc_t *td, char **s, keypair_t *kp,
    namestr_t *namestr)
{
	stabsret_t ret;
	type_t *type;
	atype_t *at;
	typedesc_t array_td = TD_NOTYPE;

	SKIPCHAR(*s, 'a');

	/* Get a keypair for this type and type descriptor. */
	ret = keypair_lookup_type(kp, td, namestr);
	if (ret != STAB_SUCCESS) {
		return (ret);
	}

	/* Get a pointer to the type from the type descriptor. */
	if ((ret = ttable_td2ptr(*td, &type)) != STAB_SUCCESS) {
			return (ret);
	}

	/*
	 * Get the IndexType.
	 * Hand the given type to resolve_type and it will assign the range.
	 */
	processing_array = 1;
	if ((ret = resolve_type(td, s, NULL)) != STAB_SUCCESS) {
		return (ret);
	}
	processing_array = 0;

	SKIPCHAR(*s, ';');

	/* Get the actual type of the array. */
	if ((ret = resolve_type(&array_td, s, NULL)) != STAB_SUCCESS) {
		return (ret);
	}

	/*
	 * Must get type_t pointer again because the above resolve_type()
	 * may have relocated the type table.
	 */
	if ((ret = ttable_td2ptr(*td, &type)) != STAB_SUCCESS) {
		return (ret);
	}

	/*
	 * If the type information is already assigned then
	 * this is a known _named_ type and we can stop parsing and
	 * exit now.
	 */
	if (type->t_code == TS_ARRAY) {
		return (STAB_SUCCESS);
	}

	/* Get the atype_t. */
	at = &type->t_tinfo.ti_at;

	/* Fill in the info. */
	type->t_code = TS_ARRAY;
	at->at_td = array_td;

	return (STAB_SUCCESS);
}

/*
 * resolve_basic() - Assigns the type_t as a basic type and assigns
 *	the information in the btype_t.
 *
 * Parse:
 *	b Sign [Display] Width ; Offset ; Nbits
 *		Display is optional.
 * NOTE:
 *	Stab interface manual is unclear if the last character is ';' or not!
 *	We have seen both in practice.
 */

static stabsret_t
resolve_basic(typedesc_t *td, char **s, keypair_t *kp, namestr_t *namestr)
{
	int number;
	stabsret_t ret;
	type_t *type;
	btype_t *bt;

	SKIPCHAR(*s, 'b');

	/* Get a keypair reference. */
	ret = keypair_lookup_type(kp, td, namestr);
	if (ret != STAB_SUCCESS) {
		return (ret);
	}

	/* Get type_t pointer. */
	if ((ret = ttable_td2ptr(*td, &type)) != STAB_SUCCESS) {
		return (ret);
	}

	/*
	 * If the type information is already assigned then
	 * this is a known _named_ type and we can stop parsing and
	 * exit now.
	 */
	if (type->t_code == TS_BASIC) {
		return (STAB_SUCCESS);
	}
	/* Get btype_t pointer. */
	bt = &type->t_tinfo.ti_bt;

	/* Assign information. */
	type->t_code = TS_BASIC;

	/* Get Sign. */
	if (**s != 's' && **s != 'u') {
		return (STAB_FAIL);
	}
	bt->bt_sign = **s;

	/* Skip sign. */
	++(*s);

	/* Check if the optional Display is present. */
	if (!isdigit(**s)) {
		/* Get Display. */
		if (**s != 'c' && **s != 'b' && **s != 'v') {
			return (STAB_FAIL);
		}
		type->t_tinfo.ti_bt.bt_display = **s;
		/* Skip Display. */
		++(*s);
	}

	/* Get Width (void has a width of zero). */
	if ((number = (int)strtol(*s, s, 10)) < 0) {
		return (STAB_FAIL);
	}

	SKIPCHAR(*s, ';');

	/* Assign width. */
	bt->bt_width = number;

	/* Get Offset. */
	if ((number = (int)strtol(*s, s, 10)) < 0) {
		return (STAB_FAIL);
	}

	SKIPCHAR(*s, ';');

	bt->bt_offset = number;

	/* Get Nbits (void has a Nbits of zero). */
	number = (int)strtol(*s, s, 10);
	if (number < 0) {
		return (STAB_FAIL);
	}
	bt->bt_nbits = number;

	/*
	 * May or may not contain a trailing ';'.
	 * If it does, then skip it since there will never be anything
	 * after this point.
	 */
	if (**s == ';') {
		++(*s);
	}

	return (STAB_SUCCESS);
}


/*
 * resolve_enum_member() - Create a linked list of enum member name and
 *	value pairs. List elements are type_t's.
 *
 * Parse:
 *	Name : Number ,
 *
 * NOTE:
 *	There is no type processing involved here.
 */
static stabsret_t
resolve_enum_member(typedesc_t *td, char **s)
{
	typedesc_t new_td = TD_NOTYPE;
	hnode_t *hnode = NULL;
	type_t *type;
	etype_t *et;
	char *name = *s;
	char *colon;
	int number;
	stabsret_t ret;

	/* Find the colon marking the end of the name. */
	colon = strchr(name, ':');

	/* There must be a name! */
	if (colon == NULL) {
		return (STAB_FAIL);
	}

	/* Temporarily null terminate the name so we can hash it. */
	*colon = '\0';
	/* Check if string exists. */
	ret = hash_get_name(name, &hnode, HASH_ENTER);
	/* Put colon back. */
	*colon = ':';

	if (ret != STAB_SUCCESS) {
		return (ret);
	}

	/* If there is no string then make one and update the hash node. */
	if (hnode->hn_stroffset == SO_NOSTRING) {
		/* Create a new string with contents. */
		ret = stringt_new_str(name, colon - name,
		    &hnode->hn_stroffset);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
	}

	/* Skip ':'. */
	*s = colon + 1;

	number = (int)strtol(*s, s, 10);

	SKIPCHAR(*s, ',');

	/* Check if we are at the end. */
	if (**s != ';') {
		/* Get a new type. */
		ret = ttable_get_type(&new_td, NULL);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}

		if ((ret = resolve_enum_member(&new_td, s)) != STAB_SUCCESS) {
			return (ret);
		}
	}

	/* Fill in the info. */
	if ((ret = ttable_td2ptr(*td, &type)) != STAB_SUCCESS) {
		return (ret);
	}
	et = &type->t_tinfo.ti_et;

	type->t_code = (TS_ENUM | TS_MEMBER);
	type->t_name = hnode->hn_stroffset;
	et->et_next = new_td;
	et->et_number = number;

	return (STAB_SUCCESS);
}

/*
 * resolve_enum() - Get a type descriptor, name it and call
 *	resolv_enum_member() to do the rest.
 *
 * Parse:
 *	e [Type] { Name : Number , } * ;
 * 		Type is optional and should not exist for C.
 *		May have 0 {Name : Number , } pairs.
 */
static stabsret_t
resolve_enum(typedesc_t *td, char **s, keypair_t *kp,
    namestr_t *namestr)
{
	typedesc_t enum_td = TD_NOTYPE;
	typedesc_t member_td = TD_NOTYPE;
	type_t *type;
	etype_t *et;
	stabsret_t ret;

	SKIPCHAR(*s, 'e');

	/* Is there a type for this enum?  There should not be for C! */
	if (**s == '(' &&
	    (ret = resolve_type(&enum_td, s, NULL)) != STAB_SUCCESS) {
			return (ret);
	}

	/* Enum's get there names expanded, ie. "foo" => "enum foo". */
	if (namestr != NULL && namestr->ms_str != NULL) {
		ret = expand_the_name(&exp_name, MSI_ENUM,
		    namestr->ms_str, namestr->ms_len);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
		/* We may have seen it before.. */
		ret = keypair_lookup_type(kp, td, &exp_name);
	} else {
		/*
		 * No name for this enum,
		 * we cannot find out if we have seen it before.
		 */
		ret = keypair_lookup_type(kp, td, NULL);
	}
	if (ret != STAB_SUCCESS) {
		return (ret);
	}

	if ((ret = ttable_td2ptr(*td, &type)) != STAB_SUCCESS) {
			return (ret);
	}

	/*
	 * If the type information is already assigned then
	 * this is a known _named_ type and we can stop parsing and
	 * exit now.
	 */
	if (type->t_code == TS_ENUM) {
		return (STAB_SUCCESS);
	}

	/* Get Name:Number pairs. */
	if (**s != ';') {
		/* Get a new type. */
		ret = ttable_get_type(&member_td, NULL);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}

		ret = resolve_enum_member(&member_td, s);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}

	}
	/* Skip ';'. */
	++(*s);

	/* Get type pointer again since the type table may have relocated. */
	if ((ret = ttable_td2ptr(*td, &type)) != STAB_SUCCESS) {
		return (ret);
	}
	/* Get the member type. */
	et = &type->t_tinfo.ti_et;

	/* Assign information. */
	type->t_code = TS_ENUM;
	et->et_number = 0;
	et->et_next = member_td;

	return (STAB_SUCCESS);
}


/*
 * resolve_fieldlist() - Build a tree of type_t nodes that make up the
 *	members of a struct or union.
 *	Unlike an enum that is a list these member are more like a tree
 *	since each node has a next member and the type that define the member.
 *
 * Parse:
 *	FieldName : { Type | Variant } , BitOffset, BitSize ; }+
 *
 * NOTE:
 *	Variant results in an STAB_NA since it is not possible in a
 *	C stab.
 */
static stabsret_t
resolve_fieldlist(typedesc_t *td, char **s, tcode_t tcode)
{
	typedesc_t new_td = TD_NOTYPE;
	typedesc_t field_td = TD_NOTYPE;
	hnode_t *hnode = NULL;
	type_t *type;
	mtype_t *mt;
	char *name;
	char *colon;
	uint_t bitoffset;
	uint_t bitsize;
	stabsret_t ret;

	name = *s;

	/* Find the colon marking the end of the name. */
	colon = strchr(name, ':');

	/* There must be a name. */
	if (colon == NULL) {
		return (STAB_FAIL);
	}
	/* Skip ':'. */
	*s = colon + 1;

	/* Temporarily null terminate the name so we can hash it. */
	*colon = '\0';
	/* Check if string exists. */
	ret = hash_get_name(name, &hnode, HASH_ENTER);
	/* Put colon back. */
	*colon = ':';

	if (ret != STAB_SUCCESS) {
		return (ret);
	}

	if (tcode == TS_UNDEF) {
		char *key = *s;
		keypair_t kp;
		/*
		 * Members of the su_type are already defined.
		 * Unfortunately, any new types will still have to get
		 * processed just in case another compilation unit requires
		 * them.
		 */
		if (get_keypair(&key, &kp) != STAB_SUCCESS) {
			return (STAB_NA);
		}
		if (*key == '=') {
			ret = resolve_type(&field_td, s, NULL);
			if (ret != STAB_SUCCESS) {
				return (ret);
			}
		} else {
			*s = key;
		}
	} else {
		/*
		 * If there is no string then make one and update
		 * the hash node.
		 */
		if (hnode->hn_stroffset == SO_NOSTRING) {
			/* Create a new string with contents. */
			ret = stringt_new_str(name, colon - name,
			    &hnode->hn_stroffset);
			if (ret != STAB_SUCCESS) {
				return (ret);
			}
		}

		/* Get the type descriptor for this member. */
		if (**s == '(') {
			ret = resolve_type(&field_td, s, NULL);
			if (ret != STAB_SUCCESS) {
				return (ret);
			}
		} else if (**s == 'v') {
			/* Variant */
			return (STAB_NA);
		} else {
			return (STAB_FAIL);
		}
	}

	SKIPCHAR(*s, ',');

	bitoffset = (uint_t)strtol(*s, s, 10);

	SKIPCHAR(*s, ',');

	bitsize = (uint_t)strtol(*s, s, 10);

	SKIPCHAR(*s, ';');

	/* Are there more field lists? */
	if (**s != ';') {
		if (tcode != TS_UNDEF) {
			/* Not a known type, get a new type. */
			ret = ttable_get_type(&new_td, NULL);
			if (ret != STAB_SUCCESS) {
				return (ret);
			}
			ret = resolve_fieldlist(&new_td, s, tcode);
			if (ret != STAB_SUCCESS) {
				return (ret);
			}
		} else {
			/* Known type so we can return after resolving it. */
			return (resolve_fieldlist(&new_td, s, tcode));
		}
	}

	/* Known type, we can return now. */
	if (tcode == TS_UNDEF) {
		return (STAB_SUCCESS);
	}

	/* We are resolving a new type, fill in the info. */
	if ((ret = ttable_td2ptr(*td, &type)) != STAB_SUCCESS) {
		return (ret);
	}
	mt = &type->t_tinfo.ti_mt;

	type->t_code = (tcode | TS_MEMBER);
	type->t_name = hnode->hn_stroffset;

	mt->mt_td = field_td;
	mt->mt_bitoffset = bitoffset;
	mt->mt_bitsize = bitsize;
	mt->mt_next = new_td;

	return (STAB_SUCCESS);
}

/*
 * resolve_su_type() - Get a type descriptor, and fill in the information,
 *	resolve_fieldlist() does most of the work here.
 *
 * Parse:
 *	{ s | u } Size FieldList ;
 *
 * NOTE:
 *	The trailing ';' seems to be a typo/omission in the
 *	Stabs Interface Manual (pg. 97)
 */
static stabsret_t
resolve_su_type(typedesc_t *td, char **s, keypair_t *kp,
    namestr_t *namestr)
{
	uint_t size;
	typedesc_t field_td = TD_NOTYPE;
	type_t *type;
	stype_t *st;
	tcode_t tcode;
	int code;
	stabsret_t ret;
	msi_t msi;

	/* Are we a struct or a union? */
	code = **s;
	switch (code) {
	case 's':
		tcode = TS_STRUCT;
		msi = MSI_STRUCT;
		break;
	case 'u':
		tcode = TS_UNION;
		msi = MSI_UNION;
		break;
	default:
		/* How did we get here? */
		return (STAB_FAIL);
	}
	/* Skip 's'. */
	++(*s);

	/* Get the struct/union's size. */
	size = (uint_t)strtol(*s, s, 10);

#ifdef __sparcv9
	/*
	 * SC5.0 under -xarch=v9 may append '@nn' to struct/union's size.
	 * According to the stabs guys:
	 *
	 * Since structs can be passed in registers on v9, the intent was to
	 * supply a register number here.  dbx doesn't need it, but until cc
	 * stops generating it, learn to ignore it.
	 */

	if (**s == '@') {
		*s += 3;
	}
#endif

	/*
	 * Structs and unions get their names expanded.
	 *	 ie "foo" => "struct foo"
	 */
	if (namestr != NULL && namestr->ms_str != NULL) {
		ret = expand_the_name(&exp_name, msi,
		    namestr->ms_str, namestr->ms_len);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
		/* We may have seen it before.. */
		ret = keypair_lookup_type(kp, td, &exp_name);
	} else {
		/*
		 * No name for this type,
		 * we cannot find out if we have seen it before.
		 */
		ret = keypair_lookup_type(kp, td, namestr);
	}

	if (ret != STAB_SUCCESS) {
		return (ret);
	}

	if ((ret = ttable_td2ptr(*td, &type)) != STAB_SUCCESS) {
		return (ret);
	}

	/*
	 * If the type information is already assigned then
	 * this is a known _named_ type, we must continue processing
	 * in case keypairs are different for this compilation unit.
	 */
	if (type->t_code == TS_UNION ||
	    type->t_code == TS_STRUCT) {
		/* Continue processing the known type. */
		ret = resolve_fieldlist(&field_td, s, TS_UNDEF);
	} else {
		/* Get a new type. */
		ret = ttable_get_type(&field_td, NULL);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
		/* Process it's members. */
		ret = resolve_fieldlist(&field_td, s, tcode);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}

		/* Get type pointer. */
		if ((ret = ttable_td2ptr(*td, &type)) != STAB_SUCCESS) {
			return (ret);
		}
		st = &type->t_tinfo.ti_st;

		/*
		 * Assign su information.
		 * We may have just resolved a forward reference,
		 * so set ALL attributes.
		 */
		type->t_code = tcode;
		st->st_size = size;
		st->st_first = field_td;
	}
	return (ret);
}


/*
 * resolve_float() -Assigns the type_t as a float type and assigns
 *	the information in the ftype_t.
 *
 * Parse:
 *	R Format ; Nbytes
 * NOTE:
 *	Stab interface manual is unclear if the last character is ';' or not!
 */
static stabsret_t
resolve_float(typedesc_t *td, char **s, keypair_t *kp, namestr_t *namestr)
{
	int format;
	int nbytes;
	type_t *type;
	ftype_t *ft;
	stabsret_t ret;

	SKIPCHAR(*s, 'R');

	/* Get a keypair reference. */
	ret = keypair_lookup_type(kp, td, namestr);
	if (ret != STAB_SUCCESS) {
		return (ret);
	}
	/* Get type pointer. */
	if ((ret = ttable_td2ptr(*td, &type)) != STAB_SUCCESS) {
		return (ret);
	}
	/* Get float type. */
	ft = &type->t_tinfo.ti_ft;

	/*
	 * If the type information is already assigned then
	 * this is a known _named_ type and we can stop parsing and
	 * exit now.
	 */
	if (type->t_code == TS_FLOAT) {
		return (STAB_SUCCESS);
	}

	/* Get Format. */
	format = (int)strtol(*s, s, 10);
	SKIPCHAR(*s, ';');

	/* Make sure format is sane. */
	switch (format) {
	default:
	case NF_NONE:		/* Undefined type */
	case NF_COMPLEX:	/* Fortran complex */
	case NF_COMPLEX16:	/* Fortran double complex */
	case NF_COMPLEX32:	/* Fortran complex*16 */
	case NF_INTERARITH:	/* Fortran interval arithmetic */
	case NF_DINTERARITH:	/* Fortran double interval arithmetic */
	case NF_QINTERARITH:	/* Fortran quad interval arithmetic */
		ret = STAB_FAIL;
		break;
	case NF_SINGLE:		/* IEEE 32 bit float	*/
	case NF_DOUBLE:		/* IEEE 64 bit float	*/
	case NF_LDOUBLE:	/* Long double		*/
		ret = STAB_SUCCESS;
		break;
	}
	if (ret != STAB_SUCCESS) {
		return (ret);
	}

	/* Get Nbytes. */
	nbytes = (int)strtol(*s, s, 10);

	/*
	 * May or may not contain a trailing ';' if it does go past it
	 * since there will never be anything after this point.
	 */
	if (**s == ';') {
		++(*s);
	}

	type->t_code = TS_FLOAT;
	ft->ft_format = format;
	ft->ft_nbytes = nbytes;

	return (STAB_SUCCESS);
}


/*
 * resolve_forward_reference() - Create a type_t that represents a
 *	forward reference. It is an error not to have the forward reference
 *	defined so, hopefully, a stab that occurs later will ultimately define
 *	the type_t that we are referencing.  When this occurs, the forward
 *	reference type being define NOW will get overwritten with the real
 *	information. See resolve_enum() and resolve_su_type().
 *
 * Parse:
 * 	x [ e | s | u | Type ] name
 *
 * NOTE:
 * 	Type_t's that are forward references may or may not have another
 *	type descriptor associated with them in the hash table.  However,
 *	they will ALWAYS have a name that may refer to a type defined later.
 */
static stabsret_t
resolve_forward_reference(typedesc_t *td, char **s, keypair_t *kp,
    namestr_t *namestr)
{
	int typespec;
	tcode_t tcode;
	msi_t msi;
	keypair_t new_kp;
	typedesc_t new_td = TD_NOTYPE;
	type_t *type;
	xtype_t *xt;
	char *name;
	char *colon;
	size_t namelen;
	hnode_t *hnode = NULL;
	stabsret_t ret;

	SKIPCHAR(*s, 'x');

	typespec = **s;
	switch (typespec) {
	default:
		return (STAB_FAIL);
	case 'e':
		tcode = TS_ENUM;
		msi = MSI_ENUM;
		++(*s);
		break;
	case 's':
		tcode = TS_STRUCT;
		msi = MSI_STRUCT;
		++(*s);
		break;
	case 'u':
		tcode = TS_UNION;
		msi = MSI_UNION;
		++(*s);
		break;
	case '(':
		tcode = TS_UNDEF;
		if ((ret = get_keypair(s, &new_kp)) != STAB_SUCCESS) {
			return (ret);
		}
		ret = keypair_lookup_type(&new_kp, &new_td, NULL);
		if (ret != STAB_SUCCESS) {
			return (STAB_FAIL);
		}
		break;
	}

	/* Get the name. */
	name = *s;

	colon = strchr(name, ':');

	if (colon == NULL) {
		return (STAB_FAIL);
	}
	namelen = colon - name;

	/* Skip ':'. */
	*s = colon + 1;

	ret = keypair_lookup_type(kp, td, namestr);
	if (ret != STAB_SUCCESS) {
		return (ret);
	}

	if (tcode != TS_UNDEF) {
		/* Axpand the name with struct/enum/union. */
		ret = expand_the_name(&exp_name, msi, name, namelen);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
		name = exp_name.ms_str;
		namelen = exp_name.ms_len;
	}

	if ((ret = hash_get_name(name, &hnode, HASH_ENTER)) != STAB_SUCCESS) {
		return (ret);
	}
	if (hnode->hn_stroffset == SO_NOSTRING) {
		/* Create a new string with contents. */
		ret = stringt_new_str(name, namelen, &hnode->hn_stroffset);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
	} else if (hnode->hn_td != TD_NOTYPE) {
		/* Already defined! */
		new_td = hnode->hn_td;
	}

	if ((ret = ttable_td2ptr(*td, &type)) != STAB_SUCCESS) {
		return (ret);
	}
	xt = &type->t_tinfo.ti_xt;
	type->t_code = (tcode | TS_FORWARD);

	xt->xt_name = hnode->hn_stroffset;
	xt->xt_td = new_td;

	return (STAB_SUCCESS);
}

/*
 * type_copy() - Given an empty destination type <*dest_td>, discover the
 *	type that it aliases (the source_type) and copy the information over.
 *	The differences (ie. name and type code) are then applied to the copy.
 *
 * Example:
 *	source	= typedef char foo;
 *	stab	= "foo:t(1,31)=(0,1)"
 */
static stabsret_t
type_copy(typedesc_t *dest_td, char **s, tcode_t tcode, keypair_t *kp,
    namestr_t *namestr)
{
	type_t *dest_type;
	typedesc_t src_td = TD_NOTYPE;
	type_t *src_type;
	stabsret_t ret;

	/* Find the type. */
	if ((ret = resolve_type(&src_td, s, NULL)) != STAB_SUCCESS) {
		return (ret);
	}

	ret = keypair_lookup_type(kp, dest_td, namestr);
	if (ret != STAB_SUCCESS) {
		return (ret);
	}

	/* Get the type pointer for the type given. */
	if ((ret = ttable_td2ptr(*dest_td, &dest_type)) != STAB_SUCCESS) {
		return (ret);
	}

	/*
	 * If the type information is already assigned then
	 * this is a known _named_ type and we can stop parsing and
	 * exit now.
	 */
	if (dest_type->t_code & tcode) {
		return (STAB_SUCCESS);
	}

	/* Get a type pointer. */
	if ((ret = ttable_td2ptr(src_td, &src_type)) != STAB_SUCCESS) {
		return (ret);
	}

	(void) memcpy(dest_type, src_type, sizeof (type_t));

	dest_type->t_code |= tcode;
	dest_type->t_name = SO_NOSTRING;
	dest_type->t_alias = src_td;

	return (STAB_SUCCESS);
}

/*
 * resolve_pointer() - Create a type_t that has the TS_POINTER bit set and
 *	and points to referred type of the pointer.
 *
 * Example:
 *	source	= typedef char **stringptr_t;
 *	stab	= "stringptr_t:t(1,1)=*(1,2)=*(0,1)"
 *
 *   This results in the following type_t list.
 *			  (1,1)        (1,2)       (0,1)
 *                          |            |           |
 *                          v            v           v
 *	"stringptr_t"<-->[ptype_t]-->[ptype_t]-->[btype_t]<-->"char"
 *
 * NOTE:
 *	Keypair (1,2) has no name associated with it.
 */
static stabsret_t
resolve_pointer(typedesc_t *td, char **s, keypair_t *kp,
    namestr_t *namestr)
{
	stabsret_t ret;
	typedesc_t new_td = TD_NOTYPE;
	type_t *type;
	ptype_t *pt;

	SKIPCHAR(*s, '*');

	/* Get the base type that we are "pointing" to. */
	if ((ret = resolve_type(&new_td, s, NULL)) != STAB_SUCCESS) {
			return (ret);
	}

	ret = keypair_lookup_type(kp, td, namestr);
	if (ret != STAB_SUCCESS) {
		return (ret);
	}

	/* Get the 'type_t *' for the pointer type. */
	if ((ret = ttable_td2ptr(*td, &type)) != STAB_SUCCESS) {
			return (ret);
	}

	/*
	 * If the type information is already assigned then
	 * don't bother doing it again.
	 */
	if (type->t_code == TS_POINTER) {
		return (STAB_SUCCESS);
	}

	pt = &type->t_tinfo.ti_pt;
	pt->pt_td = new_td;

	type->t_code = TS_POINTER;

	return (STAB_SUCCESS);
}

/*
 * resolve_type() - assigns a type to a type descriptor and if a keypair
 *	exists, then assigns the type descriptor to the keypair.
 */

static stabsret_t
resolve_type(typedesc_t *td, char **s, namestr_t *namestr)
{
	int typespec;
	stabsret_t ret;
	keypair_t kp;

	/* Get a key pair if it is there. */
	if (get_keypair(s, &kp) == STAB_SUCCESS) {
		/* Keypair exists. */
		if (**s != '=') {
			/*
			 * Not defining a type. Lookup the keypair and
			 * assign td.
			 */
			return (keypair_lookup_type(&kp, td, namestr));
		}
		/*
		 * In the process of defining a type.
		 *
		 * skip the equals
		 */
		++(*s);
	} else {
		/*
		 * This is necessary for the case when we have a complex
		 * type with no keypair.
		 */
		kp.kp_file = KP_EMPTY;
		kp.kp_type = KP_EMPTY;
	}

	/* Nitty Gritty time. */
	typespec = **s;
	switch (typespec) {
	default:
		ret = STAB_FAIL;
		break;
		/*
		 * The following are uninteresing.
		 */
	case 'C':	/* Conformant Array Bounds */
	case 'c':	/* Conformant Array */
	case 'd':	/* Dope Vector */
	case 'F':	/* Function parameter */
	case 'g':	/* Function with prototype */
	case 'L':	/* Pascal File */
	case 'P':	/* Procedure Parameter */
	case 'p':	/* Value Parameter */
	case 'q':	/* Reference Parameter */
	case 'S':	/* Set */
	case 'v':	/* Variant Record (Pascal) */
	case 'Y':	/* C++ Specification */
	case 'y':	/* Varying String (Pascal) */
	case '&':	/* Reference */
		ret = STAB_NA;
		break;

	case 'a':	/* Array */
		ret = resolve_array(td, s, &kp, namestr);
		break;
	case 'b':	/* Basic Integer */
		ret = resolve_basic(td, s, &kp, namestr);
		break;
	case 'e':	/* Enumeration */
		ret = resolve_enum(td, s, &kp, namestr);
		break;
	case 'r':	/* Range */
		/* Only valid if we are here via an array definition. */
		if (processing_array == 0) {
			ret = STAB_FAIL;
		} else {
			ret = resolve_range(td, s);
		}
		break;
	case 's':	/* Structure or Record */
	case 'u':	/* Union */
		ret = resolve_su_type(td, s, &kp, namestr);
		break;
	case 'R':	/* Floating Point (Real) */
		ret = resolve_float(td, s, &kp, namestr);
		break;
	case 'x':	/* Forward Reference */
		ret = resolve_forward_reference(td, s, &kp, namestr);
		break;
	case '*':	/* Pointer */
		ret = resolve_pointer(td, s, &kp, namestr);
		break;
	case 'f':	/* Function */
		/*
		 * All that we are given is the return type of the function.
		 * Simply make a copy of the return type of the funtion.
		 *
		 * skip the 'f'
		 */
		++(*s);
		ret = type_copy(td, s, TS_FUNC, &kp, namestr);
		break;
	case 'k':	/* Const */
		++(*s);
		ret = type_copy(td, s, TS_CONST, &kp, namestr);
		break;
	case 'B':	/* Volatile */
		++(*s);
		ret = type_copy(td, s, TS_VOLATILE, &kp, namestr);
		break;
	case 'K':	/* Restricted */
		++(*s);
		ret = type_copy(td, s, TS_RESTRICTED, &kp, namestr);
		break;
	case '(':	/* Straight typedef/alias */
		/*
		 * May be useful for printing.
		 * Make a copy of it and set the TS_TYPEDEF bit.
		 */
		ret = type_copy(td, s, TS_TYPEDEF, &kp, namestr);
		break;
	}
	return (ret);
}

/*
 * set_type_name() - Set the name of the type in the type_t as well as
 * 	update the hash node.
 */
static stabsret_t
set_type_name(type_t *type, typedesc_t td, char *name, size_t namelen)
{
	hnode_t *hnode = NULL;
	stabsret_t ret;

	/* Get the hash node associated with the name. */
	if ((ret = hash_get_name(name, &hnode, HASH_ENTER)) != STAB_SUCCESS) {
		return (ret);
	}

	/* Received a new hash node if there is no string. */
	if (hnode->hn_stroffset == SO_NOSTRING) {
		/* Create a new string with contents. */
		ret = stringt_new_str(name, namelen, &hnode->hn_stroffset);
		if (ret != STAB_SUCCESS) {
			return (ret);
		}
	}

	/*
	 * The hash node may have a string associated with it and yet
	 * not have a type descriptor defined.
	 * This can happen if a hash entry was created for a structure member
	 * and now we wish to define a type with the same name.
	 */
	if (hnode->hn_td == TD_NOTYPE) {
		hnode->hn_td = td;
		type->t_name = hnode->hn_stroffset;
	}

	return (STAB_SUCCESS);
}

/*
 * name_the_type() - Name a type from its type descriptor and its tcode.
 *	We must also create a a type_t and hash entry for a pointer to this
 *	type.
 */
static stabsret_t
name_the_type(typedesc_t td, char *name, char *colon)
{
	type_t *type;
	stabsret_t ret;
	size_t namelen;
	char save;
	tcode_t nomangle;

	nomangle = TS_BASIC | TS_FLOAT | TS_ARRAY | TS_FUNC | TS_FORWARD |
	    TS_POINTER | TS_TYPEDEF;

	namelen = colon - name;

	if (ttable_td2ptr(td, &type) != STAB_SUCCESS) {
		return (STAB_FAIL);
	}

	if (type->t_code & nomangle) {
		save = *colon;
		*colon = '\0';
		ret = set_type_name(type, td, name, namelen);
		*colon = save;
	} else if (type->t_code & TS_ENUM) {
		ret = expand_the_name(&exp_name, MSI_ENUM, name, namelen);
		if (ret == STAB_SUCCESS) {
			ret = set_type_name(type, td, exp_name.ms_str,
			    exp_name.ms_len);
		}
	} else if (type->t_code & TS_STRUCT) {
		ret = expand_the_name(&exp_name, MSI_STRUCT, name, namelen);
		if (ret == STAB_SUCCESS) {
			ret = set_type_name(type, td, exp_name.ms_str,
			    exp_name.ms_len);
		}
	} else if (type->t_code & TS_UNION) {
		ret = expand_the_name(&exp_name, MSI_UNION, name, namelen);
		if (ret == STAB_SUCCESS) {
			ret = set_type_name(type, td, exp_name.ms_str,
			    exp_name.ms_len);
		}
	} else {
		ret = STAB_FAIL;
	}

	return (ret);
}

/*
 * process_type() - Every .stab line that defines a type starts here.
 *	extract the name of the type so we can discover if it is a known
 *	type later.
 *
 * Parse:
 *	[ Name ] : TypeSpecifier
 *		Name is optional
 */
static stabsret_t
process_type(char *stabstr, char **typestr)
{
	char *colon = *typestr;
	char *kp;
	stabsret_t ret;
	typedesc_t td = TD_NOTYPE;
	namestr_t namestr;

	if (stabstr == colon) {
		/* Anonymous type. */
		namestr.ms_str = NULL;
		namestr.ms_len = 0;
	} else {
		namestr.ms_str = stabstr;
		namestr.ms_len = colon - stabstr;
	}

	/* Mode to the begining of a keypair. */
	kp = strchr(*typestr, '(');
	if (kp == NULL) {
		return (STAB_FAIL);
	}
	*typestr = kp;

	namestr.ms_size = 0;

	/* Parse the type. */
	ret = resolve_type(&td, typestr, &namestr);

	/* Return if parse failure or the type is unnamed. */
	if (ret != STAB_SUCCESS || namestr.ms_str == NULL) {
		return (ret);
	}

	/* We have a type descriptor and a name for it Woohoo! */
	return (name_the_type(td, namestr.ms_str, colon));
}


/*
 * add_stab() - First level processing.
 *	1. Discover interesting stab types.
 *	2. Discover the symbol we are describing.
 *
 * Parse:
 *	.stab "[Name]:[tT].+",StabType,#,#,#
 *
 * NOTE:
 *	Only N_LSYM and N_GSYM are the interesting stab types and of those
 *	only 't' and 'T' Symbol Descriptor are interesting.
 *
 *	    't' - Represent types and typdefs.
 *	    'T' - Represent Structs, Unions, or Enums which may be anonymous.
 */
stabsret_t
add_stab(int stabtype, char *stabstr)
{
	char *colon;
	int symdesc;
	stabsret_t ret;

	switch (stabtype) {
		/*
		 * If we do not know it, then we cannot parse it.
		 */
	default:
		ret = STAB_FAIL;
		break;

		/*
		 * Simple Values.
		 * These occur only in the stabs of elf files.
		 */
	case N_UNDF:	/* undefined */
	case N_ABS:	/* absolute */
	case N_TEXT:	/* text */
	case N_DATA:	/* data */
	case N_BSS:	/* bss */
	case N_COMM:	/* common (internal to ld) */
	case N_FN:	/* file name symbol */
		/*
		 * The following types are completely uninteresting.
		 */
	case N_FNAME:	/* procedure name (f77 kludge): name,,0 */
	case N_STSYM:	/* static symbol: name,,0,type,0 or section relative */
	case N_LCSYM:	/* .lcomm symbol: name,,0,type,0 or section relative */
	case N_MAIN:	/* name of main routine : name,,0,0,0 */
	case N_ROSYM:	/* ro_data: name,,0,type,0 or section relative */
	case N_PC:	/* global pascal symbol: name,,0,subtype,line */
	case N_CMDLINE:	/* command line info */
	case N_OBJ:	/* object file path or name */
	case N_OPT:	/* compiler options */
	case N_SLINE:	/* src line: 0,,0,linenumber,function relative */
	case N_XLINE:	/* h.o. src line: 0,,0,linenumber>>16,0 */
	case N_ILDPAD:	/* now used as ild pad stab */
	case N_SSYM:	/* structure elt: name,,0,type,struct_offset */
	case N_SO:	/* source file name: name,,0,0,0 */
	case N_BINCL:	/* header file: name,,0,0,0 */
	case N_SOL:	/* #included file name: name,,0,0,0 */
	case N_EINCL:	/* end of include file */
	case N_ENTRY:	/* alternate entry: name,linenumber,0 */
	case N_SINCL:	/* shared include file */
	case N_LBRAC:	/* lft bracket: 0,,0,nesting level,function relative */
	case N_EXCL:	/* excluded include file */
	case N_USING:	/* C++ using command */
	case N_ISYM:	/* position independent type symbol, internal */
	case N_ESYM:	/* position independent type symbol, external */
	case N_PATCH:	/* Instruction to be ignored by run-time checking. */
	case N_CONSTRUCT:	/* C++ constructor call. */
	case N_DESTRUCT:	/* C++ destructor call. */
	case N_RBRAC:	/* rt bracket: 0,,0,nesting level,function relative */
	case N_BCOMM:	/* begin common: name,, */
	case N_TCOMM:	/* begin task common: name,, */
	case N_ECOMM:	/* end task_common/common: name,, */
	case N_XCOMM:	/* excluded common block */
	case N_ECOML:	/* end common (local name): ,,address */
	case N_WITH:	/* pascal with statement: type,,0,0,offset */
	case N_LENG:	/* second stab entry with length information */

		/*
		 * The following may be interesting at a later rev.
		 */
	case N_ALIAS:	/* alias name: name,,0,0,0 */
	case N_PSYM:	/* parameter: name,,0,type,offset */
	case N_RSYM:	/* register sym: name,,0,type,register */
	case N_FUN:	/* procedure: name,,0,linenumber,0 */
		ret = STAB_NA;
		break;

	case N_ENDM:	/* Last stab emitted for module. */
		/*
		 * New compilation unit.
		 * Flush the keypair table and reuse.
		 */
		ret = keypair_flush_table();
		break;

	case N_GSYM:	/* global symbol: name,,0,type,0 */
	case N_LSYM:	/* local sym: name,,0,type,offset */
		colon = strchr(stabstr, ':');
		if (colon == NULL) {
			/* No colon means no type. */
			ret = STAB_FAIL;
			break;
		}
		symdesc = *(colon + 1);

		/* Anonymous types must have a symdesc == 'T'. */
		if (colon == stabstr &&
		    symdesc != 'T') {
			/*
			 * Anonymous name are allowed only 'T'
			 * symbol descriptors.
			 */
			ret = STAB_FAIL;
			break;
		}

		switch (symdesc) {
		default:
			ret = STAB_FAIL;
			break;
		case '(':	/* Local Variable (empty) */
		case 'A':	/* Automatic Variable */
		case 'b':	/* Based Variable */
		case 'C':	/* Conformant Array Bound */
		case 'c':	/* Constant */
		case 'G':	/* Global Variable */
		case 'H':	/* Hosted, Non-Local Variable */
		case 'L':	/* Lines in Template (LT) */
		case 'l':	/* Literal */
		case 'V':	/* Common or Static Local Variable */
		case 'W':	/* Generic Name */
		case 'Y':	/* Anonymous Unions ??? (Ya) */
			ret = STAB_NA;
			break;
		case 'T':	/* Enum, Struct or Union */
		case 't':	/* Type Name */
			ret = process_type(stabstr, &colon);
			break;
		}
	}

	return (ret);
}
