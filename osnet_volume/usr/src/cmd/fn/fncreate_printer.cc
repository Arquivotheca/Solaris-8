/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fncreate_printer.cc	1.20	98/02/13 SMI"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <locale.h>
#include <libintl.h>
#include <xfn/xfn.hh>
#include <xfn/fn_p.hh>
#include <xfn/fnselect.hh>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/mman.h>
#include "fncreate_attr_utils.hh"

#define	BUFSIZE 1024
#define	oncPREFIX "onc_"
#define	oncPREFIX1 "onc_printers"
#define	oncPRELEN 4

#define	AUTHORITATIVE 1

static const
FN_string empty_string((const unsigned char *) "");

static const
FN_string service_string((const unsigned char *) "service");

static const
FN_string printer_string((const unsigned char *) "printer");

static const
FN_string internal_name((const unsigned char *) "printers");

// Global variables from command line
static int check_files = 0;
static int check_command = 0;
static int verbose = 0;
static int supercede = 0;
static int naming_service;

#define	DEFAULT_FILE_NAME "/etc/printers.conf"
static char *file_name;
static char *printer_ctx_name;
static char *printer_name;
static const unsigned char **printer_address;
static int num_printer_address;

// external functions
extern FN_ref*
get_service_ref_from_value(const FN_string &, char *);

extern int
file_map(const char *file, char **buffer);

extern int
get_line(char *entry, char *buffer, int pos, int size);

extern char *
get_name(char *in, char **tok);

extern int
get_entry(const char *file, const unsigned char *name, char **value);

static void
check_error(FN_status &status, char *msg = 0)
{
	if (!status.is_success()) {
		if (msg)
			printf("%s\n", msg);
		fprintf(stderr, gettext("Error: %s\n"),
		    status.description()->str());
		exit(1);
	}
}

static void
usage(char *cmd, char *msg = 0)
{
	if (msg)
		fprintf(stderr, "%s: %s\n", cmd, msg);
	fprintf(stderr,
	    gettext("Usage:\t%s [-vs] compositename printername "), cmd);
	fprintf(stderr, gettext("printeraddr [printeraddr ...]\n"));
	fprintf(stderr,
	    gettext("Usage:\t%s [-vs] [-f filename] compositename\n"),
	    cmd);
	exit(1);
}

static void
process_cmd_line(int argc, char **argv)
{
	// Set the naming service
	naming_service = fnselect();
	if (naming_service == FNSP_unknown_ns) {
		fprintf(stderr,
		    gettext("Unknown undelying naming service\n"));
		exit(1);
	}

	int c;
	while ((c = getopt(argc, argv, "f:vs")) != -1) {
		switch (c) {
		case 'v' :
			verbose = 1;
			break;
		case 's' :
			supercede = 1;
			break;
		case 'f':
			check_files = 1;
			file_name = strdup(optarg);
			break;
		case '?':
			default :
			usage(argv[0], "invalid option");
		}
	}

	if (optind >= argc)
		usage(argv[0], "Insufficient command line arguments");

	if (check_files) {
		if (argc == (optind + 1)) {
			printer_ctx_name = strdup(argv[optind++]);
			return;
		} else
			usage(argv[0], "Too many arguments");
	}

	if (argc == (optind + 1)) {
		check_files = 1;
		file_name = strdup(DEFAULT_FILE_NAME);
		printer_ctx_name = strdup(argv[optind++]);
		return;
	}

	if (argc == (optind + 2)) {
		usage(argv[0], "Insufficient command line arguments");
	}

	check_command = 1;
	printer_ctx_name = strdup(argv[optind++]);
	printer_name = strdup(argv[optind++]);
	printer_address = (const unsigned char **) &argv[optind];
	num_printer_address = argc - optind;
}

