/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnattr.cc	1.10	96/04/09 SMI"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <libintl.h>
#include <xfn/xfn.hh>
#include <rpcsvc/nis.h>
#include "fnattr_utils.hh"

static unsigned int authoritative = 0;
static int follow_link = 0;

enum FNSP_attr_operation {
	FNSP_attr_add_operation,
	FNSP_attr_list_operation,
	FNSP_attr_delete_operation,
	FNSP_attr_modify_operation
};

static const unsigned char *composite_name = 0;
static FN_attrmodlist *modlist = 0;
static FN_attrset *list_attrset = 0;
static int list_mode = 0;

static const FN_identifier
attribute_syntax((const unsigned char *) "fn_attr_syntax_ascii");
static int more_options = 1;

// checks whether "--" (which signifies end of options list)
// has been encountered.
// returns 0 if it has been (i.e. no more options); 1 otherwise;
static int
check_more_options(int argc, char **argv)
{
	if (more_options == 0)
		return (0);  // cannot be undone, do not bother checking

	if (optind < argc && argv[optind] && strcmp(argv[optind], "--") == 0) {
		more_options = 0;
	}
	return (more_options);
}

static void
check_error(FN_status &status, char *msg = 0)
{
	if (!status.is_success()) {
		if (msg)
			fprintf(stderr, "%s\n", msg);
		fprintf(stderr, "%s: %s\n", gettext("Error"),
			status.description()->str());
		exit(1);
	}
}

// -L	If the composite name is bound to an XFN link, manipulate the
//	attributes associated with the object pointed to by the link.
//	If -L is not used, the attributes associated with the XFN link,
//	is manipulated.
// -A   Consult the authoritative source to get attribute information

static void
usage(char *cmd, char *msg = 0)
{
	if (msg)
		fprintf(stderr, "%s: %s\n", cmd, gettext(msg));
	fprintf(stderr, "%s:\t%s [-A][-L] %s [[-O|-U] %s]*\n",
		gettext("Usage"),
		cmd, gettext("composite_name"), gettext("attr_id"));
	fprintf(stderr, "%s:\t%s [-L] %s \\\n", gettext("Usage"), cmd,
		gettext("composite_name"));
	fprintf(stderr, "\t\t[ {-a [-s] [-O|-U] %s [%s]*} | \\\n",
		gettext("attr_id"), gettext("attr_val"));
	fprintf(stderr, "\t\t {-d [[-O|-U] %s [%s]*] } | \\\n",
		gettext("attr_id"), gettext("attr_val"));
	fprintf(stderr, "\t\t {-m [-O|-U] %s} ]*\n",
		gettext("attr_id old_val new_val"));
	exit(1);
}

static FN_attribute *
get_attr_id(int argc, char **argv)
{
	unsigned int id_format = FN_ID_STRING;
	int num_format = 0;
	FN_identifier *attr_id;
	FN_attribute *attr;
	int c;

	// Now check for identifier types, viz., UUID and OID in string format
	while (more_options && (c = getopt(argc, argv, "OU")) != -1) {
		switch (c) {
		case 'O':
			num_format++;
			id_format = FN_ID_ISO_OID_STRING;
			break;
		case 'U':
			num_format++;
			id_format = FN_ID_DCE_UUID;
			break;
		default:
			usage(argv[0]);
			break;
		}
	}
	check_more_options(argc, argv);

	if (num_format > 1)
		usage(argv[0], "Too many identifier formats");

	if (argc < (optind + 1)) {
		if (num_format == 1)
			usage(argv[0], "Missing attribute identifier");
		else
			return (NULL);
	}
	attr_id = new FN_identifier(id_format,
	    strlen(argv[optind]), (const void *)argv[optind]);
	optind++; // increment

	if (attr_id == NULL)
		return (NULL);

	attr = new FN_attribute(*attr_id, attribute_syntax);
	delete attr_id;
	return (attr);
}

