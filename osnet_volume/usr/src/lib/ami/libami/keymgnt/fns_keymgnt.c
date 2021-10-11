/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)fns_keymgnt.c	1.1 99/07/11 SMI"

#include <jni.h>
#include <wait.h>
#include <thread.h>
#include <synch.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/systeminfo.h>
#include <xfn/xfn.h>
#include <arpa/inet.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/nis.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFNS.h"
#include "com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFNS_0005fEnumeration.h"
#include "com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFILE.h"

/* Backend name services to be used */
#define	AMI_UNKNOWN_NS	0
#define	AMI_NISPLUS_NS	1
#define	AMI_NIS_NS	2
#define	AMI_FILES_NS	3

/* FNS attribute operations */
#define	AMI_ATTR_ADD	1
#define	AMI_ATTR_MODIFY	2
#define	AMI_ATTR_DELETE	3

/* Constants */
const char *attr_id_syntax = "fn_attr_syntax_ascii";
const char *attr_id_syntax_bin = "fn_attr_syntax_binary";


/* Static variables */
/*
* static int source_ns = AMI_FILES_NS; static int source_ns_installed = 0;
*
* static mutex_t initialization_lock = DEFAULTMUTEX; static FN_ctx_t
* *initial_context; static FN_ctx_t *user_context;
*/

/* a linked list for attribute values */
typedef struct _fns_item {
	void *data;
	size_t data_len;
	struct _fns_item *next;
} fns_item;

extern FN_ctx_t *
_fn_ctx_handle_from_initial_with_ns(int ns,
unsigned int authoritative, FN_status_t * s);

/*
* install_fns()
*
* This function execs fncreate to install FNS
*
		* Return 1	SUCCESS 0	FAILURE
*/

static int
install_fns(int source_ns)
{
	pid_t child;
	struct stat buffer;
	int child_status, rename_fnselect = 0;

	/* Rename the /etc/fn.conf file */
	if (stat("/etc/fn.conf", &buffer) == 0) {
		if (rename("/etc/fn.conf", "/etc/fn.conf.bak") < 0)
			return (0);
		rename_fnselect = 1;
	}
	if ((child = fork()) == -1) {
		/* Error in forking */
		return (0);
	}
	if (child == 0) {
		/* Child process */
		if ((child = fork()) == 1) {
			/* Error in forking */
			return (0);
		}
		if (child == 0) {
			if (source_ns == AMI_NISPLUS_NS)
				execl("/usr/sbin/fnselect", "fnselect",
				"nisplus", (char *) 0);
			else if (source_ns == AMI_NIS_NS)
				execl("/usr/sbin/fnselect", "fnselect",
				"nis", (char *) 0);
			else if (source_ns == AMI_FILES_NS)
				execl("/usr/sbin/fnselect", "fnselect",
				"files", (char *) 0);
			/* Should not return, hence... */
			return (0);
		}
		wait(&child_status);
		execl("/usr/sbin/fncreate", "fncreate", "-t",
		"org", "-o", "org//", (char *) 0);
		/* Should not return, hence... */
		return (0);
	}
	wait(&child_status);
	if (rename_fnselect) {
		if (rename("/etc/fn.conf.bak", "/etc/fn.conf") < 0)
			return (0);
	} else {
		unlink("/etc/fn.conf");
	}
	return (1);
}

/*
* fns_get_default_naming_service()
*
* Function to get the default naming service
*
*/
static int
fns_get_default_naming_service()
{
	char domain[NIS_MAXNAMELEN + 1];
	nis_result *res;
	char *nis_master;

	/* Check for NISPLUS */
	sprintf(domain, "org_dir.%s", nis_local_directory());
	res = nis_lookup(domain, NO_AUTHINFO | USE_DGRAM);
	if ((res) && (res->status == NIS_SUCCESS)) {
		nis_freeresult(res);
		return (AMI_NISPLUS_NS);
	}
	if (res)
		nis_freeresult(res);

	/* Check for NIS */
	if ((sysinfo(SI_SRPC_DOMAIN, domain, NIS_MAXNAMELEN) > 0) &&
	(yp_bind(domain) == 0)) {
		if (yp_master(domain, "passwd.byname", &nis_master) == 0) {
			free(nis_master);
			yp_unbind(domain);
			return (AMI_NIS_NS);
		}
		yp_unbind(domain);
	}
	return (AMI_FILES_NS);
}

