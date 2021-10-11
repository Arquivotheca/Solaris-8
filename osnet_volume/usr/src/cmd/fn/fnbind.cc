/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnbind.cc	1.8	96/03/31 SMI"


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <rpc/rpc.h>  /* for XDR */
#include <libintl.h>  /* gettext() */

#include <xfn/xfn.hh>

// Options:
// -L	create a link using 'name' and bind it to 'new_name'
// -s	supercede the binding of 'new_name' if it was already bound
// -v	run in verbose mode
//
// The second synopsis of fnbind allows the binding of 'name' to
// a reference constructed using arguments supplied in the command line.
// -r	create a reference using 'ref_type' as the reference's
//	type, and one or more pairs of 'addr_type' and 'addr_contents'
//	as the reference's list of addresses, and bind this reference
//	to 'name'.  Unless the -O or -U options are used,
//	FN_ID_STRING is used as the identifier format for
//	'ref_type' and 'addr_type'.  Unless -c or -x options are used,
//	'addr_contents' is stored as an XDR-encoded string.
//
// -U	The identifier format is FN_ID_DCE_UUID, a DCE UUID in
//	string form.
//
// -O	The identifier format is FN_ID_ISO_OID_STRING, an ASN.1
//	dot-separated integer list string.
//
// -c	Store 'addr_contents' in the given form; do not use
//	XDR-encoding.
//
// -x	'addr_contents' specifies an hexidecimal string.  Convert it
//	to its hexidecimal representation and store it; do not
//	use XDR-encoding.

static int	arg_bind_supercede = 0;
static int	arg_verbose = 0;
static char	*arg_name = 0;
static char	*arg_new_name = 0;
static int	arg_bind_as_link = 0;
static int	arg_build_reference = 0;

#define	__MAX_ADDRESSES 5
static int addr_type_count = 0;
static int addr_contents_count = 0;
static FN_identifier *addr_type[__MAX_ADDRESSES];
static void *addr_contents[__MAX_ADDRESSES];
static size_t addr_contents_len[__MAX_ADDRESSES];
static FN_identifier *ref_type = 0;


void
usage(char *cmd, char *msg = 0)
{
	if (msg)
		fprintf(stderr, "%s: %s\n", cmd, gettext(msg));
	fprintf(stderr, "%s:\t%s [-svL] %s\n", gettext("Usage"),
		cmd, gettext("name new_name"));
	fprintf(stderr,
		"\t%s -r [-sv] %s [-O|-U] %s {[-O|-U] %s [-c|-x] %s}+\n",
		cmd, gettext("new_name"), gettext("ref_type"),
		gettext("addr_type"),
		gettext("addr_contents"));
	exit(1);
}

static unsigned int
get_id_format(char format)
{
	switch (format) {
	case 'O':
		return (FN_ID_ISO_OID_STRING);
	case 'U':
		return (FN_ID_DCE_UUID);
	case 'S':
	default:
		return (FN_ID_STRING);
	}
}

static inline void
set_reference_type(char format, const char *type_str)
{
	size_t len = strlen(type_str);
	ref_type = new FN_identifier(get_id_format(format),
				    len, (const void *)type_str);
}

static int
add_address_type(char format, const char *type_str, char **err)
{
	if (addr_type_count >= __MAX_ADDRESSES) {
		*err = "too many addresses";
		return (0);
	}

	if (addr_type_count == addr_contents_count) {
		size_t len = strlen(type_str);
		addr_type[addr_type_count] =
			new FN_identifier(get_id_format(format),
					    len, (const void *)type_str);
		++addr_type_count;
		return (1);
	} else {
		*err = "expecting address contents";
		return (0);
	}
}

#define	BUFSIZE 1024

static char *
xdr_encode_string(const char *str, size_t &len, char **err)
{
	u_char	buffer[BUFSIZE];
	XDR	xdr;
	char *	answer;

	xdrmem_create(&xdr, (caddr_t)buffer, BUFSIZE, XDR_ENCODE);
	if (xdr_string(&xdr, (char **)&str, ~0) == FALSE) {
		*err = "could not XDR encode string address";
		return (0);
	}

	len = xdr_getpos(&xdr);
	answer = (char *)malloc(len);
	if (answer == 0) {
		*err = "insufficient memory";
		return (0);
	}

	memcpy(answer, buffer, len);
	return (answer);
}