// -a [-s]
// Options:  [-O|-U] attr_id [attr_val]*
static int
record_add_operation(int argc,
		    char **argv,
		    int add_supersede)
{
	FN_attribute *attr;
	FN_attrvalue *attrvalue;
	int number_of_values;
	const unsigned char **values;

	attr = get_attr_id(argc, argv);

	if (attr == NULL)
		usage(argv[0], "Missing attribute identifier");

	// Get attr values
	number_of_values = argc - optind;
	if (number_of_values > 0) {
		values = (const unsigned char **) &argv[optind];
		for (int i = 0; i < number_of_values; i++) {
			attrvalue = new FN_attrvalue(values[i]);
			attr->add(*attrvalue);
			delete attrvalue;
		}
		optind += number_of_values;
	} else {
		fprintf(stdout, "%s\n",
			gettext("No attribute value specified for addition"));
	}
	if (modlist == NULL)
		modlist = new FN_attrmodlist;
	if (add_supersede)
		modlist->add(FN_ATTR_OP_ADD, *attr);
	else
		modlist->add(FN_ATTR_OP_ADD_VALUES, *attr);

	delete attr;
	return (1);
}

// -d
// Options:  [ [-O|-U] attr_id [attr_val]* ]
// Delete all attributes if no attr_id is specified.
// Delete all attribute values if no attr_val is specified.

static int
record_delete_operation(int argc,
			char **argv,
			const FN_composite_name &name)
{
	FN_attribute *attr;
	const FN_attribute *cattr;
	FN_attrvalue *attrvalue;
	int number_of_values;
	const unsigned char **values;

	attr = get_attr_id(argc, argv);

	if (attr == NULL) {
		// Delete all attributes associated with name
		FN_status status;
		FN_ctx *init_ctx = FN_ctx::from_initial(1, status);
		FN_attrset *all;
		void *ip;

		check_error(status, "Unable to get initial context");

		all = init_ctx->attr_get_ids(name, follow_link, status);
		delete init_ctx;
		check_error(status,
			    "Unable to determine which attributes to delete");
		if (all == NULL)
			return (1);  // nothing to delete
		if (modlist == NULL)
			modlist = new FN_attrmodlist;
		for (cattr = all->first(ip);
		    cattr != NULL; cattr = all->next(ip)) {
			modlist->add(FN_ATTR_OP_REMOVE, *cattr);
		}
		delete all;
	} else {
		number_of_values = argc - optind;
		if (modlist == NULL)
			modlist = new FN_attrmodlist;
		if (number_of_values > 0) {
			values = (const unsigned char **) &argv[optind];
			for (int i = 0; i < number_of_values; i++) {
				attrvalue = new FN_attrvalue(values[i]);
				attr->add(*attrvalue);
				delete attrvalue;
			}
			optind += number_of_values;

			// Delete only those attribute values specified
			modlist->add(FN_ATTR_OP_REMOVE_VALUES, *attr);
		} else {
			// Delete entire attribute
			modlist->add(FN_ATTR_OP_REMOVE, *attr);
		}
		delete attr;
	}

	return (1);
}

// -m
// [-O|-U] attr_id oldval newval
static int
record_modify_operation(int argc,
			char **argv)
{
	FN_attribute *attr;
	FN_attrvalue *attrvalue;
	const unsigned char **values;

	attr = get_attr_id(argc, argv);

	if (attr == NULL)
		usage(argv[0], "Missing attribute identifier");

	if (argc != (optind + 2))
		usage(argv[0], "Incorrect number of attribute values");

	values = (const unsigned char **) &argv[optind];

	// Remove old value
	attrvalue = new FN_attrvalue(values[0]);
	attr->add(*attrvalue);
	if (modlist == NULL)
		modlist = new FN_attrmodlist;

	modlist->add(FN_ATTR_OP_REMOVE_VALUES, *attr);
	attr->remove(*attrvalue);
	delete attrvalue;

	// Add new value
	attrvalue = new FN_attrvalue(values[1]);
	attr->add(*attrvalue);
	modlist->add(FN_ATTR_OP_ADD_VALUES, *attr);
	delete attrvalue;

	delete attr;

	optind += 2;
	return (1);
}

static void
process_single_mod_options(int argc, char **argv,
			    unsigned int attribute_operation,
			    const FN_composite_name &name,
			    unsigned int add_supersede)
{
	FN_attribute *attr;

	if (add_supersede && attribute_operation != FNSP_attr_add_operation)
		usage(argv[0], "-s is only a modifier for -a");

	switch (attribute_operation) {
	case FNSP_attr_list_operation:
		list_mode = 1;
		attr = get_attr_id(argc, argv);
		if (argc > optind)
			usage(argv[0], "Too many attribute identifiers");
		if (attr != NULL) {
			if (list_attrset == NULL)
				list_attrset = new FN_attrset;
			list_attrset->add(*attr);
			delete attr;
		}
		break;

	case FNSP_attr_add_operation:
		record_add_operation(argc, argv, add_supersede);
		break;

	case FNSP_attr_delete_operation:
		record_delete_operation(argc, argv, name);
		break;

	case FNSP_attr_modify_operation:
		record_modify_operation(argc, argv);
		break;

	default:
		usage(argv[0]);
		break;
	}
}