static FN_ref *
check_printer_name_context(FN_ctx *initial, FN_status &status)
{
	FN_ref *answer;

	FN_string printer_str((unsigned char *) printer_ctx_name);
	FN_composite_name fullname(printer_str);
	FN_composite_name *append_to_printer = 0;
	FN_composite_name *printer_comp = 0;
	FN_string *printer_comp_string;

	// Parse the composite name and remove the service and
	// printer contexts if the exist
	// First remove the empty space if present
	void *ip;
	const FN_string *last_comp = fullname.last(ip);
	if ((last_comp) && (!last_comp->compare(empty_string))) {
		fullname.next(ip); // points after the last comp
		fullname.delete_comp(ip); // deletes the last comp
		last_comp = fullname.last(ip);
	}

	// If the last component is "service" remove it
	if ((last_comp) && (!last_comp->compare(service_string)))
		printer_comp = fullname.prefix(ip);

	// Search for "service/printer"
	if (!printer_comp) {
		// Service name not found, look for service/printer
		while ((last_comp) && (last_comp->compare(printer_string))) {
			if (!append_to_printer)
				append_to_printer = new FN_composite_name;
			append_to_printer->prepend_comp(*last_comp);
			last_comp = fullname.prev(ip);
		}
		last_comp = fullname.prev(ip);
		if ((last_comp) && (!last_comp->compare(service_string))) {
			// Found the combination service/printer
			printer_comp = fullname.prefix(ip);
		} else {
			delete append_to_printer;
			append_to_printer = 0;
		}
	}

	// Reset the printer_name_ctx to the current value
	if (printer_comp) {
		// Construct the new printer context name
		printer_comp_string = printer_comp->string();
		const unsigned char *ctx_name =
			(printer_comp_string ? printer_comp_string->str() : 0);

		// Delete the original printer context name
		if (printer_ctx_name)
			free(printer_ctx_name);
		printer_ctx_name = strdup((char *) ctx_name);
		delete printer_comp_string;
		if (append_to_printer) {
			char *old_printer_name = printer_name;
			FN_string *prstring = append_to_printer->string();
			size_t prstring_len =
			    (prstring? prstring->bytecount() : 0);
			if (old_printer_name)
				printer_name = new char[strlen(old_printer_name)
				    + prstring_len + 2];
			else
				printer_name = new char[prstring_len + 1];

			if (printer_name == NULL || prstring == NULL) {
				status.set_code(FN_E_INSUFFICIENT_RESOURCES);
				check_error(status,
				    gettext(
				"checking validity of printername context"));
			}
			strcpy(printer_name, (char *)prstring->str());
			if (old_printer_name) {
				strcat(printer_name, "/");
				strcat(printer_name, old_printer_name);
				free(old_printer_name);
			}
			delete append_to_printer;
			delete prstring;
		}
	} else
		printer_comp = new FN_composite_name(fullname);

	// If the fisrt component of the printer_comp is org,
	// then we need to add "/" at the end in order to get the
	// correct context. Hence as a default "/" is added before the
	// lookup. Bug Fix #190005

	printer_comp->append_comp(empty_string);

	printer_comp_string = printer_comp->string();

	if (verbose)
		printf(gettext("Lookup performed on %s\n"),
		    printer_comp_string->str());

	answer = initial->lookup(*printer_comp, status);
	if (!status.is_success()) {
		printf(gettext("Context \"%s\" does not exist\n"),
		    printer_comp_string->str());
		printf(
		    gettext("Use fncreate command to create\n"));
		delete printer_comp_string;
		check_error(status);
	}
	delete printer_comp;
	delete printer_comp_string;
	return (answer);
}

static FN_ref*
append_service_context(FN_ctx *initial_ctx)
{
	FN_status status;
	FN_ref *answer;
	FN_composite_name service_comp(service_string);

	answer = initial_ctx->lookup(service_comp, status);
	if (!status.is_success()) {
		// Obtain the context attributes
		FN_attrset *attrs = generate_creation_attrs(
		    FNSP_service_context, FNSP_reftype_from_ctxtype(
		    FNSP_service_context));
		if (attrs == NULL) {
			status.set_code(FN_E_INSUFFICIENT_RESOURCES);
			check_error(status,
			    gettext("Unable to generate attributes to "
			    "create service context"));
		}

		// Create the service context
		answer = initial_ctx->attr_create_subcontext(
		    service_comp, attrs, status);
		delete attrs;
		check_error(status,
		    gettext("Unable to create service context"));
	}
	return (answer);
}