/*
* fns_get_context()
*
* Function sets the initial context and the context to populate sub-context
* entries with.
*
*/
static FN_ctx_t *
fns_get_context(int source_ns, const char *ctxPrefix)
{
	FN_status_t *status;
	FN_composite_name_t *name;
	FN_ref_t *ref;
	FN_ctx_t *initial_context, *context;
	struct stat buffer;
	char domain[NIS_MAXNAMELEN + 1];
	nis_result *res;
	char *nis_master;
	int fns_installed = 0;

	if (source_ns == AMI_NISPLUS_NS) {
		/* Check if FNS is installed NIS+ */
		sprintf(domain, "ctx_dir.%s",
		nis_local_directory());
		res = nis_lookup(domain, NO_AUTHINFO | USE_DGRAM);
		if ((res) && (res->status == NIS_SUCCESS))
			fns_installed = 1;
		if (res)
			nis_freeresult(res);
	}
	if (source_ns == AMI_NIS_NS) {
		if ((sysinfo(SI_SRPC_DOMAIN, domain,
			NIS_MAXNAMELEN) > 0) &&
		(yp_bind(domain) == 0)) {
			if (yp_master(domain, "fns_org.ctx",
				&nis_master) == 0) {
				free(nis_master);
				fns_installed = 1;
			}
			yp_unbind(domain);
		}
	}
	if (source_ns == AMI_FILES_NS) {
		/*
		* Check if the file /var/fn/fns_org.ctx.dir is present
		*/
		if (stat("/var/fn/fns_org.ctx.dir", &buffer) == 0)
			fns_installed = 1;
	}
	/* If FNS not installed, INSTALL FNS */
	if (fns_installed == 0) {
		if ((geteuid() != 0) || (install_fns(source_ns) == 0))
			return (0);
	}
	/* Create the initial context and the required context */
	status = fn_status_create();
	initial_context = _fn_ctx_handle_from_initial_with_ns(
	source_ns, 0, status);
	if (initial_context == 0) {
		fn_status_destroy(status);
		return (0);
	}
	name = fn_composite_name_from_str((unsigned char *) ctxPrefix);
	ref = fn_ctx_lookup(initial_context, name, status);
	fn_composite_name_destroy(name);
	fn_ctx_handle_destroy(initial_context);
	if (ref == 0) {
		fn_status_destroy(status);
		return (0);
	}
	context = fn_ctx_handle_from_ref(ref, 0, status);
	fn_ref_destroy(ref);
	fn_status_destroy(status);
	return (context);
}

/*
* fns_set_naming_context()
*
* Function to set the desired naming service for AMI
*
*/
static int
fns_set_naming_context(JNIEnv * env, jobject obj, const char *name_service,
const char *ctx_prefix)
{
	FN_ctx_t *context;
	jclass cls;
	jfieldID fid;
	jbyteArray jarray;
	jbyte *body;

	int source_ns = AMI_UNKNOWN_NS;
	if (strcmp(name_service, "files") == 0)
		source_ns = AMI_FILES_NS;
	else if (strcmp(name_service, "nis") == 0)
		source_ns = AMI_NIS_NS;
	else if (strcmp(name_service, "nisplus") == 0)
		source_ns = AMI_NISPLUS_NS;

	if (source_ns == AMI_UNKNOWN_NS)
		source_ns = fns_get_default_naming_service();

	/* Get the required context */
	context = fns_get_context(source_ns, ctx_prefix);

	/* Set the context in the Java Object. Context could be NULL */
	cls = (*env)->GetObjectClass(env, obj);
	fid = (*env)->GetFieldID(env, cls, "context", "[B");
	jarray = (*env)->NewByteArray(env, sizeof (FN_ctx_t *));
	body = (*env)->GetByteArrayElements(env, jarray, 0);
	memcpy(body, &context, sizeof (FN_ctx_t *));
	(*env)->ReleaseByteArrayElements(env, jarray, body, 0);
	(*env)->SetObjectField(env, obj, fid, jarray);
	return (1);
}

/*
* fns_create_context
*
* Function that creates the FNS context given the username. Return 1 on
* success. If the user context already exists, it returns 1 success. On
* failure, it returns 0.
*/

static int
fns_create_context(FN_ctx_t * user_context,
const FN_composite_name_t * user_name)
{
	FN_status_t *status;
	FN_ref_t *ref;
	int ami_status = 0;

	if (!user_name)
		return (0);

	status = fn_status_create();
	if (status == 0)
		return (0);

	/* Lookup to see if the name exists */
	ref = fn_ctx_lookup(user_context, user_name, status);
	if ((ref != 0) && (fn_status_code(status) == FN_SUCCESS)) {
		/* Name is present, return success */
		ami_status = 1;
		goto create_out;
	}
	/* Not present, create the FNS context */
	ref = fn_ctx_create_subcontext(user_context, user_name, status);
	if (fn_status_code(status) != FN_SUCCESS)
		ami_status = 0;
	else
		ami_status = 1;

create_out:
	if (ref)
		fn_ref_destroy(ref);

	if (status)
		fn_status_destroy(status);
	return (ami_status);
}

/*
* fns_destroy_context
*
* Function that destroys the given username context
*/
static int
fns_destroy_context(FN_ctx_t * user_context, const char *username)
{
	FN_status_t *status;
	FN_composite_name_t *user;
	int ami_status = 0;

	if (!username)
		return (0);

	status = fn_status_create();
	user = fn_composite_name_from_str((const unsigned char *) username);
	if ((!status) || (!user))
		return (0);

	/* Destroy the context */
	fn_ctx_destroy_subcontext(user_context, user, status);
	if (fn_status_code(status) == FN_SUCCESS)
		ami_status = 1;

	fn_status_destroy(status);
	fn_composite_name_destroy(user);
	return (ami_status);
}

/*
* fns_modify_attribute()
*
* Function that adds/deletes/modifies attribute values to a given
* username/hostname. Return 1 on success, 0 on failure.
*/