static int
is_modifier(const char *str)
{
	if (str[0] == '-') {
		switch (str[1]) {
		case 'a':
		case 'm':
		case 'd':
			return (1);
		}
	}
	return (0);
}

static void
process_mod_options(int argc, char **argv,
	const FN_composite_name &name,
	int add_supersede)
{
	int c;
	int option_specified = 0, supersede_flag = 0;
	int virtual_argc, i;
	FNSP_attr_operation attribute_operation = FNSP_attr_list_operation;

	while (more_options && (c = getopt(argc, argv, "asdmOU")) != -1) {
		switch (c) {
		case 'a':
			option_specified++;
			attribute_operation = FNSP_attr_add_operation;
			authoritative = 1;
			// get supersede flag if any
			if ((c = getopt(argc, argv, "sOU")) != -1) {
				switch (c) {
				case 's':
					add_supersede = 1;
					break;
				case 'O':
				case 'U':
					--optind; // save for attr id
					break;
				default:
					usage(argv[0]);
				}
			}
			break;
		case 'd':
			option_specified++;
			attribute_operation = FNSP_attr_delete_operation;
			authoritative = 1;
			break;
		case 'm':
			option_specified++;
			attribute_operation = FNSP_attr_modify_operation;
			authoritative = 1;
			break;
		case 's':
			add_supersede = 1;
			supersede_flag = 1;
			break;
		case 'O':
		case 'U':
			--optind;  // save these for attr id processing
			break;
		default:
			usage(argv[0]);
		}
		if (option_specified > 1)
			usage(argv[0], "Incorrect options specified");

		check_more_options(argc, argv);

		// only specified -s, return to get real option
		if (supersede_flag == 1) {
			supersede_flag = 0;
			continue;
		}
		// calculate number of arguments to process for this option
		virtual_argc = optind;
		for (i = optind; i < argc; i++) {
			if (argv[i] != NULL && is_modifier(argv[i]))
				break;
			++virtual_argc;
		}

		process_single_mod_options(virtual_argc, argv,
		    attribute_operation, name, add_supersede);

		option_specified = add_supersede = supersede_flag = 0; // reset
	}

	// handle case of listing only
	if (attribute_operation == FNSP_attr_list_operation) {
		if (add_supersede)
			usage(argv[0], "-s is only a modifier for -a");
		FN_attribute *attr;
		while (attr = get_attr_id(argc, argv)) {
			if (list_attrset == NULL)
				list_attrset = new FN_attrset;
			list_attrset->add(*attr);
			delete attr;
		}
		list_mode = 1;
	}
}

static void
process_cmd_line(int argc, char **argv)
{
	int c;
	int option_specified = 0;
	int add_supersede = 0;
	FNSP_attr_operation attribute_operation = FNSP_attr_list_operation;

	while ((c = getopt(argc, argv, "LAasdlm")) != -1) {
		switch (c) {
		case 'A':
			authoritative = 1;
			break;
		case 'L':
			follow_link = 1;
			break;
		case 'a':
			option_specified++;
			attribute_operation = FNSP_attr_add_operation;
			authoritative = 1;
			break;
		case 's':
			add_supersede = 1;
			break;
		case 'l':
			option_specified++;
			attribute_operation = FNSP_attr_list_operation;
			break;
		case 'd':
			option_specified++;
			attribute_operation = FNSP_attr_delete_operation;
			authoritative = 1;
			break;
		case 'm':
			option_specified++;
			attribute_operation = FNSP_attr_modify_operation;
			authoritative = 1;
			break;
		default:
			usage(argv[0]);
			break;
		}
	}
	check_more_options(argc, argv);

	if (option_specified > 1)
		usage(argv[0], "Incorrect options specified");

	if (optind >= argc)
		usage(argv[0], "Missing composite name");
	composite_name = (const unsigned char *) argv[optind++];

	if (option_specified == 1) {
		// In the old format, the -a, -l, etc options
		// came before the composite name.
		process_single_mod_options(argc, argv, attribute_operation,
		    composite_name, add_supersede);
	} else {
		// New format accepts multiple options on same line
		process_mod_options(argc, argv, composite_name, add_supersede);
	}
}

