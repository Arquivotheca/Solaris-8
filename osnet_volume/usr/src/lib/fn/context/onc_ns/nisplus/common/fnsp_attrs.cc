/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnsp_attrs.cc	1.21	97/10/27 SMI"

#include "fnsp_attrs.hh"
#include "fnsp_internal.hh"
#include "FNSP_nisplusImpl.hh"
#include <fnsp_utils.hh>

#include <rpcsvc/nis.h>
#include <xfn/fn_xdr.hh>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

// table layout for ctx table
#define	FNSP_NAME_COL 0
#define	FNSP_ATTR_COL 3

// table layout for attribute table
#define	FNSP_ATTR_ID_COL 1
#define	FNSP_ATTR_ID_FORMAT_COL 2
#define	FNSP_ATTR_VALUE_COL 3
#define	FNSP_ATTR_SYNTAX_COL 4
#define	FNSP_ATTR_SYNTAX_FORMAT_COL 5
#define	FNSP_ATTR_TABLE_WIDTH 6

#define	FNSP_DEFAULT_TTL 43200

#define	NOBODY_RIGHTS ((NIS_READ_ACC) << 24)
#define	WORLD_RIGHTS (NIS_READ_ACC)
#define	GROUP_RIGHTS ((NIS_READ_ACC |\
	NIS_MODIFY_ACC |\
	NIS_CREATE_ACC |\
	NIS_DESTROY_ACC) << 8)
#define	FNSP_DEFAULT_RIGHTS (NOBODY_RIGHTS | WORLD_RIGHTS | OWNER_DEFAULT | \
	GROUP_RIGHTS)
#define	FNSP_TABLE_CREATE_RIGHTS (((NIS_CREATE_ACC)<<24)|(NIS_CREATE_ACC))

#define	ENTRY_FLAGS(obj, col) \
	(obj)->EN_data.en_cols.en_cols_val[col].ec_flags


static const char *FNSP_attrid_col_label = FNSP_ATTRID_COL_LABEL;
static const char *FNSP_attrvalue_col_label = FNSP_ATTRVAL_COL_LABEL;
static const char *FNSP_attrsyntax_col_label = "attrsyntax";
static const char *FNSP_attrsyntaxformat_col_label = "attrsyntaxformat";
static const char *FNSP_attridformat_col_label = "attridentifierformat";
static const char *FNSP_ctx_directory = "ctx_dir.";

static const char *FNSP_name_col_label = FNSP_NAME_COL_LABEL;
static const char *FNSP_ctx_col_label = FNSP_CONTEXTNAME;
static const char *__nis_default_path = FNSP_NIS_DEFAULT_PATH;
static const char *__nis_default_table_type = FNSP_NIS_DEFAULT_TABTYPE;
static const char __nis_default_separator = FNSP_NIS_DEFAULT_SEP;

static const FN_string FNSP_self_name((unsigned char *) FNSP_SELF_NAME);

static const char *FNSP_org_attr_str = "fns_attribute.";
static const char *FNSP_org_map = "fns.ctx_dir.";

