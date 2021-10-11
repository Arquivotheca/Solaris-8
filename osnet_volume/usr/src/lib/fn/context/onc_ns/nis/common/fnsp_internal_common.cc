/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnsp_internal_common.cc	1.21	97/10/24 SMI"

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <ctype.h>
#include <xfn/xfn.hh>
#include <xfn/fn_xdr.hh>
#include <xfn/FN_nameset.hh>
#include <xfn/FN_bindingset.hh>
#include <xfn/fn_p.hh>
#include "fnsp_internal_common.hh"
#include "fnsp_utils.hh"

#define	COMMON_SIZE 1024UL
#define	COMMON_INDEX 256
#define DBM_LIMIT 500
#define DBM_MIN	  24

static const FN_string FNSP_nis_table_suffix((unsigned char *) ".ctx.");
static const FN_string FNSP_nis_pwho_suffix((unsigned char *) ".byname.");
static const char *FNSP_sub_context_sept = "_#";
static const char FNSP_sub_ctx_escape = '/';

// ---------------------------------------------
// Routine to parse the input buffer based on
// the token supplied. Similar to strtok().
// ---------------------------------------------
char *
strparse(char *inbuf, const char *ptr, char **temp)
{
	if ((inbuf == 0) && (*temp == 0))
		return (0);

	char *outbuf;
	if (inbuf)
		outbuf = inbuf;
	else
		outbuf = *temp;
	char *mv = outbuf;

	size_t i = 0, j;
	while (1) {
		for (; (mv[i] != ptr[0]); i++)
			if (i == strlen(outbuf)) {
				*temp = 0;
				return (outbuf);
			}
		if (strncasecmp(&mv[i], ptr, strlen(ptr)) == 0) {
			for (j = 0; j < strlen(ptr); j++)
				mv[i+j] = '\0';
			*temp = &mv[i + strlen(ptr)];
			return (outbuf);
		} else
			i++;
	}
}

int FNSP_match_map_index(const char *line,
    const char *name)
{
	size_t len;

	len = strlen(name);
	return ((strncasecmp(line, name, len) == 0) &&
		(line[len] == ' ' || line[len] == '\t'));
}

void
FNSP_legalize_name(char *name)
{
	// Parse the string to check for FNSP_INTERNAL_NAME_SEP
	// If present add an extra FNSP_INTERNAL_NAME_SEP
	size_t i, j;
	int count = 0;
	char internal_name = FNSP_INTERNAL_NAME_SEP[0];
	for (i = 0; i < strlen(name); i++)
		if (name[i] == internal_name)
			count++;
	if (count == 0)
		return;

	char answer[COMMON_INDEX];
	for (i = 0, j = 0; i < strlen(name); i++, j++) {
		answer[j] = name[i];
		if (name[i] == internal_name) {
			j++;
			answer[j] = internal_name;
		}
	}
	answer[strlen(name) + count] = '\0';
	strcpy(name, answer);
}

// This routine to be used only by host and user contexts
// to get back the host and user names
void
FNSP_normalize_name(char *name)
{
	// Parse the string to check for two FNSP_INTERNAL_NAME_SEP.
	// If present, replace with one FNSP_INTERNAL_NAME_SEP
	size_t i, j;
	int count = 0;
	size_t name_len = strlen(name);
	char internal_name = FNSP_INTERNAL_NAME_SEP[0];
	for (i = 0; i < (name_len - 1); i++)
		if ((name[i] == internal_name) &&
		    (name[i+1] == internal_name))
			count++;
	if (count == 0)
		return;

	char answer[COMMON_INDEX];
	for (i = 0, j = 0; i < name_len; i++, j++) {
		answer[j] = name[i];
		if ((name[i] == internal_name) &&
		    (i < (name_len -1 )) &&
		    (name[i+1] == internal_name))
			i++;
	}
	answer[strlen(name) - count] = '\0';
	strcpy(name, answer);
}