static void
print_multigetlist(FN_multigetlist *ml, FILE *outstream = stdout)
{
	FN_status status;
	FN_attribute *attr = ml->next(status);
	while (attr) {
		print_attribute(attr, outstream);
		delete attr;
		attr = ml->next(status);
	}
}

static void
print_modlist(const FN_attrmodlist *mlist, FILE *outstream = stdout,
	    const char *msg = NULL)
{
	void *ip;
	const FN_attribute *attr;
	unsigned int mod;

	if (mlist == NULL)
		return;

	if (msg)
		fprintf(outstream, msg);

	for (attr = mlist->first(ip, mod);
	    attr != NULL;
	    attr = mlist->next(ip, mod)) {
		fprintf(outstream, "%s: ", gettext("Modification"));
		switch (mod) {
		case FN_ATTR_OP_ADD:
			fprintf(outstream, gettext("ADD"));
			break;
		case FN_ATTR_OP_ADD_EXCLUSIVE:
			fprintf(outstream, gettext("ADD EXCLUSIVE"));
			break;
		case FN_ATTR_OP_ADD_VALUES:
			fprintf(outstream, gettext("ADD VALUES"));
			break;
		case FN_ATTR_OP_REMOVE:
			fprintf(outstream, gettext("REMOVE"));
			break;
		case FN_ATTR_OP_REMOVE_VALUES:
			fprintf(outstream, gettext("REMOVE VALUES"));
			break;
		default:
			fprintf(outstream, gettext("UNKNOWN"));
		}
		print_attribute(attr, outstream);
	}
}
#ifdef DEBUG
static void
print_cmd_line()
{
	if (list_mode) {
		printf("%s '%s':\n", gettext("Listing attributes for"),
		    composite_name);
		if (list_attrset) {
			print_attrset(list_attrset);
		} else {
			printf("All attributes\n");
		}
	} else {
		printf("%s '%s':\n", gettext("Modifying attributes for"),
		    composite_name);
		print_modlist(modlist);
	}
}
#endif /* DEBUG */

main(int argc, char **argv)
{
	process_cmd_line(argc, argv);
#ifdef DEBUG
	// print_cmd_line();
#endif DEBUG

	FN_status status;
	FN_ctx *init_ctx = FN_ctx::from_initial(authoritative, status);
	check_error(status, "Unable to get initial context");
	void *ip;

	// Composite name
	FN_string name_str(composite_name);
	FN_composite_name name_comp(name_str);

	if (list_mode) {
		if (list_attrset && list_attrset->count() == 1) {
			const FN_attribute *one = list_attrset->first(ip);
			const FN_identifier *id = one->identifier();
			FN_attribute *ret_attribute =
			    init_ctx->attr_get(name_comp, *id, follow_link,
			    status);
			delete init_ctx;
			check_error(status, "Unable to list attribute values");
			if (ret_attribute) {
				fprintf(stdout, "%s: %s\n",
					gettext("Attributes for"),
					composite_name);
				print_attribute(ret_attribute);
			} else
				fprintf(stdout,
				    gettext("No attribute values\n"));
			delete ret_attribute;
		} else {
			FN_multigetlist *ret_attrset =
			    init_ctx->attr_multi_get(name_comp, list_attrset,
			    follow_link, status);
			delete init_ctx;
			check_error(status,
			    "Unable to list attribute set values");
			if (ret_attrset) {
				fprintf(stdout, "%s: %s\n",
					gettext("Attributes for"),
					composite_name);
				print_multigetlist(ret_attrset);
				delete ret_attrset;
			} else
				fprintf(stdout, gettext("No attributes\n"));
		}
	} else {
		// update mode
		if (modlist && modlist->count() == 1) {
			unsigned int mod;
			const FN_attribute *one = modlist->first(ip, mod);
			init_ctx->attr_modify(name_comp, mod, *one,
					    follow_link, status);
			delete init_ctx;
			check_error(status, "Unable to perform modification");
		} else {
			FN_attrmodlist *unexec_mods = NULL;
			init_ctx->attr_multi_modify(name_comp,
			    *modlist, follow_link, &unexec_mods, status);
			delete init_ctx;
			if (!status.is_success()) {
				fprintf(stderr, "%s: %s\n",
					gettext("Error"),
					status.description()->str());
				if (unexec_mods)
					print_modlist(unexec_mods, stderr,
			"Unable to perform the following modifications:\n");
				else
					fprintf(stderr, gettext(
			"No modifications were performed.\n"));
				exit(1);
			}
		}
	}
	exit(0);
}