static FN_ref*
append_printer_context(FN_ctx *service_ctx,
    const FN_ref *default_printer)
{
	FN_status status;
	FN_ref *answer;
	int create_printer_context = 0;
	if (verbose)
		fprintf(stdout,
		    gettext("Lookup performed on printer context\n"));
	FN_composite_name printer_comp(printer_string);
	answer = service_ctx->lookup(printer_comp, status);
	if (!status.is_success())
		create_printer_context = 1;
	else {
		// Check if it is a context
		FN_ctx *ctx = FN_ctx::from_ref(*answer, 0, status);
		if (!status.is_success()) {
			service_ctx->unbind(printer_comp, status);
			create_printer_context = 1;
		}
		delete ctx;
	}

	if (create_printer_context) {
		if (verbose)
			fprintf(stdout,
			    gettext(
			    "Printer context does not exist...creating\n"));
		// Create the context attributes
		FN_attrset *attrs = generate_creation_attrs(
		    FNSP_printername_context, FNSP_reftype_from_ctxtype(
		    FNSP_printername_context));
		if (attrs == NULL) {
			status.set_code(FN_E_INSUFFICIENT_RESOURCES);
			check_error(status,
			    gettext("Unable to generate attributes to "
			    "create printer context"));
		}

		// Create the printer context
		answer = service_ctx->attr_create_subcontext(
		    printer_comp, attrs, status);
		check_error(status,
		    gettext("Unable to create printer name context"));

		// Append the default printers
		void *ip;
		const FN_ref_addr *default_addr;
		for (default_addr = default_printer->first(ip);
		    default_addr; default_addr = default_printer->next(ip))
			answer->append_addr(*default_addr);

		// Add the new reference to the printer bindings
		service_ctx->bind(printer_comp, *answer, 0, status);
		check_error(status,
		    gettext("Unable to add default printers "
		    "to printer context"));
	}
	return (answer);
}

FN_ref_addr*
get_printer_addr_from_value(const unsigned char *value)
{
	char	*typestr, *p;
	XDR	xdr;
	FN_ref_addr *answer = 0;

	char *addrstr, addrt[BUFSIZE];
	u_char buffer[BUFSIZE];

	p = (char *) value;
	typestr = p;
	do {
		if (*p == '=') {
			*p++ = '\0';
			addrstr = p;

			// Create the address identifier
			if ((strlen(oncPREFIX1) + strlen(typestr)) >= BUFSIZE)
				return (0);
			sprintf(addrt, "%s_%s", oncPREFIX1, typestr);
			FN_identifier addrtype((unsigned char *)addrt);

			// XDR the adddress
			xdrmem_create(&xdr, (caddr_t)buffer, BUFSIZE,
			    XDR_ENCODE);
			if (xdr_string(&xdr, &addrstr, ~0) == FALSE)
				return (0);

			// Create the FN_ref_addr
			answer = new FN_ref_addr(addrtype, xdr_getpos(&xdr),
			    (void *) buffer);
			xdr_destroy(&xdr);

			return (answer);
		} else
			p++;
	} while (*p != '\0');
	return (0);
}

FN_ref *
obtain_printer_object_ref()
{
	FN_ref_addr *printer_addr;
	FN_ref *answer = new FN_ref((unsigned char *) oncPREFIX1);

	for (int i = 0; i < num_printer_address; i++) {
		printer_addr =
		    get_printer_addr_from_value(printer_address[i]);
		if (!printer_addr) {
			fprintf(stderr,
			    gettext("Incorrect printer address: %s\n"),
			    printer_address[i]);
			exit(1);
		}
		answer->append_addr(*printer_addr);
		delete printer_addr;
	}
	return (answer);
}

FN_ctx *
create_inter_printers(const FN_string *printer_name, FN_ctx *ctx,
    FN_ref *printer_ref)
{
	FN_status status;
	FN_ref *answer;

	if (verbose)
		fprintf(stdout, gettext("Lookup performed on %s\n"),
		    printer_name->str());
	answer = ctx->lookup(*printer_name, status);
	if (!answer) {
		if (verbose)
			fprintf(stdout,
			    gettext("Printer %s does not exist...creating\n"),
			    printer_name->str());
		answer = ctx->create_subcontext(*printer_name, status);
		check_error(status,
		    gettext("Unable to create printer object context"));

		// Append the default printers
		void *ip;
		const FN_ref_addr *default_addr	= printer_ref->first(ip);
		while (default_addr) {
			answer->append_addr(*default_addr);
			default_addr = printer_ref->next(ip);
		}
		ctx->bind(*printer_name, *answer, 0, status);
		check_error(status,
		    gettext("Unable to bind printer object"));
	}

	// Obtain the printer context and return
	FN_ctx *printer_ctx =
	    FN_ctx::from_ref(*answer, AUTHORITATIVE, status);
	if (!status.is_success()) {
		// Convert the binding into a context
		// First, delete the bindings
		ctx->unbind(*printer_name, status);
		check_error(status,
		    gettext("Unable to unbind printer bindings"));

		// Create the subcontext
		FN_ref *ctx_ref = ctx->create_subcontext(*printer_name, status);
		check_error(status,
		    gettext("Unable to create printer object context"));

		// Append the printer addresses
		void *prt_ip;
		const FN_ref_addr *printer_ref_addr = answer->first(prt_ip);
		while (printer_ref_addr) {
			ctx_ref->append_addr(*printer_ref_addr);
			printer_ref_addr = answer->next(prt_ip);
		}
		ctx->bind(*printer_name, *ctx_ref, 0, status);
		check_error(status,
		    gettext("Unable to bind printer object"));
		printer_ctx = FN_ctx::from_ref(*ctx_ref, AUTHORITATIVE, status);
		check_error(status,
		    gettext("Unable to get printer object context"));
		delete ctx_ref;
	}
	delete answer;
	return (printer_ctx);
}