static int
fns_modify_attribute(FN_ctx_t * user_context, const char *name,
const char *id, const void *attr_value, size_t len,
const void *old_value, size_t old_len,
unsigned op, unsigned binary)
{
	FN_status_t *status;
	FN_composite_name_t *user_name;
	FN_attrvalue_t new_attrvalue, old_attrvalue;
	FN_attribute_t *attribute;
	FN_identifier_t attr_id, attr_syntax;
	int ami_status = 0;
	int attrvalue_count;

	if ((name == 0) || (id == 0) ||
	((attr_value == 0) && (op != AMI_ATTR_DELETE)) ||
	((op == AMI_ATTR_MODIFY) && (old_value == 0)))
		return (0);

	status = fn_status_create();
	user_name = fn_composite_name_from_str((unsigned char *) name);
	if ((user_name == 0) || (status == 0))
		return (0);

	/* Create attribute vlaues */
	if (attr_value) {
		new_attrvalue.length = len;
		new_attrvalue.contents = (void *) attr_value;
	}
	if (old_value) {
		old_attrvalue.length = old_len;
		old_attrvalue.contents = (void *) old_value;
	}
	/* Create the attribute identifier */
	attr_id.format = FN_ID_STRING;
	attr_id.contents = (void *) id;
	attr_id.length = strlen(id);

	/* Get the attribute form the FNS */
	attribute = fn_attr_get(user_context, user_name, &attr_id, 1, status);
	if ((fn_status_code(status) == FN_E_NO_SUCH_ATTRIBUTE) ||
	(fn_status_code(status) == FN_E_NAME_NOT_FOUND) ||
	((fn_status_code(status) == FN_SUCCESS) &&
		(attribute == 0))) {
		switch (op) {
		case AMI_ATTR_ADD:
			/* Check if the user context exists */
			if (!fns_create_context(user_context, user_name)) {
				fn_status_destroy(status);
				fn_composite_name_destroy(user_name);
				return (0);
			}
			attr_syntax.format = FN_ID_STRING;
			if (!binary) {
				attr_syntax.length = strlen(attr_id_syntax);
				attr_syntax.contents = (void
				*) attr_id_syntax;
			} else {
				attr_syntax.length =
				strlen(attr_id_syntax_bin);
				attr_syntax.contents =
				(void *) attr_id_syntax_bin;
			}
			attribute = fn_attribute_create(&attr_id,
			&attr_syntax);
			break;
		case AMI_ATTR_DELETE:
		case AMI_ATTR_MODIFY:
			fn_status_destroy(status);
			fn_composite_name_destroy(user_name);
			return (1);
		}
	} else if (fn_status_code(status) != FN_SUCCESS) {
		fn_composite_name_destroy(user_name);
		fn_status_destroy(status);
		return (0);
	}
	/* Perform the AMI attribute operations */
	switch (op) {
	case AMI_ATTR_ADD:
		fn_attribute_add(attribute, &new_attrvalue, 0);
		break;
	case AMI_ATTR_MODIFY:
		fn_attribute_add(attribute, &new_attrvalue, 0);
		fn_attribute_remove(attribute, &old_attrvalue);
		break;
	case AMI_ATTR_DELETE:
		if (attr_value)
			fn_attribute_remove(attribute, &new_attrvalue);
		break;
	}

	attrvalue_count = fn_attribute_valuecount(attribute);
	fn_attr_modify(user_context, user_name,
	(attrvalue_count && attr_value) ? FN_ATTR_OP_ADD : FN_ATTR_OP_REMOVE,
	attribute, 1, status);
	if (fn_status_code(status) != FN_SUCCESS)
		ami_status = 0;
	else
		ami_status = 1;

	fn_attribute_destroy(attribute);
	fn_composite_name_destroy(user_name);
	fn_status_destroy(status);
	return (ami_status);
}

/*
* fns_get_attribute()
*/
static FN_attribute_t *
fns_get_attribute(const char *id, const void
*value, size_t len, unsigned binary)
{
	FN_attribute_t *answer;
	FN_identifier_t syntax, ident;
	FN_attrvalue_t attr_value;

	/* Construct syntax identifier */
	syntax.format = FN_ID_STRING;
	if (!binary) {
		syntax.length = strlen(attr_id_syntax);
		syntax.contents = (void *) strdup(attr_id_syntax);
	} else {
		syntax.length = strlen(attr_id_syntax_bin);
		syntax.contents = (void *) strdup(attr_id_syntax_bin);
	}

	/* Construct attribute identifier */
	ident.format = FN_ID_STRING;
	ident.length = strlen(id);
	ident.contents = (void *) strdup(id);

	if ((syntax.contents == 0) ||
	(ident.contents == 0)) {
		/* Malloc error */
		return (0);
	}
	/* Case where value is NULL */
	if (value == 0) {
		answer = fn_attribute_create(&ident, &syntax);
		free(syntax.contents);
		free(ident.contents);
		return (answer);
	}
	/* Construct attribute value */
	attr_value.length = len;
	attr_value.contents = (void *) value;

	/* Construct the attribute */
	answer = fn_attribute_create(&ident, &syntax);
	if (answer == 0) {
		return (0);
	}
	/* Add attibute value */
	if (fn_attribute_add(answer, &attr_value, 0) == 0) {
		fn_attribute_destroy(answer);
		return (0);
	}
	free(syntax.contents);
	free(ident.contents);
	return (answer);
}

/*
 * fns_get_attributeIDs_given_name()
 *
 * Function that returns the attribute values for a given user name. Returns 1
 * on success, 0 on failure.
*/