static unsigned char *
string_to_hex_repr(const char *str, size_t &len, char **err)
{
	size_t true_len = (len >> 1);  // divide by 2
	unsigned char *answer = (unsigned char *)malloc(true_len);
	int i, j;
	char byte[3];
	char *err_indicator;
	long byte_val;

	if (answer == 0) {
		*err = "insufficient memory";
		return (0);
	}

	byte[2] = '\0';
	for (j = 0, i = 0; j < true_len && i < len; j++, i += 2) {

		byte[0] = str[i];
		byte[1] = str[i+1];

		byte_val = strtol(byte, &err_indicator, 16);
		if (err_indicator != &(byte[2]))
			break;
		answer[j] = (unsigned char)byte_val;
	}

	if (j != true_len || i != len) {
		*err = "invalid hex string";
		free(answer);
		return (0);
	}
	len = true_len;
	return (answer);
}


static inline int
add_address_contents(char encoding, const char *contents, char **err)
{
	void *answer;
	size_t len;

	if (addr_contents_count != (addr_type_count - 1)) {
		*err = "expecting address type";
		return (0);  // overrunning address type
	}

	if (addr_contents_count > __MAX_ADDRESSES) {
		*err = "too many addresses";
		return (0);  // too many
	}

	switch (encoding) {
	case 'c':
		answer = (void *)strdup(contents);
		len = strlen(contents);
		if (answer == 0)
			*err = "insufficient memory";
		break;
	case 'x':
		len = strlen(contents);
		answer = (void *)string_to_hex_repr(contents, len, err);
		break;
	case 's':
	default:
		answer = (void *)xdr_encode_string(contents, len, err);

	}

	if (answer == 0)
		return (0);

	addr_contents[addr_contents_count] = answer;
	addr_contents_len[addr_contents_count] = len;
	++addr_contents_count;
	return (1);
}

void
process_cmd_line(int argc, char **argv)
{
	int c;
	char *err;
getopt_again:
	while ((c = getopt(argc, argv, "svLrO:U:c:x:")) != -1) {
		switch (c) {
		case 'L':
			if (arg_build_reference)
				usage(argv[0],
				    "invalid combination of options");
			arg_bind_as_link = 1;
			break;
		case 's' :
			arg_bind_supercede = 1;
			break;
		case 'v' :
			arg_verbose = 1;
			break;
		case 'r':
			if (arg_bind_as_link)
				usage(argv[0],
				    "invalid combination of options");
			arg_build_reference = 1;
			break;
		case 'O':
		case 'U':
			if (!arg_build_reference)
				usage(argv[0],
				    "invalid combination of options");
			if (ref_type == 0)
				set_reference_type(c, optarg);
			else if (add_address_type(c, optarg, &err) == 0)
				usage(argv[0], err);
			break;
		case 'c':
		case 'x':
			if (!arg_build_reference)
				usage(argv[0],
				    "invalid combination of options");
			if (add_address_contents(c, optarg, &err) == 0)
				usage(argv[0], err);
			break;
		case '?':
		default :
			usage(argv[0], "invalid option");
		}
	}

	if (!arg_build_reference) {
		if (optind < argc)
			arg_name = argv[optind++];
		else
			usage(argv[0], "missing name to find reference");

		if (optind < argc)
			arg_new_name = argv[optind++];
		else
			usage(argv[0], "missing name to be bound");

		if (optind < argc)
			usage(argv[0], "too many arguments");
	} else {
		// building reference from command line args
		if (arg_name == 0) {
			if (optind < argc)
				arg_name = argv[optind++];

			else
				usage(argv[0], "missing name for binding");
			goto getopt_again;
		} else if (ref_type == 0) {
			if (optind < argc)
				set_reference_type('S', argv[optind++]);
			else
				usage(argv[0], "no reference type specified");
			goto getopt_again;
		} else {
			if (optind < argc) {
				char *str = argv[optind++];
				if (add_address_type('S', str, &err))
					goto getopt_again;
				if (add_address_contents('e', str, &err))
					goto getopt_again;
				usage(argv[0], err);
			} else {
				if (addr_type_count == 0)
					usage(argv[0], "no address specified");
				if (addr_contents_count < addr_type_count)
					usage(argv[0],
					    "missing address contents");
				/* done */
			}
		}
	}
}

// uses ref_type, addr_* globals
FN_ref *
create_reference()
{
	FN_ref *ref = new FN_ref(*ref_type);
	FN_ref_addr *addr;
	int i;

	if (ref == 0)
		return (0);

	for (i = 0; i < addr_type_count; i++) {
		addr = new FN_ref_addr(*addr_type[i],
		    addr_contents_len[i], addr_contents[i]);
		if (addr == 0) {
			delete ref;
			return (0);
		}
		ref->append_addr(*addr);
		delete addr;
	}
	return (ref);
}