// Routines to encode and decode binary attribute values for
// single table implementation.
static const char *ascii_syntax = "fn_attr_syntax_ascii";
static const char *encoder_seperator = "!";
static const char nisplus_special_char = '\"';
static const char nisplus_escape_char = 'a';
static char *
FNSP_encode(const void *data, size_t len, unsigned &status)
{
	char *answer, *buffer;
	char data_length[12];

	// Check if data is NULL
	if (data == NULL) {
		status = FN_SUCCESS;
		return (0);
	}

	// uuenode the binary data
	buffer = FNSP_encode_binary(data, len, status);
	if (status != FN_SUCCESS)
		return (0);

	sprintf(data_length, "%d", len);
	answer = (char *) malloc(strlen(data_length) +
	     strlen(encoder_seperator) + strlen(buffer) + 1);
	if (answer == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	strcpy(answer, data_length);
	strcat(answer, encoder_seperator);
	strcat(answer, buffer);
	free(buffer);

	// Check for the special chars
	for (int i = 0; i < strlen(answer); i++)
		if (answer[i] == nisplus_special_char)
			answer[i] = nisplus_escape_char;
	return (answer);
}

static void *
FNSP_decode(const char *data, int *length, unsigned &status)
{
	char number[12];
	char *answer;
	const char *encoded_data;
	int i;
	for (i = 0; data[i] != encoder_seperator[0]; i++)
		number[i] = data[i];
	number[i] = '\0';
	(*length) = atoi(number);

	// Perform the de-mangling operation
	answer = (char *) malloc(strlen(&data[i+1]) + 1);
	if (answer == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	encoded_data = &data[i+1];
	int data_len = strlen(encoded_data);
	for (i = 0; i < data_len; i++) {
		if (encoded_data[i] == nisplus_escape_char)
			answer[i] = nisplus_special_char;
		else
			answer[i] = encoded_data[i];
	}
	answer[i] = '\0';

	void *buffer = FNSP_decode_binary(answer, (*length), status);
	if (status != FN_SUCCESS)
		return (0);

	free(answer);
	return(buffer);
}

extern "C" {
nis_result * __nis_list_localcb(nis_name, u_long,
    int (*)(nis_name, nis_object *, void *), void *);
};

extern void FNSP_nisplus_admin_group(const char *, char *);

static inline void
free_nis_result(nis_result *res)
{
	if (res)
		nis_freeresult(res);
}

static FN_attrvalue *
get_attrvalue_from_entry(const FN_identifier *syntax, nis_object *ent)
{
	FN_attrvalue *value = 0;
	char *encoded_data;
	void *decoded_data;
	size_t decoded_data_len;
	unsigned status;

	if (ENTRY_LEN(ent, FNSP_ATTR_VALUE_COL) == 0)
		return (0);

	if (strncmp((char *) syntax->contents(),
	    ascii_syntax, strlen(ascii_syntax)) == 0) {
		value = new FN_attrvalue((unsigned char *)
		    ENTRY_VAL(ent, FNSP_ATTR_VALUE_COL));
	} else {
		encoded_data = ENTRY_VAL(ent,
		    FNSP_ATTR_VALUE_COL);
		decoded_data = FNSP_decode(encoded_data,
		    (int *) &decoded_data_len, status);
		if ((status != FN_SUCCESS) || (!decoded_data))
			return (0);
		value = new FN_attrvalue(
		    decoded_data, decoded_data_len);
		free(decoded_data);
	}
	return (value);
}

static int
add_attr_to_attrset(char *, nis_object *ent, void *data)
{
	FN_attrset *answer = (FN_attrset *) data;

	// Takes care of various format types, STRING, UUID and OID_STRING
	u_short *format = (u_short *) ENTRY_VAL(ent, FNSP_ATTR_ID_FORMAT_COL);
	unsigned int id_format = (unsigned int) ntohs(*format);
	FN_identifier *id = new FN_identifier(id_format,
	    (size_t) (ENTRY_LEN(ent, FNSP_ATTR_ID_COL) - 1),
	    (const unsigned char *) ENTRY_VAL(ent, FNSP_ATTR_ID_COL));
	if (!id)
		return (0);

	FN_attrvalue *value;
	const FN_attribute *attr = answer->get(*id);
	if (!attr) {
		format = (u_short *)
		    ENTRY_VAL(ent, FNSP_ATTR_SYNTAX_FORMAT_COL);
		id_format = (unsigned int) ntohs(*format);
		FN_identifier *syntax = new FN_identifier(id_format,
		    (size_t) (ENTRY_LEN(ent, FNSP_ATTR_SYNTAX_COL) - 1),
		    (const unsigned char *)
		    ENTRY_VAL(ent, FNSP_ATTR_SYNTAX_COL));
		if (syntax == 0) {
			delete id;
			return (0);
		}
		FN_attribute new_attr(*id, *syntax);
		value = get_attrvalue_from_entry(syntax, ent);
		if (value)
			new_attr.add(*value);
		answer->add(new_attr);
		delete syntax;
	} else {
		// add value to existing attr and update attr
		FN_attribute merged_attr(*attr);
		value = get_attrvalue_from_entry(attr->syntax(), ent);
		if (value == 0) {
			delete id;
			return (0);
		}
		merged_attr.add(*value);
		answer->add(merged_attr, FN_OP_SUPERCEDE);
	}

	delete value;
	delete id;
	return (0);
}

static FN_attrset *
FNSP_get_attrset_single(const FN_string &tabname, const FN_string &aname,
    unsigned &status, unsigned nisflags)
{
	char sname[NIS_MAXNAMELEN+1];
	FN_attrset *attrset = 0;
	nis_result *res;
	nis_name tablename = (nis_name) tabname.str(&status);

	if (status != FN_SUCCESS)
		return (0);

	sprintf(sname, "[%s=\"%s\"],%s",
	    FNSP_name_col_label, aname.str(&status), tablename);
	if (status != FN_SUCCESS)
		return (0);

	if (sname[strlen(sname)-1] != '.')
		strcat(sname, ".");

	// Assumes that single table implementation uses a seperate
	// table for attribute implementation and the table name
	// is provided

	attrset = new FN_attrset;

	res = __nis_list_localcb(sname, FNSP_nisflags|nisflags,
	    add_attr_to_attrset,
	    (void *) attrset);

	if (res->status == NIS_RPCERROR) {
		// may have failed because too big
		free_nis_result(res);
		unsigned long new_flags = (FNSP_nisflags&(~USE_DGRAM));
		res = __nis_list_localcb(sname, new_flags|nisflags,
		    add_attr_to_attrset, (void *) attrset);
	}

	if ((res->status == NIS_CBRESULTS) || (res->status == NIS_SUCCESS))
		status = FN_SUCCESS;
	else if ((res->status == NIS_NOTFOUND) ||
		    (res->status == NIS_NOSUCHNAME) ||
		    (res->status == NIS_NOSUCHTABLE))
		status = FN_SUCCESS;
	else {
		status = FNSP_map_result(res, 0);
		delete attrset;
		attrset = 0;
	}

	// should return 0 if no attributes are found
	free_nis_result(res);
	return (attrset);
}

int
FNSP_store_attrset(const FN_attrset *aset, char **retbuf, size_t *retsize)
{
	char *attrbuf = NULL;
	int attrlen = 0;

	if (aset != NULL && aset->count() > 0) {
		attrbuf = FN_attr_xdr_serialize((*aset), attrlen);
		if (attrbuf == NULL) {
			*retbuf = NULL;
			*retsize = 0;
			return (0);
		}
	}

	*retbuf = attrbuf;
	*retsize = attrlen;

	return (1);
}


FN_attrset*
FNSP_extract_attrset_result(nis_result *res, unsigned &status)
{
	FN_attrset *attrset;

	/* extract attribute set */
	attrset = FN_attr_xdr_deserialize(ENTRY_VAL(res->objects.objects_val,
	    FNSP_ATTR_COL), ENTRY_LEN(res->objects.objects_val,
	    FNSP_ATTR_COL), status);
	return (attrset);
}



static FN_attrset *
FNSP_get_attrset_shared(const FN_string &tabname, const FN_string &cname,
    const FN_string &aname,
    unsigned &status, unsigned nisflags)
{
	char sname[NIS_MAXNAMELEN+1];
	FN_attrset *attrset;
	nis_result *res;
	nis_name tablename = (nis_name) tabname.str(&status);

	if (status != FN_SUCCESS)
		return (0);

	sprintf(sname, "[%s=\"%s\",%s=\"%s\"],%s",
		FNSP_name_col_label, aname.str(&status),
		FNSP_ctx_col_label, cname.str(&status), tablename);
	if (status != FN_SUCCESS)
		return (0);

	if (sname[strlen(sname)-1] != '.')
		strcat(sname, ".");

	res = nis_list(sname, FNSP_nisflags|nisflags, NULL, NULL);

	status = FNSP_map_result(res, 0);

	if (status == FN_SUCCESS)
		attrset = FNSP_extract_attrset_result(res, status);
	else
		attrset = 0;

	free_nis_result(res);
	return (attrset);
}

static FN_attrset *
FNSP_get_attrset_aux(const FNSP_Address &context, const FN_string &atomic_name,
    unsigned &status, unsigned nisflags)
{
	switch (context.get_impl_type()) {
	case FNSP_single_table_impl:
		return FNSP_get_attrset_single(context.get_table_name(),
		    atomic_name, status, nisflags);

	case FNSP_shared_table_impl:
		return FNSP_get_attrset_shared(context.get_table_name(),
		    FNSP_self_name, atomic_name,
		    status, nisflags);

	case FNSP_entries_impl:
		return FNSP_get_attrset_shared(context.get_table_name(),
		    context.get_index_name(),
		    atomic_name, status,
		    nisflags);
	case FNSP_directory_impl:
	case FNSP_null_impl:
		status = FN_E_OPERATION_NOT_SUPPORTED;
		return (0);

	default:
		status = FN_E_UNSPECIFIED_ERROR;
		return (0);
	}
}

FN_attrset *
FNSP_get_attrset(const FNSP_Address &parent, const FN_string &atomic_name,
    unsigned &status)
{
	return (FNSP_get_attrset_aux(parent, atomic_name, status,
	    parent.get_access_flags()));
}

FN_attrset *
FNSP_nisplusImpl::get_attrset(const FN_string &atomic_name, unsigned &status)
{
	return (FNSP_get_attrset(*my_address, atomic_name, status));
}


// Function defined in fnsp_internal.cc to split the internal name
// into table name and domainname
extern unsigned
split_internal_name(const FN_string &wholename,
    FN_string **head,
    FN_string **tail);

static int
FNSP_single_map_add_attribute(const FN_string &tabname,
    const FN_string &aname, const FN_attribute &attribute,
    int change_permissions = 0)
{
	unsigned status;
	nis_result *res;
	const FN_identifier *id;
	const FN_identifier *syntax;
	const FN_attrvalue *value;
	FN_string *value_string;
	void *ip;
	char atomic_tablename[NIS_MAXNAMELEN+1];
	nis_name tablename;
	char owner[NIS_MAXNAMELEN+1];
	char group[NIS_MAXNAMELEN+1];

	// If user is unauthenticated, return
	if (!key_secretkey_is_set())
		return (FN_E_ATTR_NO_PERMISSION);

	if (attribute.valuecount() == 0)
		return (FN_E_INVALID_ATTR_VALUE);
	tablename = (nis_name) tabname.str(&status);
	if (status != FN_SUCCESS)
		return (status);

	char 		*sname, *encoded_attr;
	nis_object 	obj;
	entry_col	*cols = 0;
	entry_obj	*eo;
	int 		num_cols = FNSP_ATTR_TABLE_WIDTH;

	cols = (entry_col *) calloc(num_cols, sizeof (entry_col));
	if (cols == NULL) {
		return (FN_E_INSUFFICIENT_RESOURCES);
	}
	memset((char *)(&obj), 0, sizeof (obj));

	nis_leaf_of_r(tablename, atomic_tablename, NIS_MAXNAMELEN);
	obj.zo_name = atomic_tablename;
	FNSP_nisplus_admin_group(tablename, group);
	obj.zo_group = (nis_name) group;
	if (change_permissions) {
		// Obtain the user/host`s domainname name
		FN_string *table, *ctx_domain;
		char domainname[NIS_MAXNAMELEN + 1];
		if ((status = split_internal_name(tabname,
		    &table, &ctx_domain)) != FN_SUCCESS)
			return (status);
		strcpy(domainname, (char *)
		    &((ctx_domain->str())[strlen(FNSP_ctx_directory)]));
		delete table;
		delete ctx_domain;

		// Construct the principal name
		strcpy(owner, (char *) aname.str());
		strcat(owner, ".");
		strcat(owner, domainname);
		obj.zo_owner = (nis_name) owner;

		// Check if member has permissions to add attributes
		nis_name local_principal = nis_local_principal();
		if (strcmp((char *) local_principal, owner) != 0) {
			// Check if member of a group
			if (!nis_ismember(local_principal, obj.zo_group)) {
				free (cols);
				return (FN_E_ATTR_NO_PERMISSION);
			}
		}
	} else
		obj.zo_owner = nis_local_principal();
	obj.zo_access = FNSP_DEFAULT_RIGHTS;
	obj.zo_ttl = FNSP_DEFAULT_TTL;
	obj.zo_data.zo_type = NIS_ENTRY_OBJ;
	eo = &(obj.EN_data);
	eo->en_type = (char *)__nis_default_table_type;
	eo->en_cols.en_cols_val = cols;
	eo->en_cols.en_cols_len = num_cols;

	ENTRY_VAL(&obj, FNSP_NAME_COL) = (nis_name) (aname.str(&status));
	if (status != FN_SUCCESS) {
		free(cols);
		return (status);
	}
	ENTRY_LEN(&obj, FNSP_NAME_COL) = aname.bytecount()+1;

	id = attribute.identifier();
	ENTRY_VAL(&obj, FNSP_ATTR_ID_COL) = (nis_name) id->str(&status);
	if (status != FN_SUCCESS) {
		free(cols);
		return (status);
	}
	// Store the format of identifier in one byte
	ENTRY_LEN(&obj, FNSP_ATTR_ID_COL) = id->length() + 1;
	unsigned int id_format = id->format();
	u_short id_net_format = htons((u_short) id_format);
	ENTRY_VAL(&obj, FNSP_ATTR_ID_FORMAT_COL) = (char *) &id_net_format;
	ENTRY_LEN(&obj, FNSP_ATTR_ID_FORMAT_COL) = sizeof (u_short);
	ENTRY_FLAGS(&obj, FNSP_ATTR_ID_FORMAT_COL) = EN_BINARY;

	syntax = attribute.syntax();
	ENTRY_VAL(&obj, FNSP_ATTR_SYNTAX_COL) = (char *) syntax->str(&status);
	if (status != FN_SUCCESS) {
		free(cols);
		return (status);
	}
	ENTRY_LEN(&obj, FNSP_ATTR_SYNTAX_COL) = syntax->length() + 1;
	unsigned int syntax_format = syntax->format();
	u_short syntax_net_format = htons((u_short) syntax_format);
	ENTRY_VAL(&obj, FNSP_ATTR_SYNTAX_FORMAT_COL) =
	    (char *) &syntax_net_format;
	ENTRY_LEN(&obj, FNSP_ATTR_SYNTAX_FORMAT_COL) = sizeof (u_short);
	ENTRY_FLAGS(&obj, FNSP_ATTR_SYNTAX_FORMAT_COL) = EN_BINARY;

	// Since more than one value can be present, this section
	// must be repeated until attribute value is NULL
	// Should contain attribute values and identifier
	for (value = attribute.first(ip);
	    value != NULL;
	    value = attribute.next(ip)) {
		// Check the attribute syntax value
		if (strncmp((char *) syntax->contents(),
		    ascii_syntax, strlen(ascii_syntax)) == 0) {
			value_string = value->string();
			if (value_string) {
				encoded_attr = strdup((char *)
				    value_string->str(&status));
				if (status != FN_SUCCESS) {
					free(cols);
					return (status);
				}
				ENTRY_VAL(&obj, FNSP_ATTR_VALUE_COL) =
				    (nis_name) encoded_attr;
				ENTRY_LEN(&obj, FNSP_ATTR_VALUE_COL) =
					value_string->bytecount() + 1;
				delete value_string;
			} else {
				encoded_attr = 0;
				// Takes care of empty attribute values
				ENTRY_LEN(&obj, FNSP_ATTR_VALUE_COL) = 0;
			}
		} else {
			// uuencode the attribute value
			encoded_attr = FNSP_encode(value->contents(),
			    value->length(), status);
			if (status != FN_SUCCESS) {
				free(cols);
				return (status);
			}
			if (encoded_attr) {
				ENTRY_VAL(&obj, FNSP_ATTR_VALUE_COL) =
				    (nis_name) encoded_attr;
				ENTRY_LEN(&obj, FNSP_ATTR_VALUE_COL) =
					strlen(encoded_attr) + 1;
			} else {
				encoded_attr = 0;
				// Takes care of empty attribute values
				ENTRY_LEN(&obj, FNSP_ATTR_VALUE_COL) = 0;
			}
		}

		if (encoded_attr) {
			sname = (char *) malloc(strlen(FNSP_name_col_label) +
			    strlen((char *) aname.str()) +
			    strlen(FNSP_attrid_col_label) +
			    strlen((char *) id->str()) +
			    strlen(FNSP_attrvalue_col_label) +
			    strlen(encoded_attr) +
			    strlen((char *) tablename) +17);
			if (sname == 0)
				return (FN_E_INSUFFICIENT_RESOURCES);
			sprintf(sname, "[%s=\"%s\",%s=\"%s\",%s=\"%s\"],%s",
			    FNSP_name_col_label, aname.str(),
			    FNSP_attrid_col_label, id->str(),
			    FNSP_attrvalue_col_label, encoded_attr,
			    tablename);
		} else {
			sname = (char *) malloc(strlen(FNSP_name_col_label) +
			    strlen((char *) aname.str()) +
			    strlen(FNSP_attrid_col_label) +
			    strlen((char *) id->str()) +
			    strlen((char *) tablename) + 13);
			if (sname == 0)
				return (FN_E_INSUFFICIENT_RESOURCES);
			sprintf(sname, "[%s=\"%s\",%s=\"%s\"],%s",
			    FNSP_name_col_label, aname.str(),
			    FNSP_attrid_col_label, id->str(),
			    tablename);
		}
		if (sname[strlen(sname)-1] != '.')
			strcat(sname, ".");

		res = nis_add_entry(sname, &obj, ADD_OVERWRITE);
		status = FNSP_map_result(res,
		    "could not add entry to attribute table");

		if (encoded_attr)
			free(encoded_attr);
		free_nis_result(res);
		if (status != FN_SUCCESS) {
			free(cols);
			return (status);
		}
		free(sname);
	}

	free(cols);
	return (status);
}

static int
FNSP_single_map_remove_attribute(const FN_string &tabname,
    const FN_string &aname, const FN_attribute &attribute)
{
	unsigned status;
	nis_result *res;
	char *sname;
	const FN_attrvalue *value;
	FN_string *value_string;
	void *ip;

	const unsigned char *aname_str = aname.str(&status);
	if (status != FN_SUCCESS)
		return (status);
	const FN_identifier *id = attribute.identifier();
	const FN_identifier *syntax = attribute.syntax();
	const unsigned char *id_str = id->str(&status);
	if (status != FN_SUCCESS)
		return (status);

	nis_name tablename = (nis_name) tabname.str(&status);
	id = attribute.identifier();
	char *encoded_attr;
	for (value = attribute.first(ip);
	    value != NULL;
	    value = attribute.next(ip)) {
		if (strncmp((char *) syntax->contents(),
		    ascii_syntax, strlen(ascii_syntax)) == 0) {
			if ((value_string = value->string()) == 0)
				return (FN_E_INSUFFICIENT_RESOURCES);
			encoded_attr = strdup((char *)
			    value_string->str(&status));
			delete value_string;
		} else {
			encoded_attr = FNSP_encode(value->contents(),
			    value->length(), status);
		}
		if (status != FN_SUCCESS) {
			return (status);
		}

		sname = (char *) malloc(strlen(FNSP_name_col_label) +
		    strlen((char *) aname.str()) +
		    strlen(FNSP_attrid_col_label) +
		    strlen((char *) id_str) +
		    strlen(FNSP_attrvalue_col_label) +
		    strlen(encoded_attr) + strlen((char *) tablename) +17);
		if (sname == 0)
			return (FN_E_INSUFFICIENT_RESOURCES);
		sprintf(sname, "[%s=\"%s\",%s=\"%s\",%s=\"%s\"],%s",
		    FNSP_name_col_label, aname_str,
		    FNSP_attrid_col_label, id_str,
		    FNSP_attrvalue_col_label, encoded_attr,
		    tablename);

		res = nis_remove_entry(sname, 0, REM_MULTIPLE);
		free(sname);
		free(encoded_attr);
		switch (res->status) {
		case NIS_SUCCESS:
		case NIS_NOTFOUND:
			status = FN_SUCCESS;
			free_nis_result(res);
			break;
		default:
			status = FNSP_map_result(res, "cannot empty table");
			free_nis_result(res);
			return (status);
		}
	}
	return (status);
}	

static int
FNSP_org_remove_attribute(const FN_string &tabname,
    const FN_string &aname, const FN_attribute &attribute)
{
	unsigned status, status2;
	nis_result *res;
	char sname[NIS_MAXNAMELEN+1];
	const FN_identifier *id;

	nis_name tablename = (nis_name) tabname.str(&status);
	if (status != FN_SUCCESS)
		return (status);

	// Remove the all the values of the attribute
	id = attribute.identifier();
	sprintf(sname, "[%s=\"%s\",%s=\"%s\"],%s",
	    FNSP_name_col_label, aname.str(&status),
	    FNSP_attrid_col_label, id->str(&status2),
	    tablename);
	if (status != FN_SUCCESS)
		return (status);
	if (status2 != FN_SUCCESS)
		return (status2);

	res = nis_remove_entry(sname, 0, REM_MULTIPLE);

	switch (res->status) {
	case NIS_NOTFOUND:
		status = FN_SUCCESS;
		free_nis_result(res);
		break;
	default:
		status = FNSP_map_result(res, "cannot empty table");
		free_nis_result(res);
		return (status);
	}
	return (status);
}

static int
FNSP_org_search_map(const FN_string &tabname,
    const FN_string &aname,
    const FN_attribute &attr,
    unsigned flags,
    unsigned access_flags,
    unsigned &status,
    const FN_string *cname)
{
	// Make sure the map exists
	char fns_attr_map[NIS_MAXNAMELEN+1];
	char *tablename = (char *) tabname.str(&status);
	if (status != FN_SUCCESS)
		return (0);
	char *domainname = strchr(tablename, '.');
	if (domainname == NULL) {
		status = FN_E_MALFORMED_REFERENCE;
		return (0);
	}
	strcpy(fns_attr_map, FNSP_org_attr_str);
	strcat(fns_attr_map, ++domainname);

	// Check if the table is present
	FN_string t_name((unsigned char *) fns_attr_map);
	status = FNSP_create_attribute_table_base(t_name,
	    access_flags, FN_STRING_CASE_SENSITIVE);
	if (status != FN_SUCCESS)
		return (status);

	// Combine cname and name
	const FN_string comb_name(0, cname, &aname, 0);
	// Mask all the signals
	sigset_t mask_signal, org_signal;
	sigfillset(&mask_signal);
	sigprocmask(SIG_BLOCK, &mask_signal, &org_signal);
	switch (flags) {
	case FN_ATTR_OP_ADD:
		status = FNSP_org_remove_attribute(t_name, comb_name, attr);
		if (status != FN_SUCCESS)
			break;
	case FN_ATTR_OP_ADD_EXCLUSIVE:
	case FN_ATTR_OP_ADD_VALUES:
		status = FNSP_single_map_add_attribute(t_name, comb_name, attr);
		break;
	case FN_ATTR_OP_REMOVE:
		status = FNSP_org_remove_attribute(t_name, comb_name, attr);
		break;
	case FN_ATTR_OP_REMOVE_VALUES:
		status = FNSP_single_map_remove_attribute(t_name, comb_name, attr);
		break;
	default:
		status = FN_E_UNSPECIFIED_ERROR;
		// Error
	}
	sigprocmask(SIG_SETMASK, &org_signal, 0);
	return (status);
}

static int
FNSP_modify_attribute_entry(const FN_string &tabname,
    const FN_string &aname,
    const FN_attribute &attr,
    unsigned flags,
    unsigned access_flags,
    unsigned &status,
    const FN_string *cname)
{
	char sname[NIS_MAXNAMELEN+1];
	nis_object *obj;
	char *savebuf;
	nis_result *res;
	nis_result *res1;
	int savelen;
	void *ip;
	int fns_org_map = 0;

	nis_name tablename = (nis_name) tabname.str(&status);
	if (status != FN_SUCCESS)
		return (0);

	// Check if the map is the fns.ctx_dir map
	if (strstr((char *) tablename, FNSP_org_map))
		fns_org_map = 1;

	// Get the attribute set associated with this entry
	FN_attrset *aset;

	// Get the nis_object associted with this entry and
	// obtain the attribute set
	sprintf(sname, "[%s=\"%s\",%s=\"%s\"],%s",
	    FNSP_name_col_label, aname.str(&status),
	    FNSP_ctx_col_label, cname->str(&status),
	    tablename);

	res = nis_list(sname, MASTER_ONLY|FNSP_nisflags|access_flags,
	    NULL, NULL);
	status = FNSP_map_result(res, "Unable to perform nis_list");
	if (status != FN_SUCCESS) {
		free_nis_result(res);
		return (status);
	}

	aset = FNSP_extract_attrset_result(res, status);
	// Check the status, in case of an error in XDR decode
	if (status != FN_SUCCESS) {
		free_nis_result(res);
		return (status);
	}
	if (aset == 0) {
		if (flags == FN_ATTR_OP_REMOVE ||
		    flags == FN_ATTR_OP_REMOVE_VALUES) {
			free_nis_result(res);
			return (FN_SUCCESS);
		}
		// otherwise create attribute set to work with
		aset = new FN_attrset;
	}

	// Perform the required operation on aset
	switch (flags) {
	case FN_ATTR_OP_ADD:
		if (!aset->add(attr, FN_OP_SUPERCEDE)) {
			free_nis_result(res);
			delete aset;
			return (FN_E_INSUFFICIENT_RESOURCES);
		}
		break;

	case FN_ATTR_OP_ADD_EXCLUSIVE:
		if (!aset->add(attr, FN_OP_EXCLUSIVE)) {
			free_nis_result(res);
			delete aset;
			// %%% should be FN_E_ATTR_IN_USE
			return (FN_E_UNSPECIFIED_ERROR);
		}
		break;

	case FN_ATTR_OP_ADD_VALUES: {
		const FN_identifier *ident = attr.identifier();
		const FN_attribute *old_attr = aset->get(*ident);

		if (old_attr == NULL) {
			if (!aset->add(attr)) {
				free_nis_result(res);
				delete aset;
				return (FN_E_INSUFFICIENT_RESOURCES);
			}
		} else {
			// Check if syntax are the same
			const FN_identifier *syntax = attr.syntax();
			const FN_identifier *old_syntax = old_attr->syntax();
			if ((*syntax) != (*old_syntax)) {
				free_nis_result(res);
				delete aset;
				return (FN_E_INVALID_ATTR_VALUE);
			}
			// merge attr with old_attr
			FN_attribute merged_attr(*old_attr);
			const FN_attrvalue *new_attrval;

			for (new_attrval = attr.first(ip);
			    new_attrval != NULL;
			    new_attrval = attr.next(ip)) {
				merged_attr.add(*new_attrval);
			}
			// overwrite old_attr with merged_attr
			if (!aset->add(merged_attr, FN_OP_SUPERCEDE)) {
				free_nis_result(res);
				delete aset;
				return (FN_E_INSUFFICIENT_RESOURCES);
			}
		}
		break;
	}

	case FN_ATTR_OP_REMOVE:
	case FN_ATTR_OP_REMOVE_VALUES: {
		const FN_identifier *attr_id = attr.identifier();
		const FN_attribute *old_attr = aset->get(*attr_id);

		if (old_attr == NULL) {
			// do not need to update table
			free_nis_result(res);
			delete aset;
			return (FN_SUCCESS);
		}

		if (flags == FN_ATTR_OP_REMOVE)
			aset->remove(*attr_id);
		else {
			// take intersection of attr and old_attr
			FN_attribute inter_attr(*old_attr);

			const FN_attrvalue *attr_value;
			for (attr_value = attr.first(ip);
			    attr_value != NULL;
			    attr_value = attr.next(ip)) {
				inter_attr.remove(*attr_value);
			}
			if (inter_attr.valuecount() <= 0)
				aset->remove(*attr_id);
			else if (!aset->add(inter_attr, FN_OP_SUPERCEDE)) {
				// overwrite
				free_nis_result(res);
				delete aset;
				return (FN_E_INSUFFICIENT_RESOURCES);
			}
		}
		break;
	}

	default:
		free_nis_result(res);
		delete aset;
		return (FN_E_OPERATION_NOT_SUPPORTED);
	}


	obj = res->objects.objects_val;
	savebuf = ENTRY_VAL(obj, FNSP_ATTR_COL);  // save for cleanup
	savelen = ENTRY_LEN(obj, FNSP_ATTR_COL);

	if (FNSP_store_attrset(aset,
	    &(ENTRY_VAL(obj, FNSP_ATTR_COL)),
	    (size_t *) &(ENTRY_LEN(obj, FNSP_ATTR_COL))) == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}
	ENTRY_FLAGS(obj, FNSP_ATTR_COL) = EN_MODIFIED;

	res1 = nis_modify_entry(sname, obj, 0);
	status = FNSP_map_result(res1, "Could not modify attributes");
	free(ENTRY_VAL(obj, FNSP_ATTR_COL));
	free_nis_result(res1);

	if ((status == FN_SUCCESS) && fns_org_map)
		FNSP_org_search_map(tabname, aname,
		    attr, flags, access_flags, status, cname);
cleanup:
	ENTRY_VAL(obj, FNSP_ATTR_COL) = savebuf;  // restore
	ENTRY_LEN(obj, FNSP_ATTR_COL) = savelen;
	free_nis_result(res);
	delete aset;
	return (status);
}

static int
FNSP_modify_attribute_aux(const FNSP_Address &context, const FN_string &aname,
    const FN_attribute &attr, unsigned flags,
    unsigned &status)
{
	switch (context.get_impl_type()) {
	case FNSP_shared_table_impl:
		return (FNSP_modify_attribute_entry(context.get_table_name(),
		    aname, attr, flags,
		    context.get_access_flags(),
		    status,
		    &(FNSP_self_name)));

	case FNSP_entries_impl:
		return (FNSP_modify_attribute_entry(context.get_table_name(),
		    aname, attr, flags,
		    context.get_access_flags(),
		    status,
		    &(context.get_index_name())));

	case FNSP_directory_impl:
	case FNSP_single_table_impl:
	case FNSP_null_impl:
		status = FN_E_OPERATION_NOT_SUPPORTED;
		return (0);

	default:
		status = FN_E_UNSPECIFIED_ERROR;
		return (0);
	}
}

int
FNSP_nisplusImpl::modify_attribute(
    const FN_string &atomic_name,
    const FN_attribute &attr, unsigned flags)
{
	unsigned status = FN_E_UNSPECIFIED_ERROR;

	FNSP_modify_attribute_aux(*my_address, atomic_name, attr, flags,
	    status);

	return (status);
}


static int
FNSP_set_attrset_entry(const FN_string &tabname,
    const FN_string &aname,
    const FN_attrset &attrset,
    unsigned int access_flags,
    unsigned &status,
    const FN_string *cname)
{
	char sname[NIS_MAXNAMELEN+1];
	nis_object *obj;
	char *savebuf;
	nis_result *res, *res2;
	int savelen;

	nis_name tablename = (nis_name) tabname.str(&status);
	if (status != FN_SUCCESS)
		return (0);

	// Get the nis_object associted with this entry and
	// obtain the attribute set
	sprintf(sname, "[%s=\"%s\",%s=\"%s\"],%s",
	    FNSP_name_col_label, aname.str(&status),
	    FNSP_ctx_col_label, cname->str(&status),
	    tablename);

	res = nis_list(sname, MASTER_ONLY|FNSP_nisflags|access_flags,
	    NULL, NULL);
	status = FNSP_map_result(res, "Unable to list in NIS+ table");
	if (status != FN_SUCCESS) {
		free_nis_result(res);
		return (status);
	}

	obj = res->objects.objects_val;
	savebuf = ENTRY_VAL(obj, FNSP_ATTR_COL);  // save for cleanup
	savelen = ENTRY_LEN(obj, FNSP_ATTR_COL);
	if (FNSP_store_attrset(&attrset, &(ENTRY_VAL(obj, FNSP_ATTR_COL)),
	    (size_t *) &(ENTRY_LEN(obj, FNSP_ATTR_COL))) == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}
	ENTRY_FLAGS(obj, FNSP_ATTR_COL) = EN_MODIFIED;

	res2 = nis_modify_entry(sname, obj, 0);
	status = FNSP_map_result(res, "Could not modify attribute set");
	free_nis_result(res2);
	free(ENTRY_VAL(obj, FNSP_ATTR_COL)); // cleanup

cleanup:
	ENTRY_VAL(obj, FNSP_ATTR_COL) = savebuf;  // restore
	ENTRY_LEN(obj, FNSP_ATTR_COL) = savelen;
	free_nis_result(res);
	return (status);
}

static int
FNSP_set_attrset_aux(const FNSP_Address &context, const FN_string &aname,
    const FN_attrset &attrset, unsigned &status)
{
	switch (context.get_impl_type()) {
	case FNSP_shared_table_impl:
		return (FNSP_set_attrset_entry(context.get_table_name(),
		    aname,
		    attrset,
		    context.get_access_flags(),
		    status, &FNSP_self_name));

	case FNSP_entries_impl:
		return (FNSP_set_attrset_entry(context.get_table_name(),
		    aname, attrset,
		    context.get_access_flags(),
		    status, &(context.get_index_name())));

	case FNSP_single_table_impl:
	case FNSP_directory_impl:
	case FNSP_null_impl:
		status = FN_E_OPERATION_NOT_SUPPORTED;
		return (0);

	default:
		status = FN_E_UNSPECIFIED_ERROR;
		return (0);
	}
}

int
FNSP_nisplusImpl::set_attrset(const FN_string &atomic_name,
	const FN_attrset &attrset)
{
	unsigned status = FN_E_UNSPECIFIED_ERROR;

	FNSP_set_attrset_aux(*my_address, atomic_name, attrset, status);

	return (status);
}


FN_attribute*
FNSP_get_attribute(const FNSP_Address &context,
    const FN_string &aname,
    const FN_identifier &id,
    unsigned &status)
{
	char sname[NIS_MAXNAMELEN+1];
	nis_result *res;
	FN_identifier *syntax;
	FN_attribute *answer = 0;

	if (!((context.get_context_type() == FNSP_hostname_context) ||
	    (context.get_context_type() == FNSP_username_context))) {
		status = FN_E_OPERATION_NOT_SUPPORTED;
		return (answer);
	}

	if (context.get_impl_type() != FNSP_single_table_impl) {
		status = FN_E_OPERATION_NOT_SUPPORTED;
		return (answer);
	}

	const unsigned char *id_string = id.str(&status);
	if (status != FN_SUCCESS)
		return (answer);

	const FN_string tabname = context.get_table_name();
	nis_name tablename = (nis_name) tabname.str(&status);
	if (status != FN_SUCCESS)
		return (answer);

	sprintf(sname, "[%s=\"%s\",%s=\"%s\"],%s",
	    FNSP_name_col_label, aname.str(&status),
	    FNSP_attrid_col_label, id_string,
	    tablename);
	if (status != FN_SUCCESS)
		return (answer);
	if (sname[strlen(sname)-1] != '.')
		strcat(sname, ".");

	// Get the syntax value
	res = nis_list(sname, FNSP_nisflags|context.get_access_flags(),
	    NULL, NULL);
	if ((res->status == NIS_NOTFOUND) ||
	    (res->status == NIS_NOSUCHNAME) ||
	    (res->status == NIS_NOSUCHTABLE))
		status = FN_E_NO_SUCH_ATTRIBUTE;
	else
		status = FNSP_map_result(res, 0);
	if (status != FN_SUCCESS) {
		free_nis_result(res);
		return (answer);
	}

	// Determine the number of attribtue values obtained
	unsigned int num_attr_values = res->objects.objects_len;
	if (num_attr_values == 0) {
		// No attribtue values associates with the identifier
		status = FN_E_NO_SUCH_ATTRIBUTE;
		free_nis_result(res);
		return (0);
	}

	u_short *format;
	unsigned int id_format;
	int syntax_is_ascii;
	char *encoded_data;
	void *decoded_data;
	size_t decoded_data_len;

	// Obtain the syntax identifier
	format = (u_short *) ENTRY_VAL(res->objects.objects_val,
	    FNSP_ATTR_SYNTAX_FORMAT_COL);
	id_format = (unsigned int) ntohs(*format);
	syntax = new FN_identifier(id_format,
	    (size_t)
	    ENTRY_LEN(res->objects.objects_val, FNSP_ATTR_SYNTAX_COL) - 1,
	    (const unsigned char *)
	    ENTRY_VAL(res->objects.objects_val, FNSP_ATTR_SYNTAX_COL));
	if (syntax == 0) {
		free_nis_result(res);
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	answer = new FN_attribute(id, *syntax);
	if (strncmp((char *) syntax->contents(),
	    ascii_syntax, strlen(ascii_syntax)) == 0)
		syntax_is_ascii = 1;
	else
		syntax_is_ascii = 0;
	delete syntax;
	if (answer == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		free_nis_result(res);
		return (0);
	}

	// Get the attribute values and add them to the attribute
	FN_attrvalue *value;
	for (int i = 0; i < num_attr_values; i++) {
		// Obtain only the correct id_format
		format = (u_short *) ENTRY_VAL(&res->objects.objects_val[i],
		    FNSP_ATTR_ID_FORMAT_COL);
		id_format = (unsigned int) ntohs(*format);
		if (id_format == id.format()) {
			if (syntax_is_ascii) {
				value = new FN_attrvalue((unsigned char *)
				    ENTRY_VAL(&res->objects.objects_val[i],
				    FNSP_ATTR_VALUE_COL));
			} else {
				encoded_data = ENTRY_VAL(
				    &res->objects.objects_val[i],
				    FNSP_ATTR_VALUE_COL);
				decoded_data = FNSP_decode(
				    encoded_data, (int *) &decoded_data_len,
				    status);
				if (status != FN_SUCCESS) {
					delete answer;
					break;
				}
				if (decoded_data) {
					value = new FN_attrvalue(
					    decoded_data, decoded_data_len);
					free(decoded_data);
				}
			}
			if (value == 0) {
				status = FN_E_INSUFFICIENT_RESOURCES;
				delete answer;
				break;
			} else {
				answer->add(*value);
				delete value;
			}
		}
	}

	if ((answer) && (answer->valuecount() == 0)) {
		delete answer;
		status = FN_E_NO_SUCH_ATTRIBUTE;
		answer = 0;
	}
	free_nis_result(res);
	return (answer);
}

/* used in fnsp_internal */
int
FNSP_create_attribute_table_base(const FN_string &tabname,
    unsigned int access_flags,
    unsigned string_case)
{
	unsigned status;
	char group[NIS_MAXNAMELEN+1];
	nis_name tablename = (nis_name) tabname.str(&status);
	if (status != FN_SUCCESS)
		return (FN_E_INSUFFICIENT_RESOURCES);

	table_col tcols[FNSP_ATTR_TABLE_WIDTH];
	nis_object tobj;
	nis_result *res;

	/* first check whether table exists */
	res = nis_lookup(tablename, MASTER_ONLY|FNSP_nisflags|access_flags);
	status = FNSP_map_result(res, 0);
	free_nis_result(res);
	if (status == FN_SUCCESS)
		return (FN_SUCCESS);

	tcols[FNSP_NAME_COL].tc_name = (char *) FNSP_name_col_label;
	tcols[FNSP_NAME_COL].tc_flags = TA_SEARCHABLE;
	tcols[FNSP_NAME_COL].tc_rights = FNSP_DEFAULT_RIGHTS;
	if (string_case == FN_STRING_CASE_INSENSITIVE)
		tcols[FNSP_NAME_COL].tc_flags |= TA_CASE;

	tcols[FNSP_ATTR_ID_COL].tc_name = (char *) FNSP_attrid_col_label;
	tcols[FNSP_ATTR_ID_COL].tc_flags = TA_SEARCHABLE;
	tcols[FNSP_ATTR_ID_COL].tc_rights = FNSP_DEFAULT_RIGHTS;
	if (string_case == FN_STRING_CASE_INSENSITIVE)
		tcols[FNSP_ATTR_ID_COL].tc_flags |= TA_CASE;

	tcols[FNSP_ATTR_ID_FORMAT_COL].tc_name =
	    (char *) FNSP_attridformat_col_label;
	tcols[FNSP_ATTR_ID_FORMAT_COL].tc_flags = TA_BINARY;
	tcols[FNSP_ATTR_ID_FORMAT_COL].tc_rights = FNSP_DEFAULT_RIGHTS;

	tcols[FNSP_ATTR_VALUE_COL].tc_name = (char *) FNSP_attrvalue_col_label;
	tcols[FNSP_ATTR_VALUE_COL].tc_flags = TA_SEARCHABLE;
	tcols[FNSP_ATTR_VALUE_COL].tc_rights = FNSP_DEFAULT_RIGHTS;
	if (string_case == FN_STRING_CASE_INSENSITIVE)
		tcols[FNSP_ATTR_VALUE_COL].tc_flags |= TA_CASE;

	tcols[FNSP_ATTR_SYNTAX_COL].tc_name = (char *)
	    FNSP_attrsyntax_col_label;
	tcols[FNSP_ATTR_SYNTAX_COL].tc_flags = 0;
	tcols[FNSP_ATTR_SYNTAX_COL].tc_rights = FNSP_DEFAULT_RIGHTS;

	tcols[FNSP_ATTR_SYNTAX_FORMAT_COL].tc_name =
	    (char *) FNSP_attrsyntaxformat_col_label;
	tcols[FNSP_ATTR_SYNTAX_FORMAT_COL].tc_flags = TA_BINARY;
	tcols[FNSP_ATTR_SYNTAX_FORMAT_COL].tc_rights = FNSP_DEFAULT_RIGHTS;

	tobj.zo_owner = nis_local_principal();
	// Get the default group name
	FNSP_nisplus_admin_group(tablename, group);
	tobj.zo_group = (nis_name) group;
	tobj.zo_access = FNSP_DEFAULT_RIGHTS|FNSP_TABLE_CREATE_RIGHTS;
	tobj.zo_data.zo_type = NIS_TABLE_OBJ;
	tobj.TA_data.ta_type = (char *) __nis_default_table_type;
	tobj.TA_data.ta_maxcol = FNSP_ATTR_TABLE_WIDTH;
	tobj.TA_data.ta_sep = __nis_default_separator;
	tobj.TA_data.ta_path = (char *) __nis_default_path;
	tobj.TA_data.ta_cols.ta_cols_len = FNSP_ATTR_TABLE_WIDTH;
	tobj.TA_data.ta_cols.ta_cols_val = tcols;

	// add table to name space
	res = nis_add(tablename, &tobj);
	status = FNSP_map_result(res, "could not create attribute table");
	free_nis_result(res);
	return (status);
}

int
FNSP_set_attribute(const FNSP_Address &context,
    const FN_string &aname,
    const FN_attribute &attribute)
{
	unsigned status;
	const FN_string tabname = context.get_table_name();

	// Check if the table exists, if not create one
	status = FNSP_create_attribute_table_base(tabname,
	    context.get_access_flags(),
	    FN_STRING_CASE_SENSITIVE);
	if (status != FN_SUCCESS)
		return (status);

	// Add the attibute values to the table
	return (FNSP_single_map_add_attribute(tabname, aname, attribute, 1));
}

int
FNSP_remove_attribute_values(const FNSP_Address &context,
    const FN_string &aname,
    const FN_attribute &attribute)
{
	const FN_string tabname = context.get_table_name();
	return (FNSP_single_map_remove_attribute(tabname, aname, attribute));
}

int
FNSP_remove_attribute(const FNSP_Address &context,
    const FN_string &aname,
    const FN_attribute *attribute)
{
	unsigned status, status2;
	nis_result *res;
	char sname[NIS_MAXNAMELEN+1];
	const FN_identifier *id;

	const FN_string tabname = context.get_table_name();
	nis_name tablename = (nis_name) tabname.str(&status);
	if (status != FN_SUCCESS)
		return (status);

	if (attribute) {
		// Remove the values of all the specified
		// attribute
		id = attribute->identifier();
		sprintf(sname, "[%s=\"%s\",%s=\"%s\"],%s",
		    FNSP_name_col_label, aname.str(&status),
		    FNSP_attrid_col_label, id->str(&status2),
		    tablename);
		if (status != FN_SUCCESS)
			return (status);
		if (status2 != FN_SUCCESS)
			return (status2);
	} else {
		// Remove all the attributes associated with the
		// atomic name
		sprintf(sname, "[%s=\"%s\"],%s",
		    FNSP_name_col_label, aname.str(&status),
		    tablename);
		if (status != FN_SUCCESS)
			return (status);
	}

	res = nis_remove_entry(sname, 0, REM_MULTIPLE);

	switch (res->status) {
	case NIS_NOTFOUND:
		status = FN_SUCCESS;
		free_nis_result(res);
		break;
	default:
		status = FNSP_map_result(res, "cannot empty table");
		free_nis_result(res);
		return (status);
	}
	return (status);
}