static int
fns_get_attributeIDs_given_name(FN_ctx_t * user_context,
		const char *name,		/* IN */
	fns_item ** xfn_cert_set,	/* OUT */
		int *set_length		/* OUT */
)
{
	FN_status_t *status = 0;
	FN_attrset_t *attrset = 0;
	const FN_attribute_t *attribute = NULL;
	const FN_identifier_t *id;
	void *ip;
	char *idValue;
	FN_composite_name_t *user_name = NULL;
	int ami_status = 0;
	fns_item *current_item, *previous_item;

	if (!name || !xfn_cert_set || !set_length)
		return (0);

	*xfn_cert_set = 0;
	*set_length = 0;

	/* Construct the status and user_name */
	user_name = fn_composite_name_from_str((unsigned char *) name);
	status = fn_status_create();

	/* Check for malloc errors */
	if ((user_name == 0) ||
	(status == 0)) {
		goto id_out;
	}
	attrset = fn_attr_get_ids(user_context, user_name, 1, status);
	fn_composite_name_destroy(user_name);

	/* Check the attribute vaules and status code */
	if (!attrset || (fn_status_code(status) != FN_SUCCESS))
		goto id_out;

	/* Get attribute IDs */
	for (attribute = fn_attrset_first(attrset, &ip); attribute;
	attribute = fn_attrset_next(attrset, &ip)) {
		id = fn_attribute_identifier(attribute);
		idValue = (char *) calloc(1, id->length + 1);
		current_item = (fns_item *) malloc(sizeof (fns_item));
		if (!idValue || !current_item) {
			ami_status = 0;
			goto id_out;
		}
		memcpy(idValue, id->contents, id->length);
		current_item->data = idValue;
		current_item->data_len = id->length;
		current_item->next = 0;
		if (*xfn_cert_set == 0) {
			*xfn_cert_set = current_item;
			previous_item = current_item;
		} else {
			previous_item->next = current_item;
			previous_item = current_item;
		}
		(*set_length)++;
	}

	/* success */
	ami_status = 1;

id_out:
	if (status)
		fn_status_destroy(status);
	if (attrset)
		fn_attrset_destroy(attrset);
	if (!ami_status) {
		while (*xfn_cert_set) {
			previous_item = *xfn_cert_set;
			*xfn_cert_set = (*xfn_cert_set)->next;
			free(previous_item->data);
			free(previous_item);
		}
		*xfn_cert_set = 0;
		*set_length = 0;
	}
	return (ami_status);
}

/*
 * fns_get_attributes_given_name()
 *
 * Function that returns the attribute values for a given user name. Returns 1
 * on success, 0 on failure.
*/

static int
fns_get_attribute_given_name(FN_ctx_t * user_context,
		const char *name,		/* IN */
	const char *attr_id,	/* IN */
	fns_item ** xfn_cert_set,	/* OUT */
		int *set_length,		/* OUT */
			int *binary			/* OUT */
)
{
	void *pAnswer = 0;
	FN_status_t *status = 0;
	FN_attribute_t *attribute = NULL;
	FN_identifier_t id;
	const FN_identifier_t *syntax;
	const FN_attrvalue_t *attr_value;
	void *ip;
	FN_composite_name_t *user_name = NULL;
	int ami_status = 0;
	fns_item *current_item, *previous_item;

	if (!name || !attr_id || !xfn_cert_set || !set_length)
		return (0);

	*xfn_cert_set = 0;
	*set_length = 0;
	*binary = 0;

	user_name = fn_composite_name_from_str((unsigned char *) name);

	/* Construct the identifier */
	id.format = FN_ID_STRING;
	id.length = strlen(attr_id);
	id.contents = (void *) strdup(attr_id);

	/* Construct the status */
	status = fn_status_create();

	/* Check for malloc errors */
	if ((user_name == 0) ||
	(id.contents == 0) ||
	(status == 0)) {
		goto out;
	}
	attribute = fn_attr_get(user_context, user_name, &id, 1, status);
	fn_composite_name_destroy(user_name);
	free(id.contents);

	/* Check the attribute vaules and status code */
	if (!attribute || (fn_status_code(status) != FN_SUCCESS))
		goto out;

	/* Check if the attribute is binary */
	syntax = fn_attribute_syntax(attribute);
	if (strncmp((char *) syntax->contents, attr_id_syntax_bin,
		strlen(attr_id_syntax_bin)) == 0)
		*binary = 1;

	/* Get attribute values */
	attr_value = fn_attribute_first(attribute, &ip);
	while (attr_value) {
		pAnswer = (void *) calloc(1, attr_value->length + 1);
		current_item = (fns_item *) malloc(sizeof (fns_item));
		if (!pAnswer || !current_item) {
			ami_status = 0;
			goto out;
		}
		memcpy(pAnswer, attr_value->contents, attr_value->length);
		current_item->data = pAnswer;
		current_item->data_len = attr_value->length;
		current_item->next = 0;
		if (*xfn_cert_set == 0) {
			*xfn_cert_set = current_item;
			previous_item = current_item;
		} else {
			previous_item->next = current_item;
			previous_item = current_item;
		}
		(*set_length)++;
		attr_value = fn_attribute_next(attribute, &ip);
	}

	/* success */
	ami_status = 1;

out:
	if (status)
		fn_status_destroy(status);
	if (attribute)
		fn_attribute_destroy(attribute);
	if (!ami_status) {
		while (*xfn_cert_set) {
			previous_item = *xfn_cert_set;
			*xfn_cert_set = (*xfn_cert_set)->next;
			free(previous_item->data);
			free(previous_item);
		}
		*xfn_cert_set = 0;
		*set_length = 0;
	}
	return (ami_status);
}