char *
FNSP_check_if_subcontext(const char *parent_index, const char *fullname)
{
	size_t len = strlen(fullname);
	if (len < strlen(parent_index))
		return (0);
	if (strncmp(parent_index, fullname, strlen(parent_index)) != 0)
		return (0);
	if ((fullname[len-1] != FNSP_BIND_SUFFIX[1]) ||
	    (fullname[len-2] != FNSP_BIND_SUFFIX[0]))
		return (0);

	// Count the number of FNSP_INTERNAL_NAME_SEP
	const char *ptr = &fullname[strlen(parent_index) + 1];
	if ((len = strlen(ptr)) < 2)
		return (0);
	for (size_t i = 0; i < len - 2; i++) {
		if (ptr[i] == FNSP_INTERNAL_NAME_SEP[0]) {
			if (ptr[i+1] == FNSP_INTERNAL_NAME_SEP[0])
				i++;
			else
				return (0);
		}
	}
	char *answer = (char *) calloc(len - 1, sizeof(char));
	strncpy(answer, ptr, len - 2);
	return (answer);
}

static char *
FNSP_legalize_ctx_name(const char *name)
{
	// Parse the string to check for FNSP_sub_context_sept
	// If present add FNSP_sub_ctx_escape
	size_t i,j;
	int count = 0;
	char *answer;

	// Start checking for "_#" 
	for (i = 0; i < strlen(name); i++) {
		if (name[i] == FNSP_sub_context_sept[0]) {
			i++;
			while (name[i] == FNSP_sub_ctx_escape)
				i++;
			if (name[i] == FNSP_sub_context_sept[1])
				count++;
		}
	}
	if (count == 0)
		return (strdup(name));

	// Create the new subcontext name
	answer = (char *) malloc(strlen(name) + count + 1);
	if (answer == 0)
		return (0);
	for (i = 0, j = 0; i < strlen(name); i++, j++) {
		answer[j] = name[i];
		if (name[i] == FNSP_sub_context_sept[0]) {
			i++; j++;
			while (name[i] == FNSP_sub_ctx_escape) {
				answer[j] = name[i];
				i++; j++;
			}
			if (name[i] == FNSP_sub_context_sept[1]) {
				answer[j] = FNSP_sub_ctx_escape;
				j++;
			}
			answer[j] = name[i];
		}
	}
	answer[strlen(name) + count] = '\0';
	return (answer);
}

static char *
FNSP_get_real_ctx_name(const char *name)
{
	// Parse the string to check for FNSP_sub_context_sept
	// If present delete an extra FNSP_sub_ctx_escape
	size_t i,j;
	int count = 0;
	char *answer;

	// Start checking for "_#" 
	for (i = 0; i < strlen(name); i++) {
		if (name[i] == FNSP_sub_context_sept[0]) {
			i++;
			while (name[i] == FNSP_sub_ctx_escape)
				i++;
			if (name[i] == FNSP_sub_context_sept[1])
				count++;
		}
	}
	if (count == 0)
		return (strdup(name));

	// Create the real subcontext name
	answer = (char *) malloc(strlen(name) - count + 1);
	if (answer == 0)
		return (0);
	for (i = 0, j = 0; i < strlen(name); i++, j++) {
		answer[j] = name[i];
		if (name[i] == FNSP_sub_context_sept[0]) {
			i++; j++;
			while (name[i] == FNSP_sub_ctx_escape) {
				answer[j] = name[i];
				i++; j++;
			}
			if (name[i] == FNSP_sub_context_sept[1])
				j--;
			answer[j] = name[i];
		}
	}
	answer[strlen(name) - count] = '\0';
	return (answer);
}