int
create_ref_and_bind(FN_ctx* ctx, const FN_string& new_name)
{
	FN_status status;
	unsigned bind_flags = (arg_bind_supercede ? 0 : FN_OP_EXCLUSIVE);
	FN_string *desc;

	if (arg_verbose) {
		printf("%s '%s'.\n", gettext("Creating reference for"),
		    (char *)new_name.str());
	}

	FN_ref* ref = create_reference();


	if (ref == 0) {
		fprintf(stderr, "%s '%s'\n",
			gettext("Could not create reference for"),
			(char *)new_name.str());
		return (1);
	}

	if (arg_verbose) {
		desc = ref->description(2);
		printf("%s", (desc ? (char *)desc->str() :
			    gettext("No reference description")));
		delete desc;
	}

	if (ctx->bind(new_name, *ref, bind_flags, status) == 0) {
		desc = status.description();
		fprintf(stderr, "%s '%s' %s: %s\n",
			gettext("Bind of"),
			(char *)(new_name.str()),
			gettext("failed"),
			(desc ? (char *)desc->str() :
			    gettext("No status description")));
		delete desc;
		delete ref;
		return (1);
	}
	delete ref;
	return (0);
}

int
create_link_and_bind(FN_ctx* ctx,
    const FN_string& link_name,
    const FN_string& new_name)
{
	FN_status status;
	unsigned bind_flags = (arg_bind_supercede ? 0 : FN_OP_EXCLUSIVE);
	FN_string *desc;

	if (arg_verbose) {
		printf("%s '%s' to '%s'.\n", gettext("Linking"),
		    (char *)new_name.str(), (char *)link_name.str());
	}

	FN_ref* link_ref = FN_ref::create_link(link_name);

	if (link_ref == 0) {
		fprintf(stderr, "%s '%s'\n",
			gettext("Could not create link for"),
			(char *)link_name.str());
		return (1);
	}

	if (arg_verbose) {
		desc = link_ref->description();
		printf("%s", (desc ? (char *)desc->str() :
		    gettext("No reference description")));
		delete desc;
	}

	if (ctx->bind(new_name, *link_ref, bind_flags, status) == 0) {
		desc = status.description();
		fprintf(stderr, "%s '%s' failed: %s\n",
			gettext("Bind of"),
			(char *)(new_name.str()),
			(desc ? (char *)desc->str() :
			    gettext("No status description")));
		delete desc;
		delete link_ref;
		return (1);
	}
	delete link_ref;
	return (0);
}



// returns 1 on failure and 0 on success

int
lookup_and_bind(
	FN_ctx *ctx,
	const FN_string &existing_name,
	const FN_string &new_name)
{
	FN_status status;
	FN_ref *ref = ctx->lookup(existing_name, status);
	unsigned bind_flags = (arg_bind_supercede? 0: FN_OP_EXCLUSIVE);
	FN_string *desc = 0;

	if (ref) {
		if (arg_verbose) {
			printf("%s '%s' %s '%s':\n",
			    gettext("Binding"),
			    (char *)existing_name.str(),
			    gettext("to reference of"),
			    (char *)new_name.str());
			desc = ref->description(2);
			printf("%s",
			    (desc ? (char *)desc->str() :
			    gettext("No status description")));
			delete desc;
		}
		if (ctx->bind(new_name, *ref, bind_flags, status) == 0) {
			desc = status.description();
			fprintf(stderr, "%s '%s' %s: %s\n",
				gettext("Bind of"),
				(char *)(new_name.str()),
				gettext("failed"),
				(desc ? (char *)desc->str() :
				    gettext("No status description")));
			delete desc;
			delete ref;
			return (1);
		}
	} else {
		desc = status.description();
		fprintf(stderr, "%s '%s' %s: %s\n",
			gettext("Lookup of"),
			(char *)(existing_name.str()),
			gettext("failed"),
			(desc ? (char *)desc->str() :
			    gettext("No status description")));
		delete desc;
		return (1);
	}
	return (0);
}


int
main(int argc, char **argv)
{
	process_cmd_line(argc, argv);
	unsigned int authoritative = 1;
	int exit_status = 0;

	FN_status status;
	FN_ctx* ctx = FN_ctx::from_initial(authoritative, status);

	if (ctx == 0) {
		FN_string *desc = status.description();
		fprintf(stderr, "%s %s\n",
			gettext("Unable to get initial context!"),
			desc? (char *)(desc->str()):
			gettext("No status description"));
		delete desc;
		exit(1);
	}

	if (arg_bind_as_link)
		// make reference out of link and bind
		exit_status = create_link_and_bind(
		    ctx, (unsigned char *)arg_name,
		    (unsigned char *)arg_new_name);
	else if (arg_build_reference)
		// make reference out of args and bind
		exit_status = create_ref_and_bind(ctx,
		    (unsigned char *)arg_name);

	else
		// look up reference and bind
		exit_status = lookup_and_bind(ctx,
		    (unsigned char *)arg_name,
		    (unsigned char *)arg_new_name);

	delete ctx;
	exit(exit_status);
}