/*
 * fns_search_given_attribute()
 *
 * Function that returns the list of contexts that have the given attribute.
 * Return 1 on success, 0 on failure.
 *
*/
static int
fns_search_given_attribute(FN_ctx_t * user_context,
const char *attrID,
const char *attrValue,
fns_item ** ppAnswer,
int *length
)
{
	FN_status_t *status = 0;
	FN_string_t *username;
	const char *u_name;
	FN_searchlist_t *searchlist;
	FN_attrset_t *match_attrs = 0;
	FN_attribute_t *attribute = 0;
	FN_composite_name_t *empty_name;
	fns_item *current_item, *previous_item;
	int ami_status = 0;

	if (!attrID || !attrValue || !ppAnswer)
		return (0);

	*ppAnswer = 0;
	*length = 0;

	/* Construct the search attribute set */
	status = fn_status_create();
	match_attrs = fn_attrset_create();
	if ((!match_attrs) || (!status))
		goto out;

	/* Construct the search attribute */
	if ((attribute = fns_get_attribute(attrID, attrValue,
		strlen(attrValue), 0)) == 0)
		goto out;

	/* Add search attibute to search attribute set */
	if (fn_attrset_add(match_attrs, attribute, 0) == 0)
		/* Unable to add attributes to attribute set */
		goto out;

	/* Construct the empty name */
	empty_name = fn_composite_name_from_str((uchar_t *) "");
	if (empty_name == 0)
		goto out;

	/* Perform the search operation */
	/* First search in user context */
	searchlist = prelim_fn_attr_search(user_context,
	empty_name, match_attrs, 0, 0, status);
	fn_composite_name_destroy(empty_name);
	if ((fn_status_code(status) != FN_SUCCESS) || (searchlist == 0))
		/* Attribute search operation failed */
		goto out;

	/* Get the first match */
	while (username = prelim_fn_searchlist_next(searchlist, 0, 0,
	status)) {
		if (fn_status_code(status) == FN_SUCCESS) {
			u_name = (const char *) fn_string_str(username, 0);
			current_item = (fns_item *) malloc(sizeof (fns_item));
			current_item->data = (void *) strdup(u_name);
			current_item->data_len = strlen(u_name);
			current_item->next = 0;
			if (*ppAnswer == 0) {
				*ppAnswer = current_item;
				previous_item = current_item;
			} else {
				previous_item->next = current_item;
				previous_item = current_item;
			}
			(*length)++;
			fn_string_destroy(username);
		}
	}
	prelim_fn_searchlist_destroy(searchlist);

	/* success */
	ami_status = 1;

out:
	if (match_attrs)
		fn_attrset_destroy(match_attrs);
	if (attribute)
		fn_attribute_destroy(attribute);
	if (status)
		fn_status_destroy(status);
	return (ami_status);
}

static char *
fns_list_next_item(FN_namelist_t * ns)
{
	char *answer = 0;
	FN_status_t *status;
	FN_string_t *name;

	if (ns == 0)
		return (0);
	status = fn_status_create();
	name = fn_namelist_next(ns, status);
	if (name &&
	(fn_status_code(status) == FN_SUCCESS)) {
		answer = strdup((char *) fn_string_str(name, 0));
		fn_string_destroy(name);
	}
	fn_status_destroy(status);
	return (answer);
}

static char *
fns_list_next(FN_ctx_t * user_context, FN_namelist_t * ns,
const char *id, const char *value)
{
	char *answer;
	FN_status_t *status;
	FN_composite_name_t *name;
	FN_attribute_t *attribute;
	const FN_attrvalue_t *attrvalue;
	FN_identifier_t attr_id;
	void *ip;
	int done = 0;

	/* Get the objectClass attibute */
	attr_id.format = FN_ID_STRING;
	attr_id.contents = (void *) id;
	attr_id.length = strlen(id);
	status = fn_status_create();
	while ((!done) && (answer = fns_list_next_item(ns))) {
		name = fn_composite_name_from_str((unsigned char *) answer);
		attribute = fn_attr_get(user_context, name, &attr_id, 1,
		status);
		if ((attribute == 0) || (fn_status_code(status)
		!= FN_SUCCESS))
			continue;
		fn_composite_name_destroy(name);
		for (attrvalue = fn_attribute_first(attribute, &ip);
		attrvalue; attrvalue = fn_attribute_next(attribute, &ip)) {
			if ((strlen(value) == attrvalue->length) &&
			(memcmp(value, attrvalue->contents,
				attrvalue->length) == 0))
				done = 1;
		}
		fn_attribute_destroy(attribute);
		if (!done)
			free(answer);
	}
	fn_status_destroy(status);
	return (answer);
}

/*
 * fns_list_namelist()
 *
 * Function that returns the namelist
*/
static FN_namelist_t *
fns_list_namelist(FN_ctx_t * user_context)
{
	FN_status_t *status;
	FN_namelist_t *namelist;
	FN_composite_name_t *empty_name;

	status = fn_status_create();
	empty_name = fn_composite_name_from_str((unsigned char *) "");

	namelist = fn_ctx_list_names(user_context, empty_name, status);
	fn_status_destroy(status);
	fn_composite_name_destroy(empty_name);
	return (namelist);
}

/*
 * fns_get_context_from_java_class(JNIEnv *env, jobject obj)
 *
 * Method to get the FNS context from the Java Class
 *
*/
static FN_ctx_t *
fns_get_context_from_java_class(JNIEnv * env, jobject obj)
{
	FN_ctx_t *context;
	jclass cls;
	jfieldID fid;
	jbyteArray jarray;
	jbyte *body;

	cls = (*env)->GetObjectClass(env, obj);
	fid = (*env)->GetFieldID(env, cls, "context", "[B");
	jarray = (*env)->GetObjectField(env, obj, fid);
	body = (*env)->GetByteArrayElements(env, jarray, 0);
	memcpy(&context, body, sizeof (FN_ctx_t *));
	(*env)->ReleaseByteArrayElements(env, jarray, body, 0);

	return (context);
}

