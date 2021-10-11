/*
 * Copyright (c) 1992 - 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fncreate.cc	1.32	98/11/10 SMI"


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/systeminfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/nis.h>
#include <libintl.h> // gettext
#include <xfn/xfn.hh>
#include <xfn/fn_p.hh>
#include <xfn/fnselect.hh>
#include "fncreate_attr_utils.hh"

/*
 *
 * The following are being used from fnsp_internal.cc.
 * Some other source organization (such as a small utilities object)
 * might obviate the need to include the need to link against the .so.
 *
 * FNSP_create_directory()
 * FNSP_orgname_of()
 *
 */

extern
unsigned FNSP_create_directory(const FN_string &, unsigned int access_flags);

extern FN_string *
FNSP_orgname_of(const FN_string &internal_name, unsigned &status,
    int org = 0);

/*
 *
 * The following are being used from fnsp__nis_internal.cc.
 * Some other source organization (such as a small utilities object)
 * might obviate the need to link against the .so.
 *
 * FNSP_nis_orgname_of()
 *
 */

extern FN_string *
FNSP_nis_orgname_of(FN_string &name, unsigned &status);


#define	AUTHORITATIVE 1

static int name_service;
static int create_subcontexts_p = 1;   // default is to create subcontexts
static int verbose = 0;
static char *target_name_str = 0;
static unsigned global_bind_flags = FN_OP_EXCLUSIVE;
static unsigned context_type;
static char *input_file = 0;
static char *reference_type = 0;

static char *program_name  = 0;

static int
__fns_xdr_encode_string(const char *str, char *result, size_t &len);

#define	FNSP_fs_context 101

#define	USER_SOURCE "passwd"
#define	HOST_SOURCE "hosts"
#define	UH_MAX_LEN 20
static char source[UH_MAX_LEN];

static char *
FNSP_get_user_source_table()
{
	switch (name_service) {
	case FNSP_nis_ns:
		sprintf(source, "%s.byname", USER_SOURCE);
		break;
	case FNSP_files_ns:
		sprintf(source, "/etc/%s", USER_SOURCE);
		break;
	case FNSP_nisplus_ns:
	default:
		sprintf(source, "%s.org_dir", USER_SOURCE);
		break;
	}
	return (source);
}

static char *
FNSP_get_host_source_table()
{
	switch (name_service) {
	case FNSP_nis_ns:
		sprintf(source, "%s.byname", HOST_SOURCE);
		break;
	case FNSP_files_ns:
		sprintf(source, "/etc/%s", HOST_SOURCE);
		break;
	case FNSP_nisplus_ns:
	default:
		sprintf(source, "%s.org_dir", HOST_SOURCE);
		break;
	}
	return (source);
}

// Structure to hold information about contexts, names etc
// during the creations of a lists of users' and hosts'
struct traverse_data {
	FN_ctx *parent;
	const FN_ref *parent_ref;
	const FN_composite_name *parent_name;
	const FN_string *domain_name;
	int subcontext_p;
	int count;
};

int traverse_user_list(
	const FN_string &domainname,
	FN_ctx *parent,
	const FN_ref &parent_ref,
	const FN_composite_name &parent_name,
	int sub);

int traverse_host_list(
	const FN_string &domainname,
	FN_ctx *parent,
	const FN_ref &parent_ref,
	const FN_composite_name &parent_name,
	int sub);

// ---------------------------------------------------------------------
// Code for dealing with NIS+ tables

#include <rpcsvc/nis.h>

int process_host_entry(char *, nis_object *ent, void *udata);


// Code for dealing with NIS and files entry in hosts.byname map and
// /etc/passwd file
int process_nis_host_entry(char *, void *udata);

/* ******************************************************************* */

// for host and user functions

static const FN_string FNSP_service_string((unsigned char *)"service");
static const FN_string FNSP_service_string_sc((unsigned char *)"_service");
static const FN_string FNSP_host_string((unsigned char *)"host");
static const FN_string FNSP_host_string_sc((unsigned char *)"_host");
static const FN_string FNSP_user_string((unsigned char *)"user");
static const FN_string FNSP_user_string_sc((unsigned char *)"_user");
static const FN_string FNSP_site_string_sc((unsigned char *)"_site");
static const FN_string FNSP_site_string((unsigned char *)"site");
static const FN_string FNSP_fs_string_sc((unsigned char *)"_fs");
static const FN_string FNSP_fs_string((unsigned char *)"fs");

static const FN_string FNSP_empty_component((unsigned char *)"");
static const FN_string NISPLUS_separator((unsigned char *)".");

void
usage(char *cmd, char *msg = 0)
{
	if (msg)
		fprintf(stderr, "%s: %s\n", cmd, gettext(msg));
	fprintf(stderr,
		gettext("Usage:\t%s -t org|hostname|username|host|user|"
		"service|site|generic|nsid|fs\n"), cmd);
	fprintf(stderr, gettext("\t\t[-osv] [-f input_filename] "
	    "[-r reference_type] composite_name\n"));
	exit(1);
}

// Note that we're only using the FNSP constants here instead of
// inventing a new set.  The type of context to be created do not
// always correspond exactly to these types.
unsigned
get_context_type_from_string(char *ctx_type_str)
{
	if (strcmp(ctx_type_str, "org") == 0)
		return (FNSP_organization_context);
	else if (strcmp(ctx_type_str, "hostname") == 0)
		return (FNSP_hostname_context);
	else if (strcmp(ctx_type_str, "username") == 0)
		return (FNSP_username_context);
	else if (strcmp(ctx_type_str, "host") == 0)
		return (FNSP_host_context);
	else if (strcmp(ctx_type_str, "user") == 0)
		return (FNSP_user_context);
	else if (strcmp(ctx_type_str, "service") == 0)
		return (FNSP_service_context);
	else if (strcmp(ctx_type_str, "site") == 0)
		return (FNSP_site_context);
	else if (strcmp(ctx_type_str, "generic") == 0)
		return (FNSP_generic_context);
	else if (strcmp(ctx_type_str, "nsid") == 0)
		return (FNSP_nsid_context);
	else if (strcmp(ctx_type_str, "fs") == 0)
		return (FNSP_fs_context);

	return (0);
}


void
process_cmd_line(int argc, char **argv)
{
	int c;
	char *ctx_type_str = 0;
	while ((c = getopt(argc, argv, "ost:vf:r:")) != -1) {
		switch (c) {
		case 'o' :
			create_subcontexts_p = 0;
			break;
		case 's' :
			global_bind_flags = 0;
			break;
		case 'v' :
			verbose = 1;
			break;
		case 't':
			ctx_type_str = optarg;
			break;
		case 'f':
			input_file = (optarg? strdup(optarg) : 0);
			break;
		case 'r':
			reference_type =  (optarg? strdup(optarg) : 0);
			break;
		case '?':
			default :
			usage(argv[0], "invalid option");
		}
	}

	if (optind < argc)
		target_name_str = argv[optind++];
	else
		usage(argv[0], "missing composite name of context to create");

	if (optind < argc)
		usage(argv[0], "too many arguments");

	if (ctx_type_str == 0)
		usage(argv[0], "missing type of context to create");
	else
		context_type = get_context_type_from_string(ctx_type_str);

	if (context_type == 0)
		usage(argv[0], "invalid context type");

	if (input_file && (context_type != FNSP_hostname_context &&
	    context_type != FNSP_username_context))
		usage(argv[0],
		"-f option can only be used with hostname or username types");

	if (reference_type != NULL && context_type != FNSP_generic_context)
		usage(argv[0],
		    "-r option can only be used with generic context type");

	program_name = strdup(argv[0]);
}

// ---------------------------------------------------
// Function to update makefile in /var/yp and
// create makefile for FNS in /etc/fn/'domainname'
// if it does not exist.
// ---------------------------------------------------
static char *FNSP_nis_dir = "/var/yp";
static char *FNSP_nis_map_dir = "/etc/fn";
static char *FNSP_org_map = "fns_org.ctx";
static char *FNSP_lock_file = "fns.lock";
static char *MAKE_FILE = "Makefile";
#define	FNS_NIS_INDEX 256
#define	FNS_NIS_SIZE 1024

static int
FNSP_match_map_index(const char *line, const char *name)
{
	int len;

	len = strlen(name);
	return ((strncasecmp(line, name, len) == 0) &&
		(line[len] == ' ' || line[len] == '\t'));
}