// ----------------------------------------
// Routines to serialize and de-serialize
// sub-contexts of a context ie., list
// elements of a context
// ----------------------------------------
char *
FNSP_nis_sub_context_serialize(const FN_nameset *sub_contexts,
    unsigned &status)
{
	char nis_buffer[COMMON_SIZE];
	char *context, *sub_ctx_name;
	void *ip;
	const FN_string *ctx_name;
	size_t size = 0;

	if ((sub_contexts == 0) ||
	    (sub_contexts->count() == 0)) {
		context = (char *) malloc(strlen(FNSP_sub_context_sept)
		    + 1);
		if (context) {
			status = FN_SUCCESS;
			strcpy(context, FNSP_sub_context_sept);
		} else
			status = FN_E_INSUFFICIENT_RESOURCES;
		return (context);
	}

	memset(nis_buffer, 0, COMMON_SIZE);
	for (ctx_name = sub_contexts->first(ip);
	    ctx_name != 0;
	    ctx_name = sub_contexts->next(ip)) {
		sub_ctx_name = FNSP_legalize_ctx_name(
		    (char *) ctx_name->str());
		size += strlen(sub_ctx_name) + 2;
		if (size < COMMON_SIZE) {
			strcat(nis_buffer, sub_ctx_name);
			strcat(nis_buffer, FNSP_sub_context_sept);
		}
		free (sub_ctx_name);
	}
	context = (char *) malloc(size + 1);
	if (context) {
		status = FN_SUCCESS;
		if (size < COMMON_SIZE)
			strcpy(context, nis_buffer);
		else {
			for (ctx_name = sub_contexts->first(ip);
			    ctx_name != 0;
			    ctx_name = sub_contexts->next(ip)) {
				sub_ctx_name = FNSP_legalize_ctx_name(
				    (char *) ctx_name->str());
				size += strlen(sub_ctx_name) + 2;
				strcat(context, sub_ctx_name);
				strcat(context, FNSP_sub_context_sept);
				free (sub_ctx_name);
			}
		}
	} else
		status = FN_E_INSUFFICIENT_RESOURCES;


	return (context);
}

FN_nameset *
FNSP_nis_sub_context_deserialize(char *buffer, unsigned &status)
{
	FN_string *string;
	char *ctx_name, *real_name;
	FN_nameset *nameset = new FN_nameset;
	if (nameset == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	} else
		status = FN_SUCCESS;

	char *temp;
	ctx_name = strparse(buffer, FNSP_sub_context_sept, &temp);
	while ((ctx_name) && (ctx_name[0] != '\0')) {
		if (!iscntrl(ctx_name[strlen(ctx_name) - 1])) {
			real_name = FNSP_get_real_ctx_name(ctx_name);
			string = new FN_string((unsigned char *) real_name);
			if (string == 0) {
				delete nameset;
				status = FN_E_INSUFFICIENT_RESOURCES;
				return (0);
			}
			nameset->add(*string);
			free (real_name);
			delete string;
		}
		ctx_name = strparse(0, FNSP_sub_context_sept, &temp);
	}
	return (nameset);
}


// -------------------------------------------------------
// Serializing and deserializing the binding information.
// Has to take into account the binary nature of the
// reference.
// -------------------------------------------------------
static const char *FNSP_nis_reference = FNSP_NIS_REFERENCE;
static const char *FNSP_nis_context = FNSP_NIS_CONTEXT;
static const char *FNSP_nis_hu_reference = FNSP_NIS_HU_REFERENCE;
static const char *FNSP_nis_hu_context = FNSP_NIS_HU_CONTEXT;