/*
 * Class:     com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFNS Method:
 * fns_add_attribute Signature:
 * (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)I
 *
 * Return 1	SUCCESS 0	FAILURE
*/
JNIEXPORT jint JNICALL
Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1add_1attribute(
JNIEnv * env, jobject object, jstring jname, jstring jattrID,
jstring jattrValue, jint searchable)
{
	const char *c_name, *c_attrID, *c_attrValue;
	int rc;
	FN_ctx_t *context;

	/* Convert java strings to C strings */
	if ((!jname) || (!jattrID) || (!jattrValue))
		return (0);
	c_name = (*env)->GetStringUTFChars(env, jname, 0);
	c_attrID = (*env)->GetStringUTFChars(env, jattrID, 0);
	c_attrValue = (*env)->GetStringUTFChars(env, jattrValue, 0);

	/* Call add attribute operation */
	context = fns_get_context_from_java_class(env, object);
	if (context == NULL)
		return (0);
	rc = fns_modify_attribute(context, c_name, c_attrID, c_attrValue,
	(*env)->GetStringUTFLength(env, jattrValue),
	0, 0, AMI_ATTR_ADD, searchable ? 0 : 1);

	(*env)->ReleaseStringUTFChars(env, jname, c_name);
	(*env)->ReleaseStringUTFChars(env, jattrID, c_attrID);
	(*env)->ReleaseStringUTFChars(env, jattrValue, c_attrValue);
	return (rc);
}

/*
 * Class:     com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFNS Method:
 * fns_add_binary_attribute Signature:
 * (Ljava/lang/String;Ljava/lang/String;[B)I
*/
JNIEXPORT jint JNICALL
Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1add_1binary_1attribute(
JNIEnv * env, jobject object, jstring jname, jstring jattrID,
jbyteArray jattrValue)
{
	const char *c_name, *c_attrID;
	jbyte *c_attrValue;
	FN_ctx_t *context;
	int rc;

	/* Convert java strings to C strings */
	if ((!jname) || (!jattrID) || (!jattrValue))
		return (0);
	c_name = (*env)->GetStringUTFChars(env, jname, 0);
	c_attrID = (*env)->GetStringUTFChars(env, jattrID, 0);
	c_attrValue = (*env)->GetByteArrayElements(env, jattrValue, 0);

	/* Call add attribute operation */
	context = fns_get_context_from_java_class(env, object);
	if (context == NULL)
		return (0);
	rc = fns_modify_attribute(context, c_name, c_attrID, c_attrValue,
	(*env)->GetArrayLength(env, jattrValue),
	0, 0, AMI_ATTR_ADD, 1);

	(*env)->ReleaseStringUTFChars(env, jname, c_name);
	(*env)->ReleaseStringUTFChars(env, jattrID, c_attrID);
	(*env)->ReleaseByteArrayElements(env, jattrValue, c_attrValue, 0);
	return (rc);
}


/*
 * Class:     com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFNS Method:
 * fns_delete_attribute Signature:
 * (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I
 *
 * Return 1	SUCCESS 0	FAILURE
*/
JNIEXPORT jint JNICALL
Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1delete_1attribute(
JNIEnv * env, jobject object, jstring jname,
jstring jattrID, jstring jattrValue)
{
	const char *c_name, *c_attrID, *c_attrValue;
	FN_ctx_t *context;
	int rc;

	/* Convert java strings to C strings */
	if ((!jname) || (!jattrID))
		return (0);
	c_name = (*env)->GetStringUTFChars(env, jname, 0);
	c_attrID = (*env)->GetStringUTFChars(env, jattrID, 0);
	if (jattrValue)
		c_attrValue = (*env)->GetStringUTFChars(env, jattrValue, 0);
	else
		c_attrValue = 0;

	/* Call delete attribute operation */
	context = fns_get_context_from_java_class(env, object);
	if (context == NULL)
		return (0);
	rc = fns_modify_attribute(context, c_name, c_attrID, c_attrValue,
	c_attrValue ? (*env)->GetStringUTFLength(env, jattrValue) : 0,
	0, 0, AMI_ATTR_DELETE, 0);

	(*env)->ReleaseStringUTFChars(env, jname, c_name);
	(*env)->ReleaseStringUTFChars(env, jattrID, c_attrID);
	if (c_attrValue)
		(*env)->ReleaseStringUTFChars(env, jattrValue, c_attrValue);
	return (rc);
}

/*
 * Class:     com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFNS Method:
 * fns_delete_binary_attribute Signature:
 * (Ljava/lang/String;Ljava/lang/String;[B)I
*/
JNIEXPORT jint JNICALL
Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1delete_1binary_1attribute(
JNIEnv * env, jobject object, jstring jname, jstring jattrID,
jbyteArray jattrValue)
{
	const char *c_name, *c_attrID;
	jbyte *c_attrValue;
	FN_ctx_t *context;
	int rc;

	/* Convert java strings to C strings */
	if ((!jname) || (!jattrID))
		return (0);
	c_name = (*env)->GetStringUTFChars(env, jname, 0);
	c_attrID = (*env)->GetStringUTFChars(env, jattrID, 0);
	if (jattrValue)
		c_attrValue = (*env)->GetByteArrayElements(env, jattrValue,
		0);
	else
		c_attrValue = 0;

	/* Call delete attribute operation */
	context = fns_get_context_from_java_class(env, object);
	if (context == NULL)
		return (0);
	rc = fns_modify_attribute(context, c_name, c_attrID, c_attrValue,
	c_attrValue ? (*env)->GetArrayLength(env, jattrValue) : 0,
	0, 0, AMI_ATTR_DELETE, 0);

	(*env)->ReleaseStringUTFChars(env, jname, c_name);
	(*env)->ReleaseStringUTFChars(env, jattrID, c_attrID);
	if (c_attrValue)
		(*env)->ReleaseByteArrayElements(env, jattrValue, c_attrValue,
		0);
	return (rc);
}