static void
FNSP_check_and_add_sspc_address(FN_ref *ref)
{
	char sspc_address[BUFSIZE];

	switch (naming_service) {
	case FNSP_nisplus_ns:
		strcpy(sspc_address, "sspc___printer_nisplus");
		break;
	case FNSP_nis_ns:
		strcpy(sspc_address, "sspc___printer_nis");
		break;
	case FNSP_files_ns:
		strcpy(sspc_address, "sspc___printer_files");
		break;
	default:
		strcpy(sspc_address, "sspc___printer_unknown");
		break;
	}

	FN_identifier sspc_identifier((unsigned char *) sspc_address);
	const FN_ref_addr *addr;
	void *ip;
	for (addr = ref->first(ip); addr; addr = ref->next(ip)) {
		if (*addr->type() == sspc_identifier)
			return;
	}

	// Address not present, hence add
	FN_ref_addr sspc_ref_address(sspc_identifier,
	    strlen("sspc_information"), "sspc_information");
	// ref->prepend_addr(sspc_ref_address);
	ref->append_addr(sspc_ref_address);
}

FN_ref *
create_final_printers(const FN_string *printer_name, FN_ctx *ctx,
    FN_ref *printer_ref)
{
	FN_status status;
	FN_ref *answer = ctx->lookup(*printer_name, status);

	if (verbose) {
		fprintf(stdout, gettext("Lookup performed on %s\n"),
		    printer_name->str());
		if (answer)
			fprintf(stdout, gettext("Printer: %s already"
			    " exists\nTrying to replace the old printer"
			    " address with the new printer address\n"),
			    printer_name->str());
		else
			fprintf(stdout,
			    gettext("Printer %s not present...creating\n"),
			    printer_name->str());
	}

	if (!answer) {
		// %%% For the efficient lookup of printers,
		// printer bindings are created instead of contexts
		answer = new FN_ref(*FNSP_reftype_from_ctxtype(
		    FNSP_printer_object));

		// Append the default printers
		void *ip;
		const FN_ref_addr *default_addr	=
			printer_ref->first(ip);
		while (default_addr) {
			answer->append_addr(*default_addr);
			default_addr = printer_ref->next(ip);
		}

		// Append the name_service specific addresses
		FNSP_check_and_add_sspc_address(answer);

		// Perform the bind operation
		ctx->bind(*printer_name, *answer, 0, status);
		check_error(status,
		    gettext("Unable to bind printer object"));
		if (verbose)
			fprintf(stdout,
			   gettext("Created new printer bindings\n"));
	} else {
		// Check if the same address is present.
		// If present check for supercede flag
		if (!supercede)
			fprintf(stdout,
			    gettext("Printer: %s already exists\n"),
			    printer_name->str());
		if (verbose)
			fprintf(stdout,
			    gettext("Checking for same address"
			    " type binding\n"));
		void *ref_ip, *prt_ip;
		const FN_ref_addr *ref_address, *prt_address;
		const FN_identifier *ref_id, *prt_id;
		prt_address = printer_ref->first(prt_ip);

		int bind = 1;
		while (prt_address) {
			prt_id = prt_address->type();
			ref_address = answer->first(ref_ip);
			while (ref_address) {
				ref_id = ref_address->type();
				if ((*ref_id) == (*prt_id)) {
					if (verbose)
						fprintf(stdout,
						    gettext("Binding with "
						    "\"%s\" address type "
						    "exists\n"),
						    ref_address->type()->str());
					if (supercede) {
						if (verbose)
							fprintf(stdout,
							    gettext("Overwri"
							    "ting\n"));
						answer->delete_addr(ref_ip);
						answer->insert_addr(ref_ip,
						    *prt_address);
					} else {
						fprintf(stdout,
						    gettext("Use -s option "
						    "to over-write\n"));
						bind = 0;
					}
					break;
				}
				ref_address = answer->next(ref_ip);
			}
			if (!ref_address) {
				answer->append_addr(*prt_address);
				if (verbose)
					fprintf(stdout, gettext("New printer"
					    " address appended to the "
					    "printer name\n"));
			}
			prt_address = printer_ref->next(prt_ip);
		}
		if (bind) {
			// Also check if sspc address is present
			FNSP_check_and_add_sspc_address(answer);
			ctx->bind(*printer_name, *answer, 0, status);
		}
		check_error(status,
		    gettext("Unable to bind printer object"));
	}
	return (answer);
}