char *
FNSP_nis_binding_serialize(const FN_ref &ref,
    FNSP_binding_type btype, unsigned &status)
{
	char nis_buffer[COMMON_SIZE];
	char *binding, *refbuf, *data, temp_buf[COMMON_SIZE];
	int len;
	size_t size;

	if (nis_buffer == 0) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	memset((void *) nis_buffer, 0, COMMON_SIZE);

	// Determine if it is a binding or context type
	switch (btype) {
	case FNSP_bound_reference:
		strcpy(nis_buffer, FNSP_nis_reference);
		break;
	case FNSP_child_context:
		strcpy(nis_buffer, FNSP_nis_context);
		break;
	case FNSP_hu_reference:
		strcpy(nis_buffer, FNSP_nis_hu_reference);
		break;
	case FNSP_hu_context:
		strcpy(nis_buffer, FNSP_nis_hu_context);
		break;
	default:
		status = FN_E_CONFIGURATION_ERROR;
		return (0);
	}

	strcat(nis_buffer, FNSP_sub_context_sept);

	// Get the reference in the serial form
	refbuf = FN_ref_xdr_serialize(ref, len);
	if (refbuf == NULL) {
		status = FN_E_CONFIGURATION_ERROR;
		return (0);
	}

	// Append the length to nis_buffer
	sprintf(temp_buf, "%d", len);
	strcat(nis_buffer, temp_buf);
	strcat(nis_buffer, FNSP_sub_context_sept);

	// Encode the xdr-ed reference
	data = FNSP_encode_binary(refbuf, len, status);
	free(refbuf);
	if (status != FN_SUCCESS)
		return (0);

	// malloc for the binding information
	size = strlen(nis_buffer) + strlen(data) + 1;
	binding = (char *) malloc(size + 1);
	if (binding) {
		status = FN_SUCCESS;
		// memset((void *) binding, 0, size+1);
		strcpy(binding, nis_buffer);
		strcat(binding, data);
	} else
		status = FN_E_INSUFFICIENT_RESOURCES;
	free(data);
	return (binding);
}

FN_ref *FNSP_nis_binding_deserialize(char *buf, int /* buflen */,
    FNSP_binding_type &btype, unsigned &status)
{
	FN_ref *ref;
	char *data;
	void *xdr_data;
	int len;

	// Remove the control char, if it exists
	if (iscntrl(buf[strlen(buf) - 1]))
		buf[strlen(buf) - 1] = '\0';

	// Obtain the binding type
	char *temp;
	data = strparse(buf, FNSP_sub_context_sept, &temp);
	if (strcmp(data, FNSP_nis_reference) == 0)
		btype = FNSP_bound_reference;
	else if (strcmp(data, FNSP_nis_context) == 0)
		btype = FNSP_child_context;
	else if (strcmp(data, FNSP_nis_hu_context) == 0)
		btype = FNSP_hu_context;
	else if (strcmp(data, FNSP_nis_hu_reference) == 0)
		btype = FNSP_hu_reference;
	else {
		status = FN_E_CONFIGURATION_ERROR;
		return (0);
	}

	// Get the length of the xdr-ed reference
	data = strparse(0, FNSP_sub_context_sept, &temp);
	len = atoi(data);
	if (len <= 0) {
		status = FN_E_CONFIGURATION_ERROR;
		return (0);
	}

	// Get the uuencoded reference
	data = temp;
	xdr_data = FNSP_decode_binary(data, len, status);
	if (status != FN_SUCCESS)
		return (0);
	ref = FN_ref_xdr_deserialize(
	    (char *) xdr_data, len, status);
	free(xdr_data);
	if (status != FN_SUCCESS)
		return (0);
	return (ref);
}

// ----------------------------------------------------------
// Routines to decompose_index_name used by FNSP_Address.cc
// which decomposes the whole name into index part and
// table name part.
// The other routine is the split_internal_name, which is
// defined static (ie., local) and is used to split the
// table name to give the table name and domain name.
// ----------------------------------------------------------
static const FN_string open_bracket((unsigned char *)"[");
static const FN_string close_bracket((unsigned char *)"]");
static const FN_string quote_string((unsigned char *)"\"");
static const FN_string comma_string((unsigned char *)",");

/*
 * xxx.ctx.yyy -> map = xxx.ctx, domain = yyy
 * xxx.byname.yyy -> map = xxx.byname, domain = yyy
 * yyy -> map = 0, domain = yyy
 * (e.g. no '.fns.' in given string).
 */