/*
 * Class:     com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFNS Method:
 * fns_destroy_context Signature: (Ljava/lang/String;)I
 *
 * Return 1	SUCCESS 0	FAILURE
*/
JNIEXPORT jint JNICALL
Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1destroy_1context(
JNIEnv * env, jobject object, jstring jname)
{
	const char *c_name;
	FN_ctx_t *context;
	int rc;

	c_name = (*env)->GetStringUTFChars(env, jname, 0);
	context = fns_get_context_from_java_class(env, object);
	if (context == NULL)
		return (0);
	rc = fns_destroy_context(context, c_name);
	(*env)->ReleaseStringUTFChars(env, jname, c_name);
	return (rc);
}

/*
 * Class:     com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFNS Method:
 * fns_get_attribute Signature:
 * (Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;
 *
 * Return 1	SUCCESS 0	FAILURE
*/
JNIEXPORT jobjectArray JNICALL
Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1get_1attribute(
JNIEnv * env, jobject object, jstring jname, jstring jattrID)
{
	const char *c_name, *c_attrID;
	FN_ctx_t *context;
	jobjectArray janswer = 0;
	jstring jelement;
	jbyteArray jbyteelement;
	fns_item *answer, *current;
	int i, length, binary;
	jclass cls;

	c_name = (*env)->GetStringUTFChars(env, jname, 0);
	c_attrID = (*env)->GetStringUTFChars(env, jattrID, 0);

	context = fns_get_context_from_java_class(env, object);
	if (context == NULL)
		return (0);
	if (fns_get_attribute_given_name(context, c_name, c_attrID,
		&answer, &length, &binary)) {
		/* Convert fns_item to Java String array */
		cls = (*env)->FindClass(env, "java/lang/Object");
		janswer = (*env)->NewObjectArray(env, length, cls, 0);
		for (i = 0, current = answer;
		((i < length) && (current));
		i++, current = current->next) {
			if (!binary) {
				jelement = (*env)->NewStringUTF(env,
				(char *) current->data);
				(*env)->SetObjectArrayElement(env, janswer,
				i, jelement);
			} else {
				jbyteelement = (*env)->NewByteArray(env,
				current->data_len);
				(*env)->SetByteArrayRegion(env, jbyteelement,
				0, current->data_len, current->data);
				/*
				* jelement = (*env)->NewString(env,
				* current->data, current->data_len);
				*/
				(*env)->SetObjectArrayElement(env, janswer,
				i, jbyteelement);
			}
		}
		/* Release the memory */
		while (answer) {
			current = answer->next;
			free(answer);
			answer = current;
		}
	}
	(*env)->ReleaseStringUTFChars(env, jname, c_name);
	(*env)->ReleaseStringUTFChars(env, jattrID, c_attrID);
	return (janswer);
}

/*
 * Class:     com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFNS Method:
 * fns_get_attributeIDs Signature: (Ljava/lang/String;)[Ljava/lang/String;
 *
 * Return 1	SUCCESS 0	FAILURE
*/
JNIEXPORT jobjectArray
JNICALL
Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1get_1attributeIDs(
JNIEnv * env, jobject object, jstring jname)
{
	int length, i = 0;
	fns_item *ids, *next;
	const char *c_name;
	FN_ctx_t *context;
	jobjectArray janswer;
	jstring jelement;
	jclass cls;

	c_name = (*env)->GetStringUTFChars(env, jname, 0);
	context = fns_get_context_from_java_class(env, object);
	if (context == NULL)
		return (0);
	if (!fns_get_attributeIDs_given_name(context, c_name, &ids,
	&length) ||
	!ids || !length) {
		(*env)->ReleaseStringUTFChars(env, jname, c_name);
		return (0);
	}
	(*env)->ReleaseStringUTFChars(env, jname, c_name);

	cls = (*env)->FindClass(env, "java/lang/String");
	janswer = (*env)->NewObjectArray(env, length, cls, 0);
	while (ids) {
		jelement = (*env)->NewStringUTF(env, (char *) ids->data);
		(*env)->SetObjectArrayElement(env, janswer, i++, jelement);
		next = ids->next;
		free(ids);
		ids = next;
	}
	return (janswer);

}

/*
 * Class:     com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFNS Method:
 * fns_search Signature:
 * (Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;
 *
 * Return 1	SUCCESS 0	FAILURE
*/
JNIEXPORT jobjectArray JNICALL
Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1search(
JNIEnv * env, jobject object, jstring jattrID, jstring jattrValue)
{
	const char *c_attrID, *c_attrValue;
	FN_ctx_t *context;
	jobjectArray janswer = 0;
	jstring jelement;
	fns_item *answer, *current;
	jclass cls;
	int i, length;

	c_attrID = (*env)->GetStringUTFChars(env, jattrID, 0);
	c_attrValue = (*env)->GetStringUTFChars(env, jattrValue, 0);
	context = fns_get_context_from_java_class(env, object);
	if (context == NULL)
		return (NULL);
	if (fns_search_given_attribute(context, c_attrID, c_attrValue,
		&answer, &length)) {
		/* Convert fns_item to Java String array */
		cls = (*env)->FindClass(env, "java/lang/String");
		janswer = (*env)->NewObjectArray(env, length, cls, 0);
		for (i = 0, current = answer;
		((i < length) && (current));
		i++, current = current->next) {
			jelement = (*env)->NewStringUTF(env,
			(char *) current->data);
			(*env)->SetObjectArrayElement(env, janswer, i,
			jelement);
		}
		/* Release the memory */
		while (answer) {
			current = answer->next;
			free(answer);
			answer = current;
		}
	}
	(*env)->ReleaseStringUTFChars(env, jattrID, c_attrID);
	(*env)->ReleaseStringUTFChars(env, jattrValue, c_attrValue);
	return (janswer);
}