void static
create_printers(FN_ref *printer_ref, FN_status &status,
    FN_ref *printer_object_ref)
{
	FN_ctx *printer_ctx = FN_ctx::from_ref(*printer_ref,
	    AUTHORITATIVE, status);
	check_error(status,
	    gettext("Unable to obtain printer name context"));

	// Obtain the printer object composite name
	// and traverse the components
	FN_string printer_string((unsigned char *) printer_name);
	FN_composite_name printer_comp(printer_string);
	const FN_string *prt_comp_string;
	const FN_string *prt_comp_string_next;
	FN_ctx *parent_ctx = printer_ctx;
	void *ip;

	prt_comp_string = printer_comp.first(ip);
	prt_comp_string_next = printer_comp.next(ip);
	while (prt_comp_string) {
		if (prt_comp_string_next) {
			printer_ctx = create_inter_printers(prt_comp_string,
			    parent_ctx, printer_object_ref);
			prt_comp_string = prt_comp_string_next;
			prt_comp_string_next = printer_comp.next(ip);
		} else {
			FN_ref *prt_ref = create_final_printers(prt_comp_string,
			    parent_ctx, printer_object_ref);
			delete prt_ref;
			prt_comp_string = 0;
			printer_ctx = 0;
		}
		delete parent_ctx;
		parent_ctx = printer_ctx;
	}
}

FN_ctx *
create_files_inter_printers(const FN_ref *printer_ref, FN_status &status,
    FN_ref *printer_object_ref)
{
	// Obtain the printer name context
	FN_ctx *parent_ctx =
	    FN_ctx::from_ref(*printer_ref, AUTHORITATIVE, status);
	check_error(status,
	    gettext("Unable to obtain printer name context"));

	// If printer_name is NULL return the reference
	if (!printer_name)
		return (parent_ctx);

	// Obtain the printer object composite name
	// and traverse the components
	FN_string printer_string((unsigned char *) printer_name);
	FN_composite_name printer_comp(printer_string);
	FN_ctx *printer_ctx;
	const FN_string *prt_comp_string;
	void *ip;

	prt_comp_string = printer_comp.first(ip);
	while (prt_comp_string) {
		printer_ctx = create_inter_printers(prt_comp_string,
		    parent_ctx, printer_object_ref);
		prt_comp_string = printer_comp.next(ip);
		delete parent_ctx;
		parent_ctx = printer_ctx;
	}
	return (printer_ctx);
}

#include <rpcsvc/nis.h>
static void
check_permission_for_nisplus()
{
	char *local_dir = nis_local_directory();
	// Check for NIS+
	nis_result *res;
	char nis_domain[NIS_MAXNAMELEN+1];
	sprintf(nis_domain, "org_dir.%s", local_dir);
	res = nis_lookup(nis_domain, NO_AUTHINFO | USE_DGRAM);
	if (res->status != NIS_SUCCESS) {
		fprintf(stderr, gettext("NIS+ is not installed\n"
		    "Cannot use this command in this environment\n"));
		exit(1);
	}

	// Check for ctx_dir
	sprintf(nis_domain, "ctx_dir.%s", local_dir);
	res = nis_lookup(nis_domain, NO_AUTHINFO | USE_DGRAM);
	if (res->status != NIS_SUCCESS) {
		fprintf(stderr, gettext("FNS not installed\n"
		    "Use: fncreate -t org command to install\n"));
		exit(1);
	}
}