unsigned
FNSP_nis_split_internal_name(const FN_string &wholename,
    FN_string **map, FN_string **domain)
{
	int c_start = wholename.next_substring(FNSP_nis_table_suffix);
	if (c_start == FN_STRING_INDEX_NONE) {
		c_start = wholename.next_substring(FNSP_nis_pwho_suffix);
		if (c_start == FN_STRING_INDEX_NONE) {
			// Must be complete domain name
			(*map) = new FN_string((unsigned char *) "");
			(*domain) = new FN_string(wholename);
			return (FN_SUCCESS);
		}
		c_start += strlen((char *) FNSP_nis_pwho_suffix.str());
	} else
		c_start += strlen((char *) FNSP_nis_table_suffix.str());

	*domain = new FN_string(wholename,
	    c_start, wholename.charcount() - 1);
	if (*domain == 0) {
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	if (c_start == 0)
		*map = new FN_string((unsigned char *) "");
	else {
		*map = new FN_string(wholename, 0, c_start-2);
		if (*map == 0) {
			delete *domain;
			*domain = 0;
			return (FN_E_INSUFFICIENT_RESOURCES);
		}
	}
	return (FN_SUCCESS);
}

static inline int
FNSP_table_name_p(const FN_string &name)
{
	return (name.next_substring(open_bracket) == 0);
}

// If name is of form "[<index_part>]<table_part>",
// set tab to table_part and ind to index_part.
// Otherwise, set 'tab' to entire 'name'.
int FNSP_decompose_nis_index_name(const FN_string &name,
    FN_string &tab, FN_string &ind)
{
	if (FNSP_table_name_p(name)) {
		int pos = name.next_substring(close_bracket);
		if (pos > 0) {
			int tabstart = pos + 1;
			int tabend = name.charcount()-1;

			ind = FN_string(name, 1, pos-1);

			// get rid of comma
			if (name.compare_substring(tabstart, tabstart,
			    comma_string) == 0)
				++tabstart;
			tab = FN_string(name, tabstart, tabend);
			return (1);
		}
	}
	// default
	tab = name;
	return (0);
}

char *FNSP_nis_attrset_serialize(const FN_attrset &attrset,
    unsigned &status)
{
	char *attrbuf, *data, temp_buf[COMMON_INDEX];
	char *nis_buffer;
	int len;
	size_t size;

	attrbuf = FN_attr_xdr_serialize(attrset, len);
	if (attrbuf == NULL) {
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	data = FNSP_encode_binary(attrbuf, len, status);
	free(attrbuf);
	if (status != FN_SUCCESS) {
		return (0);
	}

	// Calculate the size of the buffer required
	sprintf(temp_buf, "%d", len);
	size = strlen(temp_buf) + strlen(FNSP_sub_context_sept)
	    + strlen(data);
	nis_buffer = (char *) malloc(size + 1);
	if (nis_buffer == 0) {
		free(data);
		status = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	// memset((void *) nis_buffer, 0, size+1);
	// Copy the data into the buffer
	strcpy(nis_buffer, temp_buf);
	strcat(nis_buffer, FNSP_sub_context_sept);
	strcat(nis_buffer, data);
	free(data);

	return (nis_buffer);
}

FN_attrset *
FNSP_nis_attrset_deserialize(char *attrbuf, int,
    unsigned &status)
{
	char *data, *temp;
	void *xdr_data;
	int len;

	// Get the length of the attrset buffer
	data = strparse(attrbuf, FNSP_sub_context_sept, &temp);
	len = atoi(data);

	data = temp;
	xdr_data = FNSP_decode_binary(data, len, status);
	if (status != FN_SUCCESS)
		return (0);
	FN_attrset *attrset = FN_attr_xdr_deserialize(
	    (char *) xdr_data, len, status);
	free(xdr_data);
	if (status != FN_SUCCESS)
		return (0);
	return (attrset);
}

void
FNSP_construct_local_name(unsigned string_case, char *name)
{
	if (string_case != FN_STRING_CASE_SENSITIVE)
		for (int i = 0; i < strlen(name); i++)
			if (isupper(name[i]))
				name[i] = tolower(name[i]);
}

extern "C"
unsigned
FNSP_hash_index_value(const char *attr_index, char *answer);

unsigned
FNSP_get_first_index_data(FNSP_map_operation op,
    const char *index, const void *data,
    char *new_index, void *new_data, size_t &length,
    char *next_index)
{
	// Make sure the index is below DBM_LIMIT - DBM_MIN
	if (strlen(index) > (DBM_LIMIT - DBM_MIN))
		FNSP_hash_index_value(index, new_index);
	else
		strcpy(new_index, index);

	// If the operation is FNSP_map_delete,
	// update only the next_index and return
	strcpy(next_index, new_index);
	strcat(next_index, "_0");
	if (op == FNSP_map_delete)
		return (1);

	// Make sure the length of key and data does not
	// exceed DBM_LIMIT bytes
	length = strlen((char *) data);
	if ((strlen(index) + length) < (DBM_LIMIT - DBM_MIN)) {
		strcpy((char *) new_data, (char *) data);
		return (FN_SUCCESS);
	}

	// Copy maximum data possible
	length = DBM_LIMIT - strlen(new_index) - DBM_MIN;
	strncpy((char *) new_data, (char *) data, length);
	((char *) new_data)[length] = '\0';

	return (FN_SUCCESS);
}

unsigned
FNSP_get_next_index_data(FNSP_map_operation op,
    const char * /* index */, const void *data,
    char *new_index, void *new_data, size_t &length,
    char *next_index)
{
	// Copy the next_index to new_index
	strcpy(new_index, next_index);

	// Increment the next_index
	char num[COMMON_INDEX];
        int map_num;
        size_t i = strlen(next_index) - 1;
        while (next_index[i] != '_') i--;
        strcpy(num, &next_index[i+1]);
        map_num = atoi(num) + 1;
	sprintf(&next_index[i+1], "%d", map_num);

	// If the operation is FNSP_map_delete return
	if (op == FNSP_map_delete)
		return (FN_SUCCESS);
	else if (length >= strlen((char *) data))
		return (FN_E_NAME_NOT_FOUND);

	// Copy the remaining data
	size_t rem_data = strlen((char *) data) - length;
	size_t max_length = DBM_LIMIT - strlen(new_index) - DBM_MIN;
	if (rem_data < max_length) {
		// Data can fit in the current buffer
		strcpy((char *) new_data,
		    (char *) &(((char *) data)[length]));
		length = strlen((char *) data);
	} else {
		strncpy((char *) new_data,
		    (char *) &(((char *) data)[length]),
		    max_length);
		length += max_length;
		((char *) new_data)[max_length] = '\0';
	}
	return (FN_SUCCESS);
}

unsigned
FNSP_get_first_lookup_index(const char *map_index,
    char *new_index, char *next_index)
{
	if (strlen(map_index) > (DBM_LIMIT - DBM_MIN))
		FNSP_hash_index_value(map_index, new_index);
	else
		strcpy(new_index, map_index);
	strcpy(next_index, new_index);
	strcat(next_index, "_0");
	return (FN_SUCCESS);
}

unsigned
FNSP_get_next_lookup_index(char *new_index, char *next_index,
    char * /* mapentry */, int maplen)
{
	// Check if next lookup is required
	if ((maplen + strlen(new_index)) <
	    (DBM_LIMIT - 2*DBM_MIN))
		return (FN_E_NAME_NOT_FOUND);
 
	strcpy(new_index, next_index);
	// Increment the next_index
	char num[COMMON_INDEX];
        int map_num;
        size_t i = strlen(next_index) - 1;
        while (next_index[i] != '_') i--;
        strcpy(num, &next_index[i+1]);
        map_num = atoi(num) + 1;
	sprintf(&next_index[i+1], "%d", map_num);

	return (FN_SUCCESS);
}