static unsigned
FNSP_create_nis_orgcontext(int flags)
{
	char *nis_master, *domain;
	int yperr;

	// Get local domain name
	yp_get_default_domain(&domain);

	// Check if FNS is installed
	if ((yperr = yp_master(domain, FNSP_org_map, &nis_master))
	    == 0) {
		free(nis_master);
		return (FN_SUCCESS);
	} else
		if (yperr != YPERR_MAP)
			return (FN_E_CONFIGURATION_ERROR);

	// check for *root* permissions
	if (geteuid() != 0)
		return (FN_E_CTX_NO_PERMISSION);

	// check if this machine is the master server
	// for passwd map (or) hosts map
	if (!flags) {
		struct utsname mach_info;
		uname(&mach_info);
		if (yp_master(domain, FNSP_get_user_source_table(),
		    &nis_master) != 0)
			return (FN_E_CONFIGURATION_ERROR);
		if (strcmp(nis_master, mach_info.nodename) != 0) {
			free(nis_master);
			if (yp_master(domain, FNSP_get_host_source_table(),
			    &nis_master) != 0)
				return (FN_E_CONFIGURATION_ERROR);
			if (strcmp(nis_master, mach_info.nodename) != 0) {
				free(nis_master);
				return (FN_E_UNSPECIFIED_ERROR);
			}
		}
		free(nis_master);
	}

	// create directory /etc/fn
	FILE *rf, *wf;
	int found = 0;
	int first_time = 1;
	char mapfile[FNS_NIS_INDEX];
	char tmpfile[FNS_NIS_INDEX];
	char line[FNS_NIS_SIZE];
	struct stat buffer;
	char fns_dir[FNS_NIS_INDEX];
	sprintf(fns_dir, "%s", FNSP_nis_map_dir);
	if (stat(fns_dir, &buffer) == 0) {
		if ((buffer.st_mode & S_IFMT) != S_IFDIR) {
			fprintf(stderr, "%s %s\n", fns_dir,
			    gettext("is not a directory"));
			return (FN_E_CONFIGURATION_ERROR);
		}
	} else if (mkdir(fns_dir,
	    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
		fprintf(stderr, gettext("Unable to create %s directory\n"),
		    fns_dir);
		return (FN_E_CONFIGURATION_ERROR);
	}

	// create directory /etc/fn/'domainname'
	sprintf(fns_dir, "%s/%s", FNSP_nis_map_dir, domain);
	if (stat(fns_dir, &buffer) == 0) {
		if ((buffer.st_mode & S_IFMT) != S_IFDIR) {
			fprintf(stderr,
			    gettext("%s is not a directory\n"), fns_dir);
			return (FN_E_CONFIGURATION_ERROR);
		}
	} else if (mkdir(fns_dir,
	    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
		fprintf(stderr,
		    gettext("Unable to create %s directory\n"), fns_dir);
		return (FN_E_CONFIGURATION_ERROR);
	}

	// create directory /var/yp/'domainname' if it did not exist
	sprintf(fns_dir, "/var/yp/%s", domain);
	if (stat(fns_dir, &buffer) == 0) {
		if ((buffer.st_mode & S_IFMT) != S_IFDIR) {
			fprintf(stderr,
			    gettext("%s is not a directory\n"), fns_dir);
			return (FN_E_CONFIGURATION_ERROR);
		}
	} else if (mkdir(fns_dir,
	    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
		fprintf(stderr,
		    gettext("Unable to create %s directory\n"), fns_dir);
		return (FN_E_CONFIGURATION_ERROR);
	}

	// Create a lock file
	int lock_file;
	sprintf(mapfile, "%s/%s/%s", FNSP_nis_map_dir, domain, FNSP_lock_file);
	// Check if the lock file exists
	if (stat(mapfile, &buffer) == 0)
		return (FN_SUCCESS);
	if ((lock_file = open(mapfile, O_WRONLY | O_CREAT, 0444))
	    != -1) {
		if (lockf(lock_file, F_LOCK, 0L) != -1) {
			write(lock_file, "FNS lock file",
			    strlen("FNS lock file"));
			lockf(lock_file, F_ULOCK, 0L);
			close(lock_file);
		} else {
			fprintf(stderr,
			    gettext("Unable to lock FNS lock file %s\n"),
			    mapfile);
			return (FN_E_INSUFFICIENT_RESOURCES);
		}
	} else {
		fprintf(stderr,
		    gettext("Unable to create FNS lock file %s\n"),
		    mapfile);
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	// check if /etc/fn/'domainname'/Makefile exists
	sprintf(mapfile, "%s/%s/%s", FNSP_nis_map_dir, domain, MAKE_FILE);
	if (stat(mapfile, &buffer) == 0)
		return (FN_SUCCESS);

	// Update the /etc/fn/'domainname'/Makefile if it does not exist
	if ((wf = fopen(mapfile, "w")) == NULL)
		return (FN_E_INSUFFICIENT_RESOURCES);
	fprintf(wf, "#\n"
	    "# Copyright (c) 1995, by Sun Microsystems, Inc.\n"
	    "# All rights reserved.\n"
	    "#\n#\n#\n"
	    "DOM = `domainname`\n"
	    "NOPUSH = \"\"\n"
	    "YPDIR=/usr/lib/netsvc/yp\n"
	    "SBINDIR=/usr/sbin\n"
	    "YPDBDIR=/var/yp\n"
	    "YPPUSH=$(YPDIR)/yppush\n"
	    "MAKEDBM=$(SBINDIR)/makedbm\n"
	    "MULTI=$(YPDIR)/multi\n"
	    "REVNETGROUP=$(SBINDIR)/revnetgroup\n"
	    "STDETHERS=$(YPDIR)/stdethers\n"
	    "STDHOSTS=$(YPDIR)/stdhosts\n"
	    "MKNETID=$(SBINDIR)/mknetid\n"
	    "MKALIAS=$(YPDIR)/mkalias\n\n"
	    "CHKPIPE=  || (  echo \"NIS make terminated:\" $@"
	    "1>&2; kill -TERM 0 )\n\n"
	    "k:\n"
	    "\t@if [ ! $(NOPUSH) ]; then $(MAKE)  "
	    "$(MFLAGS) -k all; \\\n"
	    "\telse $(MAKE) $(MFLAGS) -k all NOPUSH=$(NOPUSH); fi\n\n"
	    "all: \n");
	fclose(wf);

	// Update /var/yp/makefile
	// Check if directory /var/yp exists, if not create
	if (stat(FNSP_nis_dir, &buffer) == 0) {
		if ((buffer.st_mode  & S_IFMT) != S_IFDIR) {
			fprintf(stderr, gettext("%s is not a directory\n"),
			    FNSP_nis_dir);
			return (FN_E_CONFIGURATION_ERROR);
		}
	} else if (mkdir(FNSP_nis_dir,
	    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
		fprintf(stderr,
		    gettext("Unable to create %s directory"),
		    FNSP_nis_dir);
		return (FN_E_CONFIGURATION_ERROR);
	}

	// Check if Makefile exists in /var/yp/Makefile
	sprintf(mapfile, "%s/%s", FNSP_nis_dir, MAKE_FILE);
	sprintf(tmpfile, "%s.%s", mapfile, "tmp");
	if ((wf = fopen(tmpfile, "w")) == NULL)
		return (FN_E_INSUFFICIENT_RESOURCES);
	if ((rf = fopen(mapfile, "r")) == NULL) {
		// Construct the default Makefile
		fprintf(wf, "#\n"
		    "# Copyright (c) 1995, by "
		    "Sun Microsystems, Inc.\n"
		    "# All rights reserved.\n"
		    "#\n#\n#\n\nall: fn\n");
	} else {
		while (fgets(line, sizeof (line), rf)) {
			if ((found == 0) &&
			    (FNSP_match_map_index(line, "all:"))) {
				if (!FNSP_match_map_index(line, "all: fn"))
					fprintf(wf, "all: fn%s",
					    &line[strlen("all:")]);
				else {
					fputs(line, wf);
					first_time = 0;
				}
				found = 1;
			} else
				fputs(line, wf);
		}
	}

	// Update /var/yp/Makefile
	if (first_time) {
		fprintf(wf, "\nFNS=%s/%s\n"
		    "fn.time : $(FNS)\n"
		    "\t-@if [ -f $(FNS)/%s ]; then \\\n"
		    "\t\techo \"updating FNS\"; \\\n"
		    "\t\tcd $(FNS); make; \\\n\telse \\\n"
		    "\t\techo \"couldn't find $(FNS)\"; \\\n"
		    "\tfi\n\nfn: fn.time\n",
		    FNSP_nis_map_dir, domain, MAKE_FILE);
	}
	if (rf != NULL)
		fclose(rf);
	fclose(wf);
	if (rename(tmpfile, mapfile) < 0)
		return (FN_E_INSUFFICIENT_RESOURCES);

	return (FN_SUCCESS);
}

// ---------------------------------------------------
// Function to update makefile in /var/yp and
// create makefile for FNS in /var/fn/'domainname'
// if it does not exist.
// ----------------------------------------------------
static char *FNSP_files_map_dir = "/var/fn";

unsigned
FNSP_create_files_orgcontext()
{
	// check for *root* permissions
	if (geteuid() != 0)
		return (FN_E_CTX_NO_PERMISSION);

	// create directory /var/fn
	char fns_dir[FNS_NIS_INDEX];
	char mapfile[FNS_NIS_INDEX];
	struct stat buffer;
	sprintf(fns_dir, "%s", FNSP_files_map_dir);
	if (stat(fns_dir, &buffer) == 0) {
		if ((buffer.st_mode & S_IFMT) != S_IFDIR) {
			fprintf(stderr,
			    gettext("%s is not a directory\n"), fns_dir);
			return (FN_E_CONFIGURATION_ERROR);
		}
	} else if (mkdir(fns_dir,
	    S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
		fprintf(stderr,
		    gettext("Unable to create %s directory\n"), fns_dir);
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	// Check and create a lock file
	sprintf(mapfile, "%s/%s", FNSP_files_map_dir, FNSP_lock_file);
	if (stat(mapfile, &buffer) == 0)
		return (FN_SUCCESS);
	int lock_file;
	if ((lock_file = open(mapfile, O_WRONLY | O_CREAT, 0666))
	    != -1) {
		if (lockf(lock_file, F_LOCK, 0L) != -1) {
			write(lock_file, "FNS lock file",
			    strlen("FNS lock file"));
			lockf(lock_file, F_ULOCK, 0L);
			// Change permissions
			fchmod(lock_file, 0666);
			close(lock_file);
		} else {
			fprintf(stderr,
			    gettext("Unable to lock FNS lock file %s\n"),
			    mapfile);
			return (FN_E_INSUFFICIENT_RESOURCES);
		}
	} else {
		fprintf(stderr, gettext("Unable to create FNS lock file %s\n"),
		    mapfile);
		return (FN_E_INSUFFICIENT_RESOURCES);
	}

	return (FN_SUCCESS);
}

// Routine to change permissions for FNS files to their
// respective users

static void
FNSP_change_user_ownership(char *username)
{
	char mapfile[FNS_NIS_INDEX];

	// Open the passwd file
	FILE *passwdfile;
	if ((passwdfile = fopen(FNSP_get_user_source_table(), "r")) == NULL) {
		fprintf(stderr,
		    gettext("%s: could not open file %s for read\n"
		    "Permissions for user context %s not changed\n"),
		    program_name, FNSP_get_user_source_table(),
		    username);
		return;
	}

	// Seach for the user entry
	char buffer[MAX_CANON+1];
	struct passwd *owner_passwd, buf_passwd;
	while ((owner_passwd = fgetpwent_r(passwdfile, &buf_passwd,
	    buffer, MAX_CANON)) != 0) {
		if (strcmp(owner_passwd->pw_name, username) == 0)
			break;
	}
	fclose(passwdfile);

	// Check if the user entry exists in the passwd table
	if (owner_passwd == NULL) {
		fprintf(stderr,
		    gettext("Unable to get user ID of %s\n"), username);
		fprintf(stderr, gettext("Permissions for the user context "
		    "%s not changed\n"), username);
		fprintf(stderr, gettext("Use chown to change the ownership "
		    "of files:\n\t/var/fn/fns_user_%s.ctx.pag\n\t"
		    "/var/fn/fns_user_%s.ctx.dir\n"), username, username);
		return;
	}

	// The ownership of the files are chaned in library
	return;
}

#include <ndbm.h>
#define	FNSP_USER_ATTR_MAP	"/var/fn/fns_user.attr"
static void
FNSP_create_and_change_ownership_of_attr_map()
{
	struct stat attr_file;
	char filename[FNS_NIS_INDEX];
	strcpy(filename, FNSP_USER_ATTR_MAP);
	strcat(filename, ".dir");
	int present = stat(filename, &attr_file);
	if (present == 0)
		return;

	// Create the map and change the permissions
	DBM *db = db = dbm_open(FNSP_USER_ATTR_MAP, O_CREAT, 0666);
	dbm_close(db);

	chmod(filename, 0666);
	strcpy(filename, FNSP_USER_ATTR_MAP);
	strcat(filename, ".pag");
	chmod(filename, 0666);
}


// Basic creation routine for creating context of appropriate type

FN_ref *
fnscreate(FN_ctx *ctx,
	const FN_composite_name &fullname, // name relative to initial ctx
	const FN_composite_name &name,	   // name relative to 'ctx'
	unsigned context_type,
	int &created,
	const FN_identifier *ref_type = 0)
{
	FN_status status;
	FN_ref *ref;
	FN_string *fstr = fullname.string();
	FN_attrset *attrs = generate_creation_attrs(context_type, ref_type);

	if (attrs == NULL) {
		delete fstr;
		fprintf(stderr, gettext("DEBUG fnscreate: could not "
		    "generate creation attributes\n"));
		return (NULL);
	}

	ref = ctx->attr_create_subcontext(name, attrs, status);
	delete attrs;

	if (status.is_success()) {
		++created;
		if (verbose)
			printf("%s %s\n", (fstr ? (char *)fstr->str() : ""),
			    gettext("created"));
	} else if (status.code() == FN_E_NAME_IN_USE) {
		fprintf(stderr, "%s '%s' %s.\n",
			gettext("Binding for"),
			(fstr ? (char *)fstr->str() : ""),
			gettext("already exists"));
		const FN_ref *rref = status.resolved_ref();
		const FN_composite_name *rname = status.remaining_name();
		FN_status s1;
		if (rref && rname) {
			FN_ctx *rctx = FN_ctx::from_ref(*rref,
			    AUTHORITATIVE, s1);
			if (rctx) {
				ref = rctx->lookup(*rname, s1);
				delete rctx;
			} else {
				FN_string * desc = s1.description();
				fprintf(stderr,	"%s: %s\n",
gettext("DEBUG fnscreate: Could not generate context from resolved reference"),
					desc? (char *)(desc->str()) :
					gettext("No status description"));
				delete desc;
				ref = ctx->lookup(name, s1);
			}
		} else {
			fprintf(stderr, "%s\n",
gettext("DEBUG fnscreate: Could not use resolved reference."));
			ref = ctx->lookup(name, s1);
		}
	} else {
	    FN_string *desc = status.description();
	    fprintf(stderr, "%s '%s' %s: %s\n",
		    gettext("Create of"),
		    (fstr ? (char *)fstr->str() : ""),
		    gettext("failed"),
		    (desc? (char *)(desc->str()):
		    gettext("No status description")));
	    delete desc;
	}
	delete fstr;
	return (ref);
}

int
fnsbind(FN_ctx *ctx,
	const FN_composite_name &alias_fullname,
	const FN_composite_name &alias_name,
	const FN_composite_name &orig_fullname,
	const FN_ref &ref)
{
	FN_status status;

	if (ctx->bind(alias_name, ref, global_bind_flags, status) ||
	    status.code() == FN_E_NAME_IN_USE) {
		if (status.code() == FN_E_NAME_IN_USE) {
			FN_string *pstr = alias_fullname.string();
			fprintf(stderr,
			    "%s '%s' %s.\n",
				gettext("Binding for"),
				(pstr ? (char *)pstr->str() : ""),
				gettext("already exists"));
			delete pstr;
		} else if (verbose) {
			FN_string *astr = alias_fullname.string();
			FN_string *ostr = orig_fullname.string();
			printf("'%s' %s '%s'\n",
			    (astr ? (char *)astr->str() : ""),
			    gettext("bound to context reference of"),
			    (ostr ? (char *)ostr->str() : ""));
			delete astr;
			delete ostr;
		}
		return (1);
	} else {
		FN_string *desc = status.description();
		FN_string *astr = alias_fullname.string();
		FN_string *ostr = orig_fullname.string();
		fprintf(stderr, "%s: '%s' %s '%s': %s\n",
			program_name,
			(astr ? (char *)astr->str() : ""),
			gettext("could not be bound to context reference of"),
			(ostr ? (char *)ostr->str() : ""),
			(desc ? (char *)(desc->str()) :
			    (gettext("No status description"))));
		delete desc;
		delete astr;
		delete ostr;
		return (0);
	}
}

// Make sure that NIS_GROUP has been set.
static int
check_nis_group()
{
	// Check only if the name service is NIS+
	if (name_service != FNSP_nisplus_ns)
		return (1);

	if ((getenv("NIS_GROUP") == 0) &&
	    (context_type == FNSP_organization_context)) {
		fprintf(stderr, "%s",
gettext("The environment variable NIS_GROUP has not been set.  This has\n\
administrative implications for contexts that will be created.\n\
See fncreate(1M) and nis+(1). Please try again after setting NIS_GROUP.\n"));
		return (0);
	} else
		return (1);
}

// make sure that name has two components, last of which is null
// returns 1 if condition is true, 0 otherwise
static int
check_null_trailer(const FN_composite_name &fullname,
    const FN_composite_name &name)
{
	void *iter_pos;
	if (name.count() == 2 && name.last(iter_pos)->is_empty())
		return (1);
	else {
		FN_string *fstr = fullname.string();
		fprintf(stderr, "%s: %s '%s'.\n%s.\n",
			program_name, gettext("Cannot create context for"),
			(fstr ? (char *)fstr->str(): ""),
	gettext("the supplied name should have a trailing slash '/'"));
		delete fstr;
		return (0);
	}
}

static const
    FN_composite_name canonical_service_name((unsigned char *)"_service/");
static const
    FN_composite_name canonical_username_name((unsigned char *)"_user/");
static const
    FN_composite_name canonical_hostname_name((unsigned char *)"_host/");
static const FN_composite_name canonical_site_name((unsigned char *)"_site/");
static const FN_composite_name canonical_fs_name((unsigned char *)"_fs/");
static const FN_composite_name custom_service_name((unsigned char *)"service/");
static const FN_composite_name custom_username_name((unsigned char *)"user/");
static const FN_composite_name custom_hostname_name((unsigned char *)"host/");
static const FN_composite_name custom_site_name((unsigned char *)"site/");
static const FN_composite_name custom_fs_name((unsigned char *)"fs/");

static int
name_is_special_token(const FN_composite_name &shortname,
    unsigned int ctx_type)
{
	void *iter_pos;
	const FN_string *atomic_name = shortname.first(iter_pos);

	if (atomic_name == 0)
		return (0);

	switch (ctx_type) {
	case FNSP_service_context:
		return (atomic_name->compare(FNSP_service_string,
		    FN_STRING_CASE_INSENSITIVE) == 0 ||
		    atomic_name->compare(FNSP_service_string_sc,
		    FN_STRING_CASE_INSENSITIVE) == 0);
	case FNSP_username_context:
		return (atomic_name->compare(FNSP_user_string,
		    FN_STRING_CASE_INSENSITIVE) == 0 ||
		    atomic_name->compare(FNSP_user_string_sc,
		    FN_STRING_CASE_INSENSITIVE) == 0);
	case FNSP_hostname_context:
		return (atomic_name->compare(FNSP_host_string,
		    FN_STRING_CASE_INSENSITIVE) == 0 ||
		    atomic_name->compare(FNSP_host_string_sc,
		    FN_STRING_CASE_INSENSITIVE) == 0);
	case FNSP_site_context:
		return (atomic_name->compare(FNSP_site_string,
		    FN_STRING_CASE_INSENSITIVE) == 0 ||
		    atomic_name->compare(FNSP_site_string_sc,
		    FN_STRING_CASE_INSENSITIVE) == 0);
	case FNSP_fs_context:
		return (atomic_name->compare(FNSP_fs_string,
		    FN_STRING_CASE_INSENSITIVE) == 0 ||
		    atomic_name->compare(FNSP_fs_string_sc,
		    FN_STRING_CASE_INSENSITIVE) == 0);
	}
	return (0);
}

static int
assign_aliases(
	const FN_composite_name &parent_name,
	unsigned int ctx_type,
	FN_composite_name **fn,
	const FN_composite_name **rn,
	FN_composite_name **afn,
	const FN_composite_name **arn)
{
	// get short forms
	switch (ctx_type) {
	case FNSP_service_context:
		*rn = &custom_service_name;
		*arn = &canonical_service_name;
		break;
	case FNSP_site_context:
		*rn = &custom_site_name;
		*arn = &canonical_site_name;
		break;
	case FNSP_username_context:
		*rn = &custom_username_name;
		*arn = &canonical_username_name;
		break;
	case FNSP_hostname_context:
		*rn = &custom_hostname_name;
		*arn = &canonical_hostname_name;
		break;
	}

	// construct long forms using parent name

	void *iter_pos;
	// parent has trailing FNSP_empty_component;
	parent_name.last(iter_pos);
	FN_composite_name *new_fn = parent_name.prefix(iter_pos);
	FN_composite_name *new_afn = parent_name.prefix(iter_pos);

	if (new_fn == 0 || new_afn == 0)
		return (0);

	// %%% minimal error checking here
	new_fn->append_name(**rn);
	new_afn->append_name(**arn);
	*fn = new_fn;
	*afn = new_afn;
	return (1);
}


// Create a 'nsid' context and return its reference.
//
// This is a flat context in which naming system names (with associated
// nns pointers) could be bound.  Examples of such names  are: "_service/",
// "_host/", "_user/".
//
// An "nsid" context should only be bound to a name with a trailing '/'.
// Should we make the effort to check for that here?

FN_ref *
create_nsid_context(FN_ctx *ctx,
		    const FN_composite_name &fullname,
		    const FN_composite_name &name,
		    int &created,
		    unsigned context_type = FNSP_nsid_context)
{
	return (fnscreate(ctx, fullname, name, context_type, created));
}


// Create a service context in which slash-separated, left-to-right names
// could be bound and return its reference.
FN_ref *
create_generic_context(
	FN_ctx *ctx,
	const FN_composite_name &fullname,
	const FN_composite_name &name,
	int &created,
	const char *ref_type_str)
{
	FN_identifier *ref_type = 0;
	FN_ref * answer;

	if (ref_type_str)
		ref_type = new FN_identifier(
		    (const unsigned char *)ref_type_str);

	answer = fnscreate(ctx, fullname, name, FNSP_generic_context,
			    created, ref_type);
	delete ref_type;
	return (answer);
}

// Create a service context in which slash-separated, left-to-right names
// could be bound and return its reference.
FN_ref *
create_service_context(
	FN_ctx *ctx,
	const FN_composite_name &fullname,
	const FN_composite_name &name,
	int &created)
{
	return (fnscreate(ctx, fullname, name, FNSP_service_context, created));
}

// Create an nsid context with a service subcontext within it and
// return the reference of the nsid context.
// If 'service_ref_holder' is non-zero, set it to reference of
// service context created.

FN_ref *
create_nsid_and_service_context(FN_ctx *ctx,
				const FN_composite_name &fullname,
				const FN_composite_name &name,
				int &how_many,
				unsigned int context_type = FNSP_nsid_context,
				FN_ref **service_ref_holder = 0,
				FN_ctx **nsid_ctx_holder = 0)
{
	// name should have a trailing '/'

	if (nsid_ctx_holder)
		*nsid_ctx_holder = 0;  // initialize

	// Create nsid context associated.
	FN_ref *nsid_ref = fnscreate(ctx, fullname, name, context_type,
	    how_many);

	if (nsid_ref == 0)
		return (0);

	// Create service subcontext.
	FN_status status;
	FN_ctx *nsid_ctx = FN_ctx::from_ref(*nsid_ref,
		AUTHORITATIVE, status);
	if (nsid_ctx == 0) {
		FN_string *desc = status.description();
		fprintf(stderr, "%s: %s: %s\n",
		    program_name,
		    gettext("Could not generate context from nsid reference"),
		    desc ? (char *)(desc->str()) :
			gettext("No status description"));
		delete desc;
		delete nsid_ref;
		return (0);
	}

	void *iter_pos;
	FN_composite_name service_fullname(fullname);

	if (service_fullname.last(iter_pos)->is_empty())
		service_fullname.insert_comp(iter_pos, FNSP_service_string);
	else
		service_fullname.append_name(custom_service_name);

	FN_ref *service_ref =
	    create_service_context(nsid_ctx, service_fullname,
				    custom_service_name, how_many);
	if (service_ref) {
		// add alias
		FN_composite_name alias_fullname(fullname);

		if (alias_fullname.last(iter_pos)->is_empty())
			alias_fullname.insert_comp(iter_pos,
			    FNSP_service_string_sc);
		else
			alias_fullname.append_name(canonical_service_name);

		fnsbind(nsid_ctx, alias_fullname, canonical_service_name,
			service_fullname, *service_ref);

		if (service_ref_holder)
			*service_ref_holder = service_ref;
		else
			delete service_ref;
	}
	if (nsid_ctx_holder)
		*nsid_ctx_holder = nsid_ctx;
	else
		delete nsid_ctx;

	return (nsid_ref);
}

static FN_identifier
FNSP_fs_reftype((const unsigned char *)"onc_fn_fs");
static FN_identifier
FNSP_nisplus_user_fs_addrtype((const unsigned char *)"onc_fn_fs_user_nisplus");
static FN_identifier
FNSP_user_fs_addrtype((const unsigned char *)"onc_fn_fs_user");
static FN_identifier
FNSP_host_fs_addrtype((const unsigned char *)"onc_fn_fs_host");

/* returns 1 for success; 0 for failure */
static int
fnsbind_ref(FN_ctx *ctx, const FN_composite_name &parent_fullname,
	    const FN_composite_name &target_name, const FN_ref &ref)
{
	FN_status status;
	FN_string *pstr;
	FN_string *nstr;
	FN_string *desc;
	if (ctx->bind(target_name, ref, global_bind_flags, status) ||
	    status.code() == FN_E_NAME_IN_USE) {
		if (status.code() == FN_E_NAME_IN_USE) {
			pstr = parent_fullname.string();
			nstr = target_name.string();
			fprintf(stderr,	"%s '%s' %s '%s' %s.\n",
				gettext("Binding for"),
				(nstr ? (char *)nstr->str() : ""),
				gettext("in"),
				(pstr ? (char *)pstr->str() : ""),
				gettext("context already exists"));
			delete pstr;
			delete nstr;
		} else if (verbose) {
			pstr = parent_fullname.string();
			nstr = target_name.string();
			printf("%s '%s' %s '%s'.\n",
			    gettext("Created binding for"),
			    (nstr ? (char *)nstr->str() : ""),
			    gettext("in context"),
			    (pstr ? (char *)pstr->str() : ""));
			delete nstr;
			delete pstr;
		}
		return (1);
	} else {
		// could not create canonical binding
		pstr = parent_fullname.string();
		nstr = target_name.string();
		desc = status.description();
		fprintf(stderr, "%s: %s '%s' %s '%s': %s\n",
			program_name,
			gettext("could not create binding for"),
			(nstr ? (char *)nstr->str() : ""),
			gettext("in context"),
			(pstr ? (char *)pstr->str() : ""),
			(desc ? (char *)desc->str() :
			    gettext("No status description")));
		delete pstr;
		delete nstr;
		delete desc;
		return (0);
	}
}

// create and return reference for user fs binding
static FN_ref *
create_user_fs_ref(const FN_string &user_name, const FN_string &domain_name)
{
	FN_ref *ref = new FN_ref(FNSP_fs_reftype);
	char fs_addr[NIS_MAXNAMELEN];
	char encoded_addr[NIS_MAXNAMELEN];
	size_t len;

	if (ref == 0)
		return (0);

	sprintf(fs_addr, "%s", user_name.str());

	len = NIS_MAXNAMELEN;
	if (__fns_xdr_encode_string(fs_addr, encoded_addr, len) == 0) {
		delete ref;
		return (0);
	}
	FN_ref_addr addr(FNSP_user_fs_addrtype, len, encoded_addr);
	ref->append_addr(addr);

	// NIS+ refs get a second address of type FNSP_nisplus_user_fs_addrtype

	if (name_service == FNSP_nisplus_ns) {
		sprintf(fs_addr, "[name=%s]passwd.org_dir.%s",
			user_name.str(), domain_name.str());

		len = NIS_MAXNAMELEN;
		if (__fns_xdr_encode_string(fs_addr, encoded_addr, len) == 0) {
			delete ref;
			return (0);
		}
		FN_ref_addr
		    addr(FNSP_nisplus_user_fs_addrtype, len, encoded_addr);
		ref->append_addr(addr);
	}

	return (ref);
}

static int
create_user_fs(FN_ctx *user_ctx,
    const FN_composite_name &user_fullname,
    const FN_string &user_name,
    const FN_string &domain_name,
    const FN_composite_name *target_name = 0)
{
	FN_ref *ref = create_user_fs_ref(user_name, domain_name);
	int status = 0;

	if (target_name)
		status = fnsbind_ref(user_ctx, user_fullname, *target_name,
		    *ref);
	else {
		status = (fnsbind_ref(user_ctx, user_fullname,
		    canonical_fs_name, *ref) &&
		    fnsbind_ref(user_ctx, user_fullname, custom_fs_name, *ref));
	}
	delete ref;
	return (status);
}

// create and return reference for host fs binding
static FN_ref *
create_host_fs_ref(const FN_string &host_name, const FN_string &domain_name)
{
	FN_ref *ref = new FN_ref(FNSP_fs_reftype);
	char fs_addr[NIS_MAXNAMELEN];
	char encoded_addr[NIS_MAXNAMELEN];
	size_t len = NIS_MAXNAMELEN;

	if (ref == 0)
		return (0);

	switch (name_service) {
	case FNSP_files_ns:
	case FNSP_nis_ns:
		sprintf(fs_addr, "%s", host_name.str());
		break;
	case FNSP_nisplus_ns:
	default:
		sprintf(fs_addr, "%s.%s", host_name.str(),
		    domain_name.str());
		break;
	}
	if (__fns_xdr_encode_string(fs_addr, encoded_addr, len) == 0) {
		delete ref;
		return (0);
	}

	FN_ref_addr addr(FNSP_host_fs_addrtype, len, encoded_addr);
	ref->append_addr(addr);

	return (ref);
}

/* returns 1 for success; 0 for failure */
static int
create_host_fs(FN_ctx *host_ctx,
    const FN_composite_name &host_fullname,
    const FN_string &host_name,
    const FN_string &domain_name,
    const FN_composite_name *target_name = 0)
{
	FN_ref *ref = create_host_fs_ref(host_name, domain_name);
	int status = 0;

	if (target_name)
		status = fnsbind_ref(host_ctx, host_fullname, *target_name,
		    *ref);
	else {
		status = (fnsbind_ref(host_ctx, host_fullname,
		    canonical_fs_name, *ref) &&
		    fnsbind_ref(host_ctx, host_fullname, custom_fs_name, *ref));
	}
	delete ref;
	return (status);
}


// Create a site context in which dot-separated, right-to-left site names
// could be bound.
// If 'subcontext_p' is set, a nsid context for the site, and a service
// context in the nsid context are created.
FN_ref *
create_site_context(FN_ctx *ctx,
		    const FN_composite_name &fullname,
		    const FN_composite_name &name,
		    int subcontext_p,
		    int &how_many)
{
	FN_ref *site_ref = fnscreate(ctx,
	    fullname, name, FNSP_site_context, how_many);

	if (site_ref == 0 || subcontext_p == 0)
		return (site_ref);

	FN_status status;
	FN_ctx *site_ctx = FN_ctx::from_ref(*site_ref,
	    AUTHORITATIVE, status);
	if (site_ctx == 0) {
		FN_string *desc = status.description();
		fprintf(stderr, "%s: %s: %s\n",
			program_name,
		gettext("Could not generate context from site reference"),
			desc ? (char *)(desc->str()) :
			gettext("No status description"));
		delete desc;
		delete site_ref;
		return (0);
	}
	delete site_ctx;

	// %%% why are we not using site_ctx to create

	FN_composite_name site_nsid_name(name);
	site_nsid_name.append_comp(FNSP_empty_component);  // tag on "/"

	FN_composite_name site_full_nsid_name(fullname); // tag on "/"
	site_full_nsid_name.append_comp(FNSP_empty_component);

	FN_ref *nsid_ref =
		create_nsid_and_service_context(ctx,
						site_full_nsid_name,
						site_nsid_name,
						how_many);
	if (nsid_ref)
		delete nsid_ref;

	return (site_ref);
}

// Create nsid context associated with host, and create a 'service'
// context in its nsid context and create 'fs' bindings
FN_ref *
create_host_context(FN_ctx *ctx,
		    const FN_ref & /* parent_ref */,
		    const FN_composite_name &fullname,
		    const FN_composite_name &name,
		    const FN_string &domain_name,
		    const FN_string &host_name,
		    int subcontexts_p,
		    int &how_many)
{
	// name should have a trailing '/'

	FN_ref *host_ref;
	FN_ctx *host_ctx = 0;
	int how_many_before = how_many;  // remember count before attempt

	if (subcontexts_p) {
		host_ref = create_nsid_and_service_context(ctx, fullname, name,
		    how_many, FNSP_host_context, 0, &host_ctx);
		if (host_ctx && create_host_fs(host_ctx, fullname, host_name,
		    domain_name))
			how_many += 2;
		delete host_ctx;
	} else
		host_ref = create_nsid_context(ctx, fullname, name, how_many,
		    FNSP_host_context);

	return (host_ref);
}

// create 'host' contexts for the canonical host name
// (can_nsid_name and alias_nsid_name are host names with trailing '/')
// in parent context 'ctx' with name 'parent_name'
FN_ref *
create_host_context_can(FN_ctx *ctx,
			const FN_ref &parent_ref,
			const FN_composite_name &parent_name,
			const FN_string &can_hostname,
			const FN_string &alias_hostname,
			const FN_string &domain_name,
			int subcontext_p,
			int &how_many)
{
	FN_ref *ref = 0;
	FN_status status;
	void *iter_pos;

	// Construct relative and full names of "<hostname>/" for canonical name
	FN_composite_name can_hostname_nsid(can_hostname);
	can_hostname_nsid.append_comp(FNSP_empty_component);

	FN_composite_name can_fullname(parent_name);
	(void) can_fullname.last(iter_pos);
	can_fullname.insert_comp(iter_pos, can_hostname);

	// Construct relative and full names of "<hostname>/" for alias name
	FN_composite_name alias_hostname_nsid(alias_hostname);
	alias_hostname_nsid.append_comp(FNSP_empty_component);

	FN_composite_name alias_fullname(parent_name);
	(void) alias_fullname.last(iter_pos);
	alias_fullname.insert_comp(iter_pos, alias_hostname);

	if (can_hostname.compare(alias_hostname,
	    FN_STRING_CASE_INSENSITIVE) == 0) {
		// canonical name is same as alias name
		ref = create_host_context(ctx, parent_ref,
		    can_fullname, can_hostname_nsid,
		    domain_name, can_hostname, subcontext_p, how_many);
	} else {
		//
		// canonical name is different from alias name
		// do lookup first, since canonical name is likely to already
		// have context (assumes canonical entry is first in list)
		//
		ref = ctx->lookup(can_hostname_nsid, status);
		if (status.is_success() == 0) {
			// canonical name context does not exist
			ref = create_host_context(ctx, parent_ref,
			    can_fullname, can_hostname_nsid,
			    domain_name, can_hostname, subcontext_p, how_many);
			if (ref == 0) {
				FN_string *desc = status.description();
				FN_string *cstr = can_fullname.string();
				fprintf(stderr,	"%s: %s '%s': %s\n",
					program_name,
					(cstr? (char *)cstr->str() : ""),
					gettext("Could not create context for"),
					(desc? (char *)(desc->str()):
					    gettext("No status description")));
				delete desc;
				delete cstr;
				return (0);
			}
		}

		// make binding for alias name (to point to ref of
		// canonical name)
		if (status.is_success()) {
			fnsbind(ctx, alias_fullname, alias_hostname_nsid,
			    can_fullname, *ref);
		}
	}
	return (ref);
}


FN_ref *
create_user_context(
	FN_ctx *ctx,
	const FN_ref & /* parent_ref */,
	const FN_composite_name &fullname,
	const FN_composite_name &name,
	const FN_string &domain_name,
	const FN_string &user_name,
	int subcontexts_p,
	int &how_many)
{
	// name should have a trailing '/'

	FN_ref *user_ref;
	FN_ctx *user_ctx = 0;

	int how_many_before = how_many;  // remember count before attempt

	if (subcontexts_p) {
		user_ref = create_nsid_and_service_context(ctx, fullname,
		    name, how_many, FNSP_user_context, 0, &user_ctx);
		if (user_ctx && create_user_fs(user_ctx, fullname, user_name,
		    domain_name))
			how_many += 2;
		delete user_ctx;
	} else
		user_ref = create_nsid_context(ctx, fullname, name, how_many,
		    FNSP_user_context);

	// If NIS is the naming service return
	if (name_service != FNSP_files_ns)
		return (user_ref);

	// if created new contexts, change their ownership to be owned by user
	if (user_ref && (how_many_before < how_many))
		FNSP_change_user_ownership((char *) user_name.str());

	return (user_ref);
}

FN_ref *
create_username_context(FN_ctx *ctx,
			const FN_composite_name &fullname,
			const FN_composite_name &name,
			int subcontexts_p,
			int &how_many)
{
	// A "username" context should only be bound to a name of "user/".
	// Should we make the effort to check for that here?

	FN_ref *username_ref = fnscreate(ctx, fullname, name,
	    FNSP_username_context, how_many);

	// If files ns create and changes permissions on *attr* files
	if (name_service == FNSP_files_ns)
		FNSP_create_and_change_ownership_of_attr_map();

	if (subcontexts_p == 0 || username_ref == 0)
		return (username_ref);

	// Get username context
	FN_status stat;
	FN_ctx *username_ctx = FN_ctx::from_ref(*username_ref,
	    AUTHORITATIVE, stat);
	if (username_ctx == 0) {
		FN_string *desc = stat.description();
		fprintf(stderr,	"%s: %s: %s\n", program_name,
		gettext("Could not generate context from username reference"),
		    (desc ? (char *)(desc->str()):
		    gettext("No status description")));
		delete desc;
		fprintf(stderr, "%s: %s.\n", program_name,
			gettext("No user contexts were created"));
		return (username_ref);
	}

	// Get organization directory name of username context
	FN_string *org_dir;
	if (name_service != FNSP_files_ns) {
		unsigned status;
		FN_string *username_dir =
		    FNSP_reference_to_internal_name(*username_ref);
		if (username_dir == 0) {
			fprintf(stderr, "%s: %s.\n", program_name,
gettext("Could not obtain object name of username context"));
			return (username_ref);
		}
		if (name_service == FNSP_nisplus_ns)
			org_dir = FNSP_orgname_of(*username_dir, status);
		else
			org_dir = FNSP_nis_orgname_of(*username_dir, status);
		// use stat for printing purposes
		stat.set(status, 0, 0);
		delete username_dir;
		if (status != FN_SUCCESS) {
			FN_string *desc = stat.description();
			fprintf(stderr, "%s: %s: %s\n", program_name,
gettext("Could not obtain directory name of username context's organization"),
			    desc? ((char *)(desc->str())) :
			    gettext("No status description"));
			delete desc;
			return (username_ref);
		}
	} else
		org_dir = new FN_string(FNSP_empty_component);

	// For each user, create a user nsid context
	void *iter_pos;
	int users;
	if (fullname.last(iter_pos)->is_empty())
		// trailing slash supplied
		users = traverse_user_list(*org_dir,
		    username_ctx, *username_ref, fullname, 1);
	else {
		FN_composite_name new_fullname(fullname);
		new_fullname.append_comp(FNSP_empty_component);
		users = traverse_user_list(*org_dir, username_ctx,
		    *username_ref, new_fullname, 1);
	}

	delete org_dir;
	delete username_ctx;

	how_many += users;

	return (username_ref);
}

FN_ref *
create_hostname_context(FN_ctx *ctx,
			const FN_composite_name &fullname,
			const FN_composite_name &name,
			int subcontexts_p,
			int &how_many)
{
	// An "hostname" context should only be bound to a name of "host/".
	// Should we make the effort to check for that here?

	FN_ref *hostname_ref = fnscreate(ctx,
	    fullname,
	    name,
	    FNSP_hostname_context,
	    how_many);

	if (subcontexts_p == 0 || hostname_ref == 0)
		return (hostname_ref);

	// Get handle to hostname context
	FN_status stat;
	FN_ctx *hostname_ctx = FN_ctx::from_ref(*hostname_ref,
	    AUTHORITATIVE, stat);
	if (hostname_ctx == 0) {
		FN_string *desc = stat.description();
		fprintf(stderr,	"%s: %s: %s\n", program_name,
	gettext("Could not generate context from hostname reference"),
			(desc ? (char *)(desc->str()) :
			    gettext("No status description")));
		fprintf(stderr, "%s: %s.\n",
		    program_name, gettext("No host contexts were created"));
		delete desc;
		return (hostname_ref);
	}

	// Get organization directory name of hostname context
	FN_string *org_dir;
	if (name_service != FNSP_files_ns) {
		unsigned status;
		FN_string *hostname_dir =
		    FNSP_reference_to_internal_name(*hostname_ref);
		if (hostname_dir == 0) {
			fprintf(stderr, "%s: %s.\n", program_name,
gettext("Could not obtain object name of hostname context"));
			return (hostname_ref);
		}
		if (name_service == FNSP_nis_ns)
			org_dir = FNSP_nis_orgname_of(*hostname_dir, status);
		else
			org_dir = FNSP_orgname_of(*hostname_dir, status);
		// use stat for printing purposes
		stat.set(status, 0, 0);
		delete hostname_dir;
		if (status != FN_SUCCESS) {
			FN_string *desc = stat.description();
			fprintf(stderr, "%s: %s: %s\n", program_name,
gettext("Could not obtain directory name of hostname context's organization"),
			    desc ? (char *)(desc->str()) :
			    gettext("No status description"));
			delete desc;
			return (hostname_ref);
		}
	} else
		org_dir = new FN_string(FNSP_empty_component);

	// For each host, create a host nsid context
	int hosts;
	void *iter_pos;
	if (fullname.last(iter_pos)->is_empty())
		// trailing slash supplied
		hosts = traverse_host_list(*org_dir, hostname_ctx,
		    *hostname_ref, fullname, 1);
	else {
		FN_composite_name new_fullname(fullname);
		new_fullname.append_comp(FNSP_empty_component);
		hosts = traverse_host_list(*org_dir, hostname_ctx,
		    *hostname_ref, new_fullname, 1);
	}

	delete org_dir;
	delete hostname_ctx;

	how_many += hosts;
	return (hostname_ref);
}

// Create printer context in the service context
static int
create_printer_context(const FN_ref *service_ref,
    const FN_composite_name &fullname)
{
	FN_status status;
	FN_string *fstr;
	void *iter_pos;
	if (fullname.last(iter_pos)->is_empty())
		// trailing slash supplied
		fstr = fullname.string();
	else {
		FN_composite_name new_fullname(fullname);
		new_fullname.append_comp(FNSP_empty_component);
		fstr = new_fullname.string();
	}

	FN_ctx *service_ctx = FN_ctx::from_ref(*service_ref,
	    AUTHORITATIVE, status);
	if (service_ctx == 0) {
		FN_string *desc = status.description();
		fprintf(stderr, "%s: %s %s: %s\n", program_name,
		    (fstr ? (char *)fstr->str() : ""),
		    gettext("Could not generate context from service reference"),
		    desc ? (char *)(desc->str()) :
		    gettext("No status description"));
		delete desc;
		delete fstr;
		return (0);
	}

	FN_attrset *attrs = generate_creation_attrs(
	    FNSP_printername_context, FNSP_reftype_from_ctxtype(
	    FNSP_printername_context));
	if (attrs == NULL) {
		fprintf(stderr, "%s: %sservice/printer %s\n", program_name,
		    (fstr ? (char *)fstr->str() : ""),
		    gettext("Could not generate printer context"));
		delete service_ctx;
		delete fstr;
		return (0);
	}

	// Create the printer context
	FN_string printer_name_string((const unsigned char *) "printer");
	FN_ref *ptr_ref = service_ctx->attr_create_subcontext(
	    printer_name_string, attrs, status);
	delete attrs;
	delete service_ctx;
	if (status.is_success()) {
		if (verbose)
			printf("%sservice/printer %s\n",
			    (fstr ? (char *)fstr->str() : ""),
			    gettext("created"));
		delete ptr_ref;
	} else if (status.code() == FN_E_NAME_IN_USE) {
		fprintf(stderr, "%s '%sservice/printer' %s.\n",
		    gettext("Binding for"),
		    (fstr ? (char *)fstr->str() : ""),
		    gettext("already exists"));
	} else {
	    FN_string *desc = status.description();
	    fprintf(stderr, "%sservice/printer %s: %s\n",
		    (fstr ? (char *)fstr->str() : ""),
		    gettext("create of printer context failed"),
		    (desc? (char *)(desc->str()):
		    gettext("No status description")));
	    delete desc;
	    delete fstr;
	    return (0);
	}
	delete fstr;
	return (1);
}

// construct full and relative names for "_host/"
static int
create_org_host_context(const FN_ref *org_ref,
    const FN_composite_name &fullname, int subcontexts_p)
{
	FN_status status;
	int count = 0;
	FN_ctx *org_ctx = FN_ctx::from_ref(*org_ref,
	    AUTHORITATIVE, status);
	if (org_ctx == 0) {
		FN_string *desc = status.description();
		fprintf(stderr, "%s: %s: %s\n", program_name,
		gettext("Could not generate context from nsid reference"),
			desc ?(char *)(desc->str()):
			gettext("No status description"));
		delete desc;
		return (0);
	}

	void *iter_pos;
	FN_ref *ref;

	// construct full and relative names for "_host/"
	FN_composite_name full_hname(fullname);
	(void) full_hname.last(iter_pos);
	full_hname.insert_name(iter_pos, FNSP_host_string);
	if (ref = create_hostname_context(org_ctx,
	    full_hname, custom_hostname_name, subcontexts_p, count)) {
		FN_composite_name alias_fullname(fullname);
		(void) alias_fullname.last(iter_pos);
		alias_fullname.insert_name(iter_pos, FNSP_host_string_sc);
		fnsbind(org_ctx, alias_fullname, canonical_hostname_name,
			full_hname, *ref);
		delete ref;
	}
	return (1);
}

// construct full and relative names for "user/"
static int
create_org_user_context(const FN_ref *org_ref,
    const FN_composite_name &fullname, int subcontexts_p)
{
	FN_status status;
	int count = 0;
	FN_ctx *org_ctx = FN_ctx::from_ref(*org_ref,
	    AUTHORITATIVE, status);
	if (org_ctx == 0) {
		FN_string *desc = status.description();
		fprintf(stderr, "%s: %s: %s\n", program_name,
		gettext("Could not generate context from nsid reference"),
			desc ?(char *)(desc->str()):
			gettext("No status description"));
		delete desc;
		return (0);
	}

	void *iter_pos;
	FN_ref *ref;

	// construct full and relative names for "user/"
	FN_composite_name full_uname(fullname);
	(void) full_uname.last(iter_pos);
	full_uname.insert_name(iter_pos, FNSP_user_string);
	if (ref = create_username_context(org_ctx,
	    full_uname, custom_username_name, subcontexts_p, count)) {
		FN_composite_name alias_fullname(fullname);
		(void) alias_fullname.last(iter_pos);
		alias_fullname.insert_name(iter_pos, FNSP_user_string_sc);
		fnsbind(org_ctx, alias_fullname, canonical_username_name,
			full_uname, *ref);
		delete ref;
	}
	return (1);
}

// Creation of org context, create the bindings for service and printer.
// Also creates contexts for hostname and username contexts.
// subcontext_p determines whether these are populated.
// Three routines for nisplus, nis and files
FN_ref *
create_nisplus_org_context(
	FN_ctx *ctx,
	const FN_composite_name &fullname,
	const FN_composite_name &name,
	int subcontexts_p)
{
	// create nsid context for organization
	FN_ref *org_ref;
	FN_status status;
	int count = 0;
	FN_ref *service_ref;

	org_ref = create_nsid_and_service_context(ctx, fullname, name,
	    count, FNSP_nsid_context, &service_ref);

	if (org_ref == 0)
		return (0);

	// Create the printer context
	if (!create_printer_context(service_ref, fullname)) {
		delete service_ref;
		delete org_ref;
		return (0);
	}
	delete service_ref;

	// Create hostname and username context
	if ((!create_org_user_context(org_ref, fullname, subcontexts_p)) ||
	    (!create_org_host_context(org_ref, fullname, subcontexts_p))) {
		delete org_ref;
		return (0);
	}

	return (org_ref);
}

void
check_error(FN_status &status, char *msg = 0)
{
	if (!status.is_success()) {
		fprintf(stderr, "Error: %s\n",
		    status.description()->str());
		usage(program_name, msg);
	}
}

// Routine to create org context for NIS
FN_ref *
create_nis_org_context(
	FN_ctx *initial_context,
	const FN_composite_name &fullname,
	const FN_composite_name & /* name */,
	int subcontexts_p)
{
	unsigned nis_status;
	int answer;
	int count = 0;
	if ((nis_status = FNSP_create_nis_orgcontext(0))
	    != FN_SUCCESS) {
		if (nis_status == FN_E_UNSPECIFIED_ERROR) {
			printf(
			    gettext("\tThis machine is not NIS master server.\n"
			    "\tBy installing FNS on this machine,"
			    "the FNS maps\n\thave to be copied (using "
			    "ypxfr) by system administrator\n"
			    "\ton to NIS replicas and NIS master server.\n"));
			printf(gettext("\tDo you still want to"
			    " install FNS [y/n]: "));
			answer = getchar();
			if ((answer == 'y') || (answer == 'Y')) {
				if ((nis_status = FNSP_create_nis_orgcontext(1))
				    != FN_SUCCESS)
					return (0);
			} else {
				printf(gettext("FNS not installed\n"));
				return (0);
			}
		} else {
			FN_status stat;
			stat.set_code(nis_status);
			fprintf(stderr,
			    gettext("Unable to create FNS org context\n"));
			fprintf(stderr, gettext("ERROR: %s\n"),
			    stat.description()->str());
			return (0);
		}
	}

	FN_status status;
	FN_string *fullname_str = fullname.string();
	FN_ref *parent_ref = initial_context->lookup(fullname, status);
	if (status.is_success()) {
		if (verbose)
			printf(gettext("Binding for %s already exists\n"),
			    fullname_str->str());
	} else {
		parent_ref = initial_context->create_subcontext(fullname, status);
		check_error(status, gettext("Unable to create org context"));
	}

	FN_ctx *ctx = FN_ctx::from_ref(*parent_ref, AUTHORITATIVE, status);
	check_error(status, gettext("Unable to get parent context"));

	void *iter_pos;
	FN_composite_name service_fullname(fullname);

	if (service_fullname.last(iter_pos)->is_empty())
		service_fullname.insert_comp(iter_pos, FNSP_service_string);
	else
		service_fullname.append_name(custom_service_name);

	FN_ref *service_ref = create_service_context(ctx, service_fullname,
	    custom_service_name, count);

	if (service_ref) {
		// Add alias
		FN_composite_name alias_name(fullname);
		if (alias_name.last(iter_pos)->is_empty())
			alias_name.insert_comp(iter_pos,
			    FNSP_service_string_sc);
		else
			alias_name.append_name(canonical_service_name);
		fnsbind(ctx, alias_name, canonical_service_name,
		    service_fullname, *service_ref);

		// Create the printer context
		if (!create_printer_context(service_ref, fullname)) {
			delete service_ref;
			delete parent_ref;
			delete ctx;
			delete fullname_str;
			return (0);
		}
		delete service_ref;
	}
	delete ctx;
	delete fullname_str;

	// Create hostname and username context
	if ((!create_org_user_context(parent_ref, fullname, subcontexts_p)) ||
	    (!create_org_host_context(parent_ref, fullname, subcontexts_p))) {
		delete parent_ref;
		return (0);
	}
	return (parent_ref);
}

// Routuine to create org context for *files*
FN_ref *
create_files_org_context(
	FN_ctx *initial_context,
	const FN_composite_name &fullname,
	const FN_composite_name & /* name */,
	int subcontexts_p)
{
	FN_status status;
	unsigned stat;
	if ((stat = FNSP_create_files_orgcontext()) != FN_SUCCESS) {
		status.set(stat);
		check_error(status, "Unable to create org context");
		return (0);
	}

	FN_string *fullname_str = fullname.string();
	FN_ref *parent_ref = initial_context->lookup(fullname, status);
	if (status.is_success()) {
		if (verbose)
			printf(gettext("Binding for %s already exists\n"),
			    fullname_str->str());
	} else {
		initial_context->create_subcontext(fullname, status);
		check_error(status, gettext("Unable to create org context"));
		if (verbose)
			printf(gettext("Created binding for '%s' context.\n"),
			    fullname_str->str());
		parent_ref = initial_context->lookup(fullname, status);
		check_error(status, gettext("Unable to get parent reference"));
	}
	delete fullname_str;

	FN_ctx *ctx = FN_ctx::from_ref(*parent_ref, AUTHORITATIVE, status);
	check_error(status, gettext("Unable to get parent context"));

	int count = 0;
	void *iter_pos;
	FN_composite_name service_fullname(fullname);

	if (service_fullname.last(iter_pos)->is_empty())
		service_fullname.insert_comp(iter_pos, FNSP_service_string);
	else
		service_fullname.append_name(custom_service_name);

	FN_ref *service_ref = create_service_context(ctx, service_fullname,
	    custom_service_name, count);

	if (service_ref) {
		// Add alias
		FN_composite_name alias_name(fullname);
		if (alias_name.last(iter_pos)->is_empty())
			alias_name.insert_comp(iter_pos,
			    FNSP_service_string_sc);
		else
			alias_name.append_name(canonical_service_name);
		fnsbind(ctx, alias_name, canonical_service_name,
		    service_fullname, *service_ref);

		// Create the printer context
		if (!create_printer_context(service_ref, fullname)) {
			delete service_ref;
			delete parent_ref;
			delete ctx;
			return (0);
		}
		delete service_ref;
	}
	delete ctx;

	// Create hostname and username context
	if ((!create_org_user_context(parent_ref, fullname, subcontexts_p)) ||
	    (!create_org_host_context(parent_ref, fullname, subcontexts_p))) {
		delete parent_ref;
		return (0);
	}

	return (parent_ref);
}

FN_ref *
create_org_context(
	FN_ctx *ctx,
	const FN_composite_name &fullname,
	const FN_composite_name &name,
	int subcontexts_p)
{
	switch (name_service) {
	case FNSP_files_ns:
		return (create_files_org_context(ctx,
		    fullname, name, subcontexts_p));
	case FNSP_nis_ns:
		return (create_nis_org_context(ctx,
		    fullname, name, subcontexts_p));
	case FNSP_nisplus_ns:
	default:
		return (create_nisplus_org_context(ctx,
		    fullname, name, subcontexts_p));
	}
}


/*
 * Lookup the 'parent' context in which the context is to be created.
 * If the name ends in a '/', the parent name consists of
 * fullname up to the second last '/' encountered.
 * e.g. '_org//_service/', parent = '_org//', rest = '_service/'
 *      '_org//', parent = 'org/', rest = '/'
 * If the name does not end in a '/', the parent name consists of
 * fullname up to the last '/'.
 * e.g.  '_org//_service/abc', parent = '_org//service/', rest = 'abc'
 *      '_org/abc', parent = '_org/', rest = 'abc'
 */

FN_ctx *
lookup_parent_context(
	const FN_composite_name &fullname,
	FN_composite_name &rest,
	FN_status &status,
	FN_composite_name **save_parent_name = 0)
{
	if (fullname.count() < 2) {
		status.set(FN_E_ILLEGAL_NAME, 0, 0, &fullname);
		return (0);
	}

	FN_ctx *initial_context = FN_ctx::from_initial(AUTHORITATIVE, status);

	if (initial_context == 0) {
		FN_string *desc = status.description();
		fprintf(stderr, "%s: %s: %s\n",
			program_name,
			gettext("Unable to get initial context"),
			desc? (char *)(desc->str()):
			gettext("No status description"));
		delete desc;
		exit(1);
	}

	void *lp;
	const FN_string *lastcomp = fullname.last(lp);
	const FN_string *second_lastcomp = 0;
	if (lastcomp == 0 || lastcomp->is_empty()) {
		// last component is null (i.e. names ends in '/')
		// rest is second last component plus '/'.
		// parent is first component to '/' before rest
		second_lastcomp = fullname.prev(lp);
		rest.append_comp(*second_lastcomp);
		rest.append_comp(FNSP_empty_component);
	} else {
		// last component is a component name
		rest.append_comp(*lastcomp);
	}
	FN_composite_name *parent_name = fullname.prefix(lp);

	if (parent_name == 0) {
		// we were given a name like "xxx/",
		fprintf(stderr, "%s: %s.\n", program_name,
	gettext("Cannot create new bindings in the initial context"));
		exit(1);
	}

	parent_name->append_comp(FNSP_empty_component); // append null component

	FN_ctx *target_ctx = 0;

	FN_ref *parent_ref = initial_context->lookup(*parent_name, status);
	delete initial_context;
	if (status.is_success()) {
		target_ctx = FN_ctx::from_ref(*parent_ref,
		    AUTHORITATIVE, status);
		delete parent_ref;
	} else {
		const FN_composite_name *cp = status.remaining_name();

		if (cp) {
			void *iter_pos;
			// 'srname' = remaining_name without trailing null
			// component
			// parent has trailing FNSP_empty_component;
			cp->last(iter_pos);
			FN_composite_name *srname = cp->prefix(iter_pos);

			// tag on remaining name to make status sensible
			srname->append_name(rest);
			status.set_remaining_name(srname);
			delete srname;
		} else {
			status.set_remaining_name(&rest);
		}
	}

	if (save_parent_name)
		*save_parent_name = parent_name;
	else
		delete parent_name;

	return (target_ctx);
}

int
process_nisplus_org_request(const FN_composite_name &fullname,
    FN_status &status)
{
	FN_string *desc;
	if (fullname.count() < 2) {
		status.set(FN_E_ILLEGAL_NAME, 0, 0, &fullname);
		return (0);
	}

	// Check for the presense of ctx_dir.
	// This should be done before the intial context is obtained
	char domain[NIS_MAXNAMELEN];
	sprintf(domain, "ctx_dir.%s", nis_local_directory());
	FN_string domain_string((unsigned char *) domain);
	unsigned ret_status = FNSP_create_directory(domain_string, 0);
	if (ret_status != FN_SUCCESS) {
		status.set(ret_status, 0, 0, &fullname);
		FN_string *desc = status.description();
		fprintf(stderr, "%s: %s %s %s: %s\n",
			program_name, gettext("create of"), domain,
			gettext("directory failed"),
			desc ? ((char *)(desc->str())) :
			gettext("No status description"));
		delete desc;
		return (0);
	}
	FN_composite_name rest;
	FN_ctx *target_ctx =
	    lookup_parent_context(fullname, rest, status);
		if (target_ctx == 0) {
			desc = status.description();
			FN_string *fstr = fullname.string();
			fprintf(stderr, "%s: %s '%s' %s: %s\n",
				program_name, gettext("create of"),
				(fstr ? (char *)fstr->str() : ""),
				gettext("failed"),
				desc ? ((char *)(desc->str())) :
				gettext("No status description"));
			delete desc;
			delete fstr;
			return (0);
	}

	FN_ref *orgnsid_ref = create_org_context(target_ctx, fullname, rest,
	    create_subcontexts_p);
	delete orgnsid_ref;
	delete target_ctx;
	return (orgnsid_ref != 0);
}

int
process_nis_org_request(const FN_composite_name &fullname, FN_status &status)
{
	FN_composite_name parent_name(fullname);
	FN_string *parent_str = parent_name.string();
	if (strcmp((char *) parent_str->str(), "org") == 0) {
		parent_name.append_name(FNSP_empty_component);
		parent_name.append_name(FNSP_empty_component);
	} else
		if (strcmp((char *) parent_str->str(), "org/") == 0)
			parent_name.append_name(FNSP_empty_component);
	else
		if (strcmp((char *) parent_str->str(), "org//") == 0);
	else {
		status.set(FN_E_ILLEGAL_NAME, 0, 0, &parent_name);
		check_error(status, "Name should be \"org\"");
	}
	delete parent_str;

	FN_ctx *initial_context = FN_ctx::from_initial(AUTHORITATIVE, status);
	check_error(status, "Unable to obtain initial context");

	FN_composite_name rest;
	FN_ref *orgnsid_ref = create_org_context(initial_context,
	    parent_name, rest, create_subcontexts_p);

	if (orgnsid_ref) {
		delete orgnsid_ref;
	}
	delete initial_context;
	return (orgnsid_ref != 0);
}

int
process_org_request(const FN_composite_name &fullname, FN_status &status)
{
	switch (name_service) {
	case FNSP_nis_ns:
	case FNSP_files_ns:
		return (process_nis_org_request(fullname, status));
	case FNSP_nisplus_ns:
	default:
		return (process_nisplus_org_request(fullname, status));
	}
}

int
process_hostname_request(
	FN_ctx *target_ctx,
	const FN_composite_name &fullname,
	const FN_composite_name &name,
	const FN_composite_name &parent_name)
{
	int count = 0;
	FN_ref *hostname_ref = 0;
	if (name_is_special_token(name, FNSP_hostname_context)) {
		FN_composite_name *fn, *afn;
		const FN_composite_name *rn, *arn;
		if (assign_aliases(parent_name, FNSP_hostname_context, &fn, &rn,
		    &afn, &arn)) {
			hostname_ref = create_hostname_context(target_ctx,
			    *fn,
			    *rn,
			    create_subcontexts_p,
			    count);
			if (hostname_ref)
				fnsbind(target_ctx, *afn, *arn, *fn,
				    *hostname_ref);
		}
		if (fn) delete fn;
		if (afn) delete afn;
	} else {
		hostname_ref = create_hostname_context(target_ctx,
		    fullname,
		    name,
		    create_subcontexts_p,
		    count);
	}
	delete hostname_ref;
	return (hostname_ref != 0);
}

int
process_username_request(
	FN_ctx *target_ctx,
	const FN_composite_name &fullname,
	const FN_composite_name &name,
	const FN_composite_name &parent_name)
{
	int count = 0;
	FN_ref *username_ref = 0;
	if (name_is_special_token(name, FNSP_username_context)) {
		FN_composite_name *fn, *afn;
		const FN_composite_name *rn, *arn;
		if (assign_aliases(parent_name, FNSP_username_context, &fn, &rn,
		    &afn, &arn)) {
			username_ref = create_username_context(target_ctx,
			    *fn, *rn, create_subcontexts_p, count);
			if (username_ref)
				fnsbind(target_ctx, *afn, *arn, *fn,
				    *username_ref);
		}
		if (fn) delete fn;
		if (afn) delete afn;
	} else {
		username_ref = create_username_context(target_ctx,
		    fullname,
		    name,
		    create_subcontexts_p,
		    count);
	}
	delete username_ref;
	return (username_ref != 0);
}

#include <limits.h>

static int
present_in_passwd_file(const FN_composite_name &name)
{
	char passwd_name[MAX_CANON+1];
	FILE *passwdfile;
	FN_string *name_str;

	name_str = name.string();
	strcpy(passwd_name, (char *) name_str->str());
	delete name_str;
	if ((passwdfile = fopen(FNSP_get_user_source_table(), "r")) == NULL) {
		fprintf(stderr,
		    gettext("%s: could not open file %s for read\n"),
		    program_name, FNSP_get_user_source_table());
		exit(1);
	}

	// Use fgetpwent_r to determine if the name is present
	// in the passwd file
	char buffer[MAX_CANON+1];
	struct passwd pw_temp, *pw;
	while ((pw = fgetpwent_r(passwdfile, &pw_temp,
	    buffer, MAX_CANON)) != 0) {
		if (strcmp(pw->pw_name, passwd_name) == 0) {
			fclose(passwdfile);
			return (1);
		}
	}
	fclose(passwdfile);
	return (0);
}

// Most of the work is trying to determine whether user has a password
// entry, and if so, print out a warning before creation.

int
process_user_request(
	FN_ctx *target_ctx,
	const FN_composite_name &fullname,
	const FN_composite_name &name)
{
	unsigned nisflags = 0;  // FOLLOW_LINKS? FOLLOW_PATHS?
	nis_result* res = 0;
	int count = 0;
	FN_ref *usernsid_ref = 0;
	char tname[NIS_MAXNAMELEN+1];
	FN_composite_name fcname(fullname);
	void *iter;
	const FN_string *user_name = name.first(iter);

	// 'target_ctx' points to username context;
	// get domain name of username context
	unsigned status;
	FN_status stat;
	FN_ref *username_ref = target_ctx->get_ref(stat);
	FN_string *username_dir =
	    FNSP_reference_to_internal_name(*username_ref);
	if (username_dir == 0) {
		fprintf(stderr, "%s: %s.\n", program_name,
		    gettext(
		    "Could not obtain object name of username context"));
		return (0);
	}

	FN_string *domainname;
	if (name_service == FNSP_files_ns) {
		domainname = new FN_string(FNSP_empty_component);
		if (!present_in_passwd_file(name))
			fprintf(stderr,
			    gettext(
			    "WARNING: user '%s' not in passwd table.\n"),
			    user_name->str());
	} else if (name_service == FNSP_nis_ns) {
		domainname = FNSP_nis_orgname_of(*username_dir, status);
		int maplen;
		char *mapentry;
		if (yp_match((char *) domainname->str(), "passwd.byname",
		    (char *) user_name->str(),
		    strlen((char *) (user_name->str())),
		    &mapentry, &maplen) != 0)
			fprintf(stderr,
			    gettext("ERROR: user '%s' not in passwd table.\n"),
			    user_name->str());
	} else {
		domainname = FNSP_orgname_of(*username_dir, status);
		sprintf(tname, "[name=\"%s\"], %s.%s",
		    user_name->str(), FNSP_get_user_source_table(),
		    domainname->str());
		if (tname[strlen(tname)-1] != '.')
			strcat(tname, ".");

		res = nis_list(tname, nisflags, 0, 0);

		if (res && (res->status != NIS_SUCCESS &&
		    res->status != NIS_S_SUCCESS))
			fprintf(stderr,
			gettext("WARNING: user '%s' not in passwd table.\n"),
			    user_name->str());
	}

	delete username_dir;
	usernsid_ref = create_user_context(target_ctx, *username_ref,
	    fullname, name, *domainname, *user_name,
	    create_subcontexts_p, count);
	delete username_ref;
	delete usernsid_ref;
	delete domainname;
	return (usernsid_ref != 0);
}

/* Create host context with name 'name' in context 'target_ctx'. */
static int
strpresent(const char *inbuf, const char *ptr)
{
	if (inbuf == 0)
		return (0);
	if (ptr == 0)
		return (1);

	size_t i = strlen(inbuf);
	size_t j = strlen(ptr);
	if (i < j)
		return (0);
	for (size_t k = 0; k < (i - j + 1); k++) {
		if ((strncmp(&inbuf[k], ptr, j) == 0) &&
		    ((inbuf[k+j] == ' ') || (inbuf[k+j] == '\t') ||
		     (inbuf[k+j] == '\n') || (inbuf[k+j] == EOF)))
			if ((k != 0) && ((inbuf[k-1] == ' ') ||
			    (inbuf[k-1] == '\t')))
				return (1);
	}
	return (0);
}

static char *
present_in_hosts_file(const FN_composite_name &name)
{
	char *answer = 0;
	FN_string *name_str = name.string();
	char hosts_name[FNS_NIS_INDEX];
	strcpy(hosts_name, (char *) name_str->str());
	delete name_str;

	FILE *hostsfile;
	if ((hostsfile = fopen(FNSP_get_host_source_table(), "r")) == NULL) {
		fprintf(stderr,
		    gettext("%s: could not open file %s for read\n"),
		    program_name, FNSP_get_host_source_table());
		return (0);
	}

	char line[FNS_NIS_SIZE];
	while (fgets(line, FNS_NIS_SIZE, hostsfile)) {
		if (strpresent(line, hosts_name)) {
			answer = (char *) malloc(strlen(line) + 1);
			strcpy(answer, line);
			break;
		}
	}
	fclose(hostsfile);
	return (answer);
}

static int
process_host_request(
	FN_ctx *target_ctx,
	const FN_composite_name &fullname,
	const FN_composite_name &name)
{
	unsigned nisflags = 0;   // FOLLOW_LINKS? FOLLOW_PATHS
	nis_result* res = 0;
	struct traverse_data td;
	char tname[NIS_MAXNAMELEN+1];
	int i = 1;  // failure by default
	int count = 0;

	// 'target_ctx' points to hostname context;
	// Get domain name of hostname context

	unsigned status;
	FN_status stat;
	FN_ref *hostname_ref = target_ctx->get_ref(stat);
	FN_string *hostname_dir =
	    FNSP_reference_to_internal_name(*hostname_ref);
	if (hostname_dir == 0) {
		fprintf(stderr, "%s: %s.\n", program_name,
		gettext("Could not obtain object name of hostname context"));
		return (0);
	}
	FN_string *domainname;
	switch (name_service) {
	case FNSP_nisplus_ns:
		domainname = FNSP_orgname_of(*hostname_dir, status);
		break;
	case FNSP_nis_ns:
		domainname = FNSP_nis_orgname_of(*hostname_dir, status);
		break;
	case FNSP_files_ns:
	default:
		domainname = new FN_string(FNSP_empty_component);
		break;
	}
	delete hostname_dir;

	// Get name of hostname context and hostname from fullname
	void *iter;
	const FN_string *host_name;
	const FN_string *trailer = fullname.last(iter);

	if (trailer->is_empty())			// Trailing slash
		host_name = fullname.prev(iter);  	//  next last is host
	else
		host_name = trailer;
	FN_composite_name *parent_name = fullname.prefix(iter);

	if (parent_name == 0) {
		// we were given a name like "xxx/"
		fprintf(stderr,	"%s: %s.\n", program_name,
		    gettext("Cannot create bindings in the initial context"));
		delete hostname_ref;
		delete domainname;
		return (0);
	}

	parent_name->append_comp(FNSP_empty_component);

	// Construct arguments for process_host_entry call
	td.parent = target_ctx;
	td.parent_ref = hostname_ref;
	td.parent_name = parent_name;
	td.subcontext_p = create_subcontexts_p;
	td.domain_name = domainname;
	td.count = 0;


	// Split based on the naming service
	int entry_processed = 0;
	switch (name_service) {
	case FNSP_nisplus_ns:
		// Construct NIS+ entry name for host
		sprintf(tname, "[name=\"%s\"],%s.%s",
		    host_name->str(), FNSP_get_host_source_table(),
		    domainname->str());
		if (tname[strlen(tname)-1] != '.')
			strcat(tname, ".");

		res = nis_list(tname, nisflags, 0, 0);  // get entry for host

		if (res && (res->status == NIS_SUCCESS ||
		    res->status == NIS_S_SUCCESS)) {
			// process_host_entry returns 0 if OK, 1 for error
			i = process_host_entry(0,
			    &(res->objects.objects_val[0]), (void *)&td);
			entry_processed = 1;
		} else
			fprintf(stderr,
			    gettext("WARNING: host '%s' not in hosts table.\n"),
			    host_name->str());
		break;
	case FNSP_files_ns:
		char *file_entry;
		if (!(file_entry = present_in_hosts_file(name)))
			fprintf(stderr,
			    gettext("WARNING: host '%s' not in hosts table.\n"),
			    host_name->str());
		else {
			i = process_nis_host_entry(file_entry, &td);
			free(file_entry);
			entry_processed = 1;
		}
		break;
	case FNSP_nis_ns:
		// Must be NIS
		int maplen;
		char *mapentry;
		if (yp_match((char *) domainname->str(), "hosts.byname",
		    (char *) host_name->str(),
		    strlen((char *) (host_name->str())),
		    &mapentry, &maplen) != 0) {
			fprintf(stderr,
			    gettext("ERROR: user '%s' not in hosts table.\n"),
			    host_name->str());
		} else {
			i = process_nis_host_entry(mapentry, &td);
			free(mapentry);
			entry_processed = 1;
		}
		break;
	}

	if (!entry_processed) {
		FN_ref *ref = create_host_context(target_ctx, *hostname_ref,
		    fullname, name, *domainname, *host_name,
		    create_subcontexts_p, count);
		i = (ref != 0);
		delete ref;
	}

	delete hostname_ref;
	delete domainname;
	delete parent_name;
	if (res)
		nis_freeresult(res);
	return (i == 0);
}

int
process_site_request(
	FN_ctx *target_ctx,
	const FN_composite_name &fullname,
	const FN_composite_name &name,
	const FN_composite_name &parent_name)
{
	int count = 0;
	FN_ref *site_ref = 0;

	if (name_is_special_token(name, FNSP_site_context)) {
		FN_composite_name *fn, *afn;
		const FN_composite_name *rn, *arn;
		if (assign_aliases(parent_name, FNSP_site_context, &fn, &rn,
		    &afn, &arn)) {
			site_ref = create_site_context(target_ctx,
			    *fn, *rn, create_subcontexts_p, count);
			if (site_ref)
				fnsbind(target_ctx, *afn, *arn, *fn, *site_ref);
		}
		if (fn) delete fn;
		if (afn) delete afn;
	} else {
		site_ref = create_site_context(target_ctx, fullname, name,
		    create_subcontexts_p, count);
	}
	if (site_ref)
		delete site_ref;
	return (site_ref != 0);
}


int
process_service_request(FN_ctx *target_ctx,
			const FN_composite_name &fullname,
			const FN_composite_name &name,
			const FN_composite_name &parent_name)
{
	int count = 0;
	FN_ref *service_ref = 0;
	if (name_is_special_token(name, FNSP_service_context)) {
		FN_composite_name *fn, *afn;
		const FN_composite_name *rn, *arn;
		if (assign_aliases(parent_name, FNSP_service_context, &fn, &rn,
		    &afn, &arn)) {
			service_ref = create_service_context(target_ctx,
			    *fn, *rn, count);
			if (service_ref)
				fnsbind(target_ctx, *afn, *arn, *fn,
				    *service_ref);
		}
		if (fn) delete fn;
		if (afn) delete afn;
	} else {
		service_ref = create_service_context(target_ctx, fullname,
		    name, count);
	}
	delete service_ref;
	return (service_ref != 0);
}

int
process_generic_request(FN_ctx *target_ctx,
			const FN_composite_name &fullname,
			const FN_composite_name &name,
			const char * ref_type)
{
	int count = 0;
	FN_ref *ref = create_generic_context(target_ctx, fullname,
	    name, count, ref_type);
	delete ref;
	return (ref != 0);
}

int
process_nsid_request(
	FN_ctx *target_ctx,
	const FN_composite_name &fullname,
	const FN_composite_name &name)
{
	int count = 0;
	FN_ref *nsid_ref = create_nsid_context(target_ctx, fullname, name,
	    count);
	delete nsid_ref;
	return (nsid_ref != 0);
}

int
process_fs_request(
	FN_ctx *target_ctx,
	const FN_composite_name & /* fullname */,
	const FN_composite_name &name,
	const FN_composite_name &parent_name)
{
	int count = 0;
	FN_status status;
	FN_ref *parent_ref = target_ctx->get_ref(status);
	const FN_identifier *reftype;

	if (parent_ref == 0 || (reftype = parent_ref->type()) == 0)
		return (0);

	// determine whether parent is a user or a host
	unsigned int parent_type;
	if (*reftype == *FNSP_reftype_from_ctxtype(FNSP_host_context))
		parent_type = FNSP_host_context;
	else if (*reftype == *FNSP_reftype_from_ctxtype(FNSP_user_context))
		parent_type = FNSP_user_context;
	else {
		fprintf(stderr, "%s: %s\n", program_name,
	gettext("can only create fs bindings for a host or a user"));
		delete parent_ref;
		return (0);
	}

	// determine domain name
	FN_string *internal_name =
	    FNSP_reference_to_internal_name(*parent_ref);
	if (internal_name == 0) {
		fprintf(stderr, "%s: %s.\n", program_name,
	gettext("Could not obtain internal name of target context"));
		delete parent_ref;
		return (0);
	}
	unsigned int s;
	FN_string *domain_name = FNSP_orgname_of(*internal_name, s);
	delete internal_name;
	if (domain_name == 0) {
		fprintf(stderr, "%s: %s.\n", program_name,
	gettext("Could not obtain directory name of target context"));
		delete parent_ref;
		return (0);
	}

	// determine object name (i.e. host or user name)
	void *iter;
	const FN_string *obj_name = parent_name.last(iter);
	if (obj_name && obj_name->is_empty())
		obj_name = parent_name.prev(iter);
	if (obj_name == 0) {
		fprintf(stderr, "%s: %s.\n", program_name,
		    gettext("Could not obtain object name of target context"));
		delete domain_name;
		delete parent_ref;
		return (0);
	}

	int bind_status;
	if (name_is_special_token(name, FNSP_fs_context)) {
		switch (parent_type) {
		case FNSP_host_context:
			bind_status = create_host_fs(target_ctx, parent_name,
						*obj_name, *domain_name);
			break;
		case FNSP_user_context:
			bind_status = create_user_fs(target_ctx, parent_name,
						*obj_name, *domain_name);
			break;
		}
	} else {
		switch (parent_type) {
		case FNSP_host_context:
			bind_status = create_host_fs(target_ctx, parent_name,
			    *obj_name, *domain_name, &name);
			break;
		case FNSP_user_context:
			bind_status = create_user_fs(target_ctx, parent_name,
			    *obj_name, *domain_name, &name);
			break;
		}
	}

	delete domain_name;
	delete parent_ref;
	return (bind_status);
}


// Returns 1 if error encountered; 0 if OK
static int
process_user_entry_aux(char *user_str, int len, struct traverse_data *td)
{
	FN_ref *ref;
	FN_string user_name((unsigned char *)user_str, len);
	void *iter_pos;

	// Generate full and relative names of "<user>/"
	FN_composite_name user_nsid(user_name);
	user_nsid.append_comp(FNSP_empty_component);
	FN_composite_name user_full_nsid(*(td->parent_name));
	(void) user_full_nsid.last(iter_pos);
	user_full_nsid.insert_comp(iter_pos, user_name);

	// Create context using user
	ref = create_user_context(td->parent, *(td->parent_ref),
	    user_full_nsid, user_nsid, *(td->domain_name), user_name,
	    td->subcontext_p, td->count);
	if (ref) {
		delete ref;
		return (0);
	}
	return (1);
}

#define	MAXINPUTLEN 256

extern FILE *get_user_file(const char *, const char *);
extern FILE *get_nis_user_file(const char *, const char *);
extern FILE *get_files_user_file(const char *, const char *);
extern FILE *get_host_file(const char *, const char *);
extern FILE *get_nis_host_file(const char *, const char *);
extern FILE *get_files_host_file(const char *, const char *);
extern void free_user_file(FILE *);
extern void free_host_file(FILE *);

// Returns 1 if error encountered; 0 if OK
static int
process_user_entry(char *, nis_object *ent, void *udata)
{
	struct traverse_data *td = (struct traverse_data *) udata;
	long entry_type;

	// extract user name from entry
	entry_type = *(long *)
	    (ent->EN_data.en_cols.en_cols_val[0].ec_value.ec_value_val);

	if (entry_type == ENTRY_OBJ) {
		fprintf(stderr, "%s\n",
			gettext("Encountered object that is not an entry"));
		return (1);
	}

	return (process_user_entry_aux(ENTRY_VAL(ent, 0),
	    ENTRY_LEN(ent, 0), td));
}


int
traverse_user_list(
	const FN_string &domainname,
	FN_ctx *parent,
	const FN_ref &parent_ref,
	const FN_composite_name &parent_name,
	int subcontext_p)
{
	struct traverse_data td;
	char *user;
	FILE *userfile;
	char line[MAXINPUTLEN];

	td.parent = parent;
	td.parent_ref = &parent_ref;
	td.parent_name = &parent_name;
	td.domain_name = &domainname;
	td.subcontext_p = subcontext_p;
	td.count = 0;

	if (input_file) {
		userfile = fopen(input_file, "r");
		if (userfile == 0) {
			fprintf(stderr, "%s: %s '%s'.",	program_name,
			gettext("Could not open input file"), input_file);
		}
	} else {
		if (name_service == FNSP_nisplus_ns)
			userfile = get_user_file(program_name,
			    (const char *)domainname.str());
		else if (name_service == FNSP_nis_ns)
			userfile = get_nis_user_file(program_name,
			    (const char *)domainname.str());
		else
			userfile = get_files_user_file(program_name,
			    (const char *) domainname.str());
	}
	if (userfile == NULL)
		return (0);

	while (fgets(line, MAXINPUTLEN, userfile) != NULL) {
		user = strtok(line, "\n\t ");
		if (user == 0 ||
		    (process_user_entry_aux(user, strlen(user), &td) != 0))
			break;
	}
	if (input_file)
		fclose(userfile);
	else
		free_user_file(userfile);
	return (td.count);
}


// Return 1 if error; 0 if OK
int
process_host_entry_aux(
	char *chost,
	int clen,
	char *host,
	int len,
	struct traverse_data *td)
{
	FN_ref *ref;
	FN_string can_host((unsigned char *)chost, clen);
	FN_string alias_host((unsigned char *)host, len);

	ref = create_host_context_can(td->parent, *(td->parent_ref),
	    *(td->parent_name), can_host, alias_host, *(td->domain_name),
	    td->subcontext_p, td->count);
	if (ref) {
		delete ref;
		return (0);
	}
	return (1);
}


// Return 1 if error encountered; 0 if OK
int
process_host_entry(char *, nis_object *ent, void *udata)
{
	struct traverse_data *td = (struct traverse_data *) udata;
	long entry_type;

	// extract host name from entry
	entry_type = *(long *)
	    (ent->EN_data.en_cols.en_cols_val[0].ec_value.ec_value_val);

	if (entry_type == ENTRY_OBJ) {
		fprintf(stderr, "%s\n",
			gettext("Encountered object that is not an entry"));
		return (1);
	}

	return (process_host_entry_aux(ENTRY_VAL(ent, 0), ENTRY_LEN(ent, 0),
	    ENTRY_VAL(ent, 1), ENTRY_LEN(ent, 1),
	    td));
}

static int
process_mapentry_of_host(char *line, void *udata)
{
	struct traverse_data *td = (struct traverse_data *) udata;
	char can_host[FNS_NIS_SIZE], host[FNS_NIS_INDEX];
	char *ptr, *host_ptr;

	// Get the primay host name
	host_ptr = strpbrk(line, " \t\n\0");
	if (host_ptr == 0)
		return (1);
	strncpy(can_host, line, (host_ptr-line));
	can_host[host_ptr-line] = '\0';

	// Get the alias name, if present
	while ((host_ptr) && (((*host_ptr) == ' ') || ((*host_ptr) == '\t')))
		host_ptr++;
	if ((host_ptr) && ((*host_ptr) != '\n') &&
	    ((*host_ptr) != '\0') && ((*host_ptr) != '#')) {
		ptr = strpbrk(host_ptr, " \t\n\0");
		strncpy(host, host_ptr, (ptr-host_ptr));
		host[ptr-host_ptr] = '\0';
		host_ptr = ptr;
		while ((host_ptr) && (((*host_ptr) == ' ') || ((*host_ptr) == '\t')))
			host_ptr++;
	} else
		strcpy(host, can_host);
	// Create the host context
	if (process_host_entry_aux(can_host,
	    strlen(can_host), host, strlen(host), td) != 0)
		return (1);

	// Check for other names to bind
	while ((host_ptr) && (((*host_ptr) == ' ') ||
	    ((*host_ptr) == '\t')))
		host_ptr++;
	while ((host_ptr) && ((*host_ptr) != '\n') &&
	    ((*host_ptr) != '\0') && ((*host_ptr) != '#')) {
		// Get the next host name
		ptr = strpbrk(host_ptr, " \t\n\0");
		strncpy(host, host_ptr, (ptr-host_ptr));
		host[ptr-host_ptr] = '\0';

		// Create the binding
		if (process_host_entry_aux(can_host,
		    strlen(can_host), host, strlen(host), td) != 0)
			return (1);
		// Check for the next host name
		host_ptr = ptr;
		while ((host_ptr) && (((*host_ptr) == ' ') || ((*host_ptr) == '\t')))
			host_ptr++;
	}
	return (0);
}	


// Return 1 if error encountered; 0 if OK
int
process_nis_host_entry(char *mapentry, void *udata)
{
	// Get over the IP address
	char *line = strpbrk(mapentry, " \t\n\0");
	if (line == 0)
		return (1);
	while ((*line == ' ') ||
	    (*line == '\t'))
		line++;
	return (process_mapentry_of_host(line, udata));
}

int
traverse_host_list(
	const FN_string &domainname,
	FN_ctx *parent,
	const FN_ref &parent_ref,
	const FN_composite_name &parent_name,
	int subcontext_p)
{
	struct traverse_data td;

	td.parent = parent;
	td.parent_ref = &parent_ref;
	td.parent_name = &parent_name;
	td.domain_name = &domainname;
	td.subcontext_p = subcontext_p;
	td.count = 0;

	FILE *hostfile;
	if (input_file) {
		hostfile = fopen(input_file, "r");
		if (hostfile == 0) {
			fprintf(stderr, "%s: %s '%s'.",	program_name,
			    gettext("Could not open input file"), input_file);
		}
	} else {
		if (name_service == FNSP_nisplus_ns)
			hostfile = get_host_file(program_name,
			    (const char *)domainname.str());
		else if (name_service == FNSP_nis_ns)
			hostfile = get_nis_host_file(program_name,
			    (const char *)domainname.str());
		else
			hostfile = get_files_host_file(program_name,
			    (const char *)domainname.str());
	}

	if (hostfile == NULL)
		return (0);

	char line[MAXINPUTLEN];
	while (fgets(line, MAXINPUTLEN, hostfile) != NULL)
		if (process_mapentry_of_host(line, &td) != 0)
			break;
	if (input_file)
		fclose(hostfile);
	else
		free_host_file(hostfile);
	return (td.count);
}

void
FNSP_set_naming_service()
{
	// Set if naming service
	name_service = fnselect_from_probe();
	if ((name_service == FNSP_default_ns) ||
	    (name_service == FNSP_unknown_ns))
		name_service = FNSP_nisplus_ns;
}

main(int argc, char **argv)
{
	// Detemine the naming service
	FNSP_set_naming_service();

	process_cmd_line(argc, argv);

	FN_status status;
	int exit_status = 1;
	FN_composite_name relative_name;
	FN_composite_name fullname((unsigned char *)target_name_str);
	FN_composite_name *parent_name = 0;
	FN_string *desc;

	// If request to create is for organization
	// then check for ctx_dir, so that from_initial will work fine
	if (context_type == FNSP_organization_context) {
		if ((create_subcontexts_p == 0 || check_nis_group()) &&
		    process_org_request(fullname, status))
			exit_status = 0;
		exit(exit_status);
	}

	FN_ctx *target_ctx = lookup_parent_context(fullname,
	    relative_name,
	    status,
	    &parent_name);

	if (target_ctx == 0) {
		desc = status.description();
		fprintf(stderr, "%s: %s '%s' %s: %s\n",
			program_name,
			gettext("create of"),
			target_name_str,
			gettext("failed"),
			desc ? ((char *)(desc->str())) :
			gettext("No status description"));
		delete desc;
		exit(1);
	}

	switch (context_type) {
	case FNSP_hostname_context:
		if ((create_subcontexts_p == 0 || check_nis_group()) &&
		    process_hostname_request(target_ctx, fullname,
		    relative_name, *parent_name))
			exit_status = 0;
		break;

	case FNSP_username_context:
		if ((create_subcontexts_p == 0 || check_nis_group()) &&
		    process_username_request(target_ctx, fullname,
		    relative_name, *parent_name))
			exit_status = 0;
		break;

	case FNSP_user_context:
		if (check_nis_group() &&
		    process_user_request(target_ctx, fullname, relative_name))
			exit_status = 0;
		break;

	case FNSP_host_context:
		if (check_nis_group() &&
		    process_host_request(target_ctx, fullname, relative_name))
			exit_status = 0;
		break;

	case FNSP_site_context:
		if (process_site_request(target_ctx, fullname, relative_name,
					 *parent_name))
			exit_status = 0;
		break;

	case FNSP_service_context:
		if (process_service_request(target_ctx, fullname, relative_name,
		    *parent_name))
			exit_status = 0;
		break;

	case FNSP_nsid_context:
		if (check_null_trailer(fullname, relative_name) &&
		    process_nsid_request(target_ctx, fullname, relative_name))
			exit_status = 0;
		break;

	case FNSP_generic_context:
		if (process_generic_request(target_ctx, fullname, relative_name,
		    reference_type))
			exit_status = 0;
		break;

	case FNSP_fs_context:
		if (process_fs_request(target_ctx, fullname, relative_name,
		    *parent_name))
			exit_status = 0;
		break;
	default:
		fprintf(stderr, "%s: %s: %d\n",
			argv[0], gettext("unknown context type"), context_type);
		break;
	}

	delete parent_name;
	delete target_ctx;
	exit(exit_status);
}


#include <rpc/rpc.h>  /* for XDR */

static int
__fns_xdr_encode_string(const char *str, char *buffer, size_t &len)
{
	XDR	xdr;

	xdrmem_create(&xdr, (caddr_t)buffer, len, XDR_ENCODE);
	if (xdr_string(&xdr, (char **)&str, ~0) == FALSE) {
		return (0);
	}

	len = xdr_getpos(&xdr);
	xdr_destroy(&xdr);
	return (1);
}