/*
 * Class:     com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFNS Method:
 * fns_set_naming_context Signature: (Ljava/lang/String;Ljava/lang/String;)I
 *
 * Return 1	SUCCESS 0	FAILURE
*/
JNIEXPORT jint JNICALL
Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1set_1naming_1context(
JNIEnv * env, jobject object, jstring jname, jstring jctx)
{
	const char *c_name, *ctx_prefix;
	int rc;

	c_name = (*env)->GetStringUTFChars(env, jname, 0);
	ctx_prefix = (*env)->GetStringUTFChars(env, jctx, 0);
	rc = fns_set_naming_context(env, object, c_name, ctx_prefix);
	(*env)->ReleaseStringUTFChars(env, jname, c_name);
	(*env)->ReleaseStringUTFChars(env, jctx, ctx_prefix);

	return (rc);
}

/*
 * Class:     com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFNS_0005fEnumeration
 * Method:    fns_list_next Signature:
 * (Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
*/
JNIEXPORT jstring JNICALL
Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_1Enumeration_fns_1list_1next(
JNIEnv * env, jobject object, jstring jattrID, jstring jattrValue)
{
	jclass cls;
	jfieldID fid;
	jstring jns;
	FN_namelist_t *ns;
	FN_ctx_t *context;
	const jchar *jpointer;
	const char *c_attrID, *c_attrValue;
	char *answer;
	jstring jname;

	if ((!jattrID) || (!jattrValue))
		return (0);

	/* Get the namelist pointer */
	cls = (*env)->GetObjectClass(env, object);
	fid = (*env)->GetFieldID(env, cls, "list_ctx", "Ljava/lang/String;");
	jns = (*env)->GetObjectField(env, object, fid);
	context = fns_get_context_from_java_class(env, object);
	if (context == NULL)
		return (NULL);
	if ((jns == 0) || ((void *)
		(jpointer = (*env)->GetStringChars(env, jns, 0)) == 0)) {
		if ((ns = fns_list_namelist(context)) == 0)
			return (0);
		/* Copy the namelist pointer into java */
		jns = (*env)->NewString(env, (jchar *) & ns, sizeof (void *));
		(*env)->SetObjectField(env, object, fid, jns);
	} else {
		/* Copy the namelist pointer from java */
		memcpy(&ns, jpointer, sizeof (void *));
		(*env)->ReleaseStringChars(env, jns, jpointer);
	}

	/* Convert java strings to C strings */
	c_attrID = (*env)->GetStringUTFChars(env, jattrID, 0);
	c_attrValue = (*env)->GetStringUTFChars(env, jattrValue, 0);

	answer = fns_list_next(context, ns, c_attrID, c_attrValue);
	(*env)->ReleaseStringUTFChars(env, jattrID, c_attrID);
	(*env)->ReleaseStringUTFChars(env, jattrValue, c_attrValue);

	if (!answer) {
		if (ns)
			fn_namelist_destroy(ns);
		return (0);
	}
	jname = (*env)->NewStringUTF(env, answer);
	free(answer);
	return (jname);
}

/*
 * Class:     com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFNS Method:
 * fns_context_handle_destroy Signature: ()I
*/
JNIEXPORT jint JNICALL
Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1context_1handle_1destroy(
JNIEnv * env, jobject obj)
{
	FN_ctx_t *context = fns_get_context_from_java_class(env, obj);
	if (context != NULL)
		fn_ctx_handle_destroy(context);
	return (1);
}

/* Method to obtain the home directory of a user */
/*
 * Class:     com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFILE
 * Method:    ami_get_user_home_directory
 * Signature: (Ljava/lang/String;)Ljava/lang/String;
 */
JNIEXPORT jstring
JNICALL
Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FILE_ami_1get_1user_1home_1directory(
    JNIEnv *env, jobject obj, jstring jname)
{
	const char *username;
	char buf[2048], filename[1024];
	struct passwd userPasswd, *ptr;
	jstring janswer;

	/* Following line added for lint */
	if (obj == NULL) obj = NULL;

	username = (*env)->GetStringUTFChars(env, jname, 0);
	ptr = getpwnam_r(username, &userPasswd, buf, 2048);
	if (ptr == NULL) {
		strcpy(filename, "/home/");
		strcat(filename, username);
	} else
		strcpy(filename, userPasswd.pw_dir);
	(*env)->ReleaseStringUTFChars(env, jname, username);
	janswer = (*env)->NewStringUTF(env, filename);
	return (janswer);
}

/*
 * Class:     com_sun_ami_keymgnt_AMI_0005fKeyMgnt_0005fFNS
 * Method:    fns_get_uid
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1get_1euid(
    JNIEnv *env, jclass class, jstring jstring)
{
	const char *username;
	int uid = 0;
	struct passwd userPasswd, *ptr;
	char buf[2048];

	/* Following line added for lint */
	if (class == NULL) class = NULL;

	if (jstring == 0)
		return ((jint) geteuid());

	username = (*env)->GetStringUTFChars(env, jstring, 0);
	/* If it is a hostname, return 0 */
	if (((int)inet_addr(username)) != -1) {
		uid = 0;
	} else {
		ptr = getpwnam_r(username, &userPasswd, buf, 2048);
		if (ptr != NULL)
			uid = userPasswd.pw_uid;
	}

	(*env)->ReleaseStringUTFChars(env, jstring, username);
	return (uid);
}