#include <sys/types.h>
#include <sys/stat.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>
static void
check_permission_for_nis()
{
	// Make sure the lock file exists
	struct stat buffer;
	char *domain, fns_dir[BUFSIZE];
	yp_get_default_domain(&domain);
	sprintf(fns_dir, "/etc/fn/%s/fns.lock", domain);
	if (stat(fns_dir, &buffer) != 0) {
		fprintf(stderr, gettext("FNS for NIS is not installed\n"
		    "Cannot use this command in this environment\n"
		    "Use: fncreate -t org command to install\n"));
		exit(1);
	}

	// Check for root permissions
	if (geteuid() != 0) {
		fprintf(stderr, gettext("fncreate_printer: "
		    "No root permissions\n"));
		exit(1);
	}
}

static void
check_permission_for_files()
{
	struct stat buffer;
	char fns_dir[BUFSIZE];
	sprintf(fns_dir, "/var/fn/fns.lock");
	if (stat(fns_dir, &buffer) != 0) {
		fprintf(stderr, gettext("FNS for files is not installed\n"
		    "Cannot use this command in this environment\n"
		    "Use: fncreate -t org command to install\n"));
		exit(1);
	}
}

static void
check_permissions_for_fncreate_printer()
{
	switch (naming_service) {
	case FNSP_nisplus_ns:
		check_permission_for_nisplus();
		break;
	case FNSP_nis_ns:
		check_permission_for_nis();
		break;
	case FNSP_files_ns:
		check_permission_for_files();
		break;
	}
}

main(int argc, char **argv)
{
	// Internationalization
	setlocale(LC_ALL, "");

	process_cmd_line(argc, argv);
	check_permissions_for_fncreate_printer();

	FN_status status;
	FN_ctx *init_ctx = FN_ctx::from_initial(AUTHORITATIVE, status);
	check_error(status,
	    gettext("Unable to get initial context"));

	const FN_composite_name *remaining_name = 0;
	FN_ref *context_ref = check_printer_name_context(init_ctx, status);
	delete init_ctx;

	// Obtain the context in which the service/printer to be
	// created
	FN_ctx *context_ctx = FN_ctx::from_ref(*context_ref,
	    AUTHORITATIVE, status);
	check_error(status,
	    gettext("Unable to get printer context context"));
	delete context_ref;

	// Check for the existence of the service context
	// If not present create one
	FN_ref *service_ref = append_service_context(context_ctx);
	delete context_ctx;

	// Obtain service context
	FN_ctx *service_ctx = FN_ctx::from_ref(*service_ref,
	    AUTHORITATIVE, status);
	check_error(status,
	    gettext("Unable to obtain service context"));
	delete service_ref;

	FN_ref *printer_ref = 0;
	FN_ctx *printer_ctx = 0;
	if (check_command) {
		// Obtain the printer object reference
		FN_ref *printer_object_ref = obtain_printer_object_ref();

		// Check for printer context, if not present
		// create one and append the default printer
		printer_ref = append_printer_context(service_ctx,
		    printer_object_ref);
		delete service_ctx;
		create_printers(printer_ref, status, printer_object_ref);
		check_error(status,
		    gettext("Unable to create printers"));
		exit(0);
	}


	// Files implementation
	int size, pos = 0;
	char entry[BUFSIZ];
	char *buffer, *name, *p, *tmp;
	FN_ref *ref;
	FN_string *name_str;

	if ((size = file_map(file_name, &buffer)) < 0) {
		fprintf(stderr, gettext("Insufficient resources (or) "));
		fprintf(stderr, gettext("File not found: "));
		fprintf(stderr, "%s\n", file_name);
		exit(1);
	}

	do {
		pos = get_line(entry, buffer, pos, size);
		tmp = p = strdup(entry);
		while ((p = get_name(p, &name)) || name) {
			char *in;
			in = strdup(entry);
			ref = get_service_ref_from_value(internal_name, in);

			if (ref) {
				if (!printer_ctx) {
					FN_ref *inter_ref =
					    append_printer_context(
					    service_ctx, ref);
					delete service_ctx;

					// Update the intermediate printers
					printer_ctx =
					    create_files_inter_printers(
					    inter_ref, status, ref);
					delete inter_ref;
				}
				name_str = new
				    FN_string((const unsigned char *) name);
				printer_ref = create_final_printers(name_str,
				    printer_ctx, ref);
				delete name_str;
			}
			free(in);
			if (p == 0)
				break;
		}
		free(tmp);
	} while (pos < size);

	(void) munmap(buffer, size);
	exit(0);
}
