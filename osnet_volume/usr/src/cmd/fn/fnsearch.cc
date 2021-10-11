/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fnsearch.cc	1.4	96/04/19 SMI"


// fnsearch [-A] [-L] [-l] [-v] [-n max] [-s scope] composite_name \
//	[-a attribute]... filter_expr [filter_arg]...
//
// attribute 	::=  [-O | -U] string
// filter_arg	::=  [-O | -U] string
// scope	::=  object | context | subtree | constrained_subtree


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <libintl.h>
#include <rpc/types.h>
#include <xfn/xfn.hh>
#include "fnattr_utils.hh"


// Command line arguments

typedef struct {
	unsigned int	format;
	char		*value;
} ident_t;

static struct {
	char		*command;
	bool_t		authoritative;
	bool_t		follow_links;
	bool_t		return_refs;
	bool_t		debug;
	bool_t		force_extended;
	unsigned int	output_detail;
	unsigned int	max;
	unsigned int	scope;
	char		*cname;
	ident_t		*attrs;
	size_t		num_attrs;
	char		*filter_expr;
	ident_t		*filter_args;
	size_t		num_filter_args;
} cl;


// Values of the "-s" option

static const struct {
	const char	*name;
	unsigned int	scope;
} scopes[] = {
	{"object",		FN_SEARCH_NAMED_OBJECT},
	{"context",		FN_SEARCH_ONE_CONTEXT},
	{"subtree",		FN_SEARCH_SUBTREE},
	{"constrained_subtree",	FN_SEARCH_CONSTRAINED_SUBTREE}
};
static const int SCOPES_SZ = sizeof (scopes) / sizeof (*scopes);


// Class used to allocate and manage the information that is needed to
// build the structures for the search operations.
//
// Using the filter expression and the information about the filter
// arguments from the command line, analyze_expr() creates an
// API-style filter expression, and constructs a list of API-style
// arguments.  It also determines whether an extended search is needed
// and sets "extended" accordingly.
//
// The append_arg() methods add a new filter argument to the current
// list of arguments.  The first form uses a substitution token to format
// the next unused argument from the command line.  The second takes
// a command-line-style argument explicity.
//
// new_filter() uses the information gathered by analyze_expr() to
// construct a new FN_search_filter structure.
// new_control() constructs a new FN_search_control structure.
// new_search_attrs() constructs a new attribute set for use with
// attr_search().
//
// same_attr(i, j) returns TRUE if the i'th and j'th args are attributes
// with the same identifier.

class FilterInfo {
public:
	char *filter_expr;
	bool_t extended;	// TRUE if an extended search is needed

	FilterInfo();
	void analyze_expr();
	FN_search_control *new_control(const FN_attrset *ret_attr_ids) const;
	FN_search_filter *new_filter() const;
	FN_attrset *new_search_attrs() const;
private:
	FN_search_filter_type *types;
	void **args;
	int numargs;
	int args_used;

	void append_arg(char token);
	void append_arg(FN_search_filter_type,
	    unsigned int format,
	    const char *value,
	    size_t valuelen);
	void append_arg(FN_search_filter_type, void *arg);
	void assert_all_args_used() const;
	bool_t same_attr(int i, int j) const;
};


// Standard attribute syntax.

static FN_identifier syntax_ascii((unsigned char *)"fn_attr_syntax_ascii");


// Process the command line arguments.  argc and argv are passed
// from main().

static void
process_cmdline(int argc, char *argv[]);


// Process the attributes on the command line.  argc and argv are passed
// from main().

static void
process_cmdline_attrs(int argc, char *argv[]);


// Process the filter args on the command line.  argc and argv are passed
// from main().

static void
process_cmdline_filter_args(int argc, char *argv[]);


// Print command name, a message (if msg is non-null), and a usage
// description; then exit.

static void
usage(const char *msg = NULL);


// Build an attribute set based on the attribute information from the
// command line.

static FN_attrset *
attrs_to_return();


// Return TRUE if "fe", a pointer into the command line filter expression,
// points to a literal attribute that needs to be replaced by "%a".  If so,
// set attrlen to the length of the literal.

static bool_t
at_attr(const char *fe, int &attrlen);


// Return TRUE if "fe", a pointer into the command line filter expression,
// points to a keyword ("and", "or", or "not").  If so, set wordlen to the
// length of the keyword.

static bool_t
at_keyword(const char *fe, int &wordlen);


// Return TRUE if "fe", a pointer into the command line filter expression,
// points to an integer.  If so, set wordlen to the length of the integer.

static bool_t
at_integer(const char *fe, int &wordlen);


// Do a basic search, and print the results.

static void
do_basic_search(FN_ctx &,
    const FN_composite_name &,
    const FilterInfo &,
    const FN_attrset *ret_attr_ids);


// Do an extended search, and print the results.

static void
do_ext_search(FN_ctx &,
    const FN_composite_name &,
    const FilterInfo &,
    const FN_attrset *ret_attr_ids);


// Print the name, reference (if non-NULL), and attributes (if non-NULL) of
// a match found during a search.  The name is relative to the target context
// (cl.cname) if "relative" is TRUE, otherwise it's relative to the initial
// context.  Print the status description if the status is not "success".

static void
print_match(const FN_string &,
    const FN_ref *,
    const FN_attrset *,
    bool_t relative,
    const FN_status &);


// Assert that ptr is not null.

static void
mem_check(const void *);


// Print an error message, including the status description if status is
// non-null, and exit.  Other arguments are as for printf().

static void
error(const FN_status *, const char *fmt, ...);


// Print a status description to stderr.

static void
print_status(const FN_status &);


int
main(int argc, char *argv[])
{
	process_cmdline(argc, argv);

	FN_status status;
	FN_ctx *init_ctx = FN_ctx::from_initial(cl.authoritative, status);
	if (init_ctx == NULL) {
		error(&status, gettext("Unable to get initial context"));
	}
	FN_composite_name cname((unsigned char *)cl.cname);

	FN_attrset *ret_attr_ids = attrs_to_return();

	FilterInfo *info = new FilterInfo;
	mem_check(info);
	info->analyze_expr();
	if (cl.debug) {
		printf("| %s %s\n\n",
		    info->filter_expr, info->extended ? "|" : "[basic]");
	}

	if (info->extended) {
		do_ext_search(*init_ctx, cname, *info, ret_attr_ids);
	} else {
		do_basic_search(*init_ctx, cname, *info, ret_attr_ids);
	}

	return (0);
}


static void
usage(const char *msg)
{
	if (msg != NULL) {
		fprintf(stderr, "%s: %s\n", cl.command, msg);
	}
	fprintf(stderr, "%s:\t%s %s %s \\\n\t%s\n\t%s\n\t%s\n\t%s%s\n",
	    gettext("Usage"),
	    cl.command,
	    gettext("[-A] [-L] [-l] [-v] [-n max] [-s scope]"),
	    gettext("composite_name"),
	    gettext("    [-a attribute]... filter_expr [filter_arg]..."),
	    gettext("attribute   ::=  [-O | -U] string"),
	    gettext("filter_arg  ::=  [-O | -U] string"),
	    gettext("scope       ::="),
	    gettext("  object | context | subtree | constrained_subtree"));

	exit(-1);
}


static void
process_cmdline(int argc, char *argv[])
{
	cl.command = strrchr(argv[0], '/');
	if (cl.command == NULL) {
		cl.command = argv[0];
	} else {
		cl.command++;
	}

	cl.scope = FN_SEARCH_ONE_CONTEXT;

	int c;
	while ((c = getopt(argc, argv, "AdLlvn:s:")) != EOF) {
		switch (c) {
		case 'A':
			cl.authoritative = TRUE;
			break;
		case 'd':
			cl.debug = TRUE;
			break;
		case 'L':
			cl.follow_links = TRUE;
			break;
		case 'l':
			cl.return_refs = TRUE;
			break;
		case 'v':
			cl.return_refs = TRUE;
			cl.output_detail = 2;
			break;
		case 'n':
			char *term;
			cl.max = (unsigned int)strtol(optarg, &term, 10);
			if (*term != '\0') {
				usage();
			}
			// "-n 0" converts a basic search into an extended one.
			cl.force_extended = TRUE;
			break;
		case 's': {
			int matches = 0;
			for (int i = 0; i < SCOPES_SZ; i++) {
				size_t len = strlen(optarg);
				const char *sname = scopes[i].name;
				if (len <= strlen(sname) &&
				    strncmp(optarg, sname, len) == 0) {
					matches++;
					cl.scope = scopes[i].scope;
				}
			}
			if (matches != 1) {
				usage();
			}
			break;
		}
		default:
			usage();
		}
	}
	if (optind == argc) {
		usage();
	}
	cl.cname = argv[optind++];

	process_cmdline_attrs(argc, argv);

	if (optind == argc) {
		usage();
	}
	cl.filter_expr = argv[optind++];

	process_cmdline_filter_args(argc, argv);
}


static void
process_cmdline_attrs(int argc, char *argv[])
{
	cl.attrs = new ident_t[(argc - optind) / 2];
	mem_check(cl.attrs);
	int n = 0;

	bool_t expecting = FALSE;	// TRUE if we're expecting -O or -U
	int c;
	while ((c = getopt(argc, argv, "a:O:U:")) != EOF) {
		switch (c) {
		case 'a':
			if (strcmp(optarg, "-O") == 0 ||
			    strcmp(optarg, "-U") == 0) {
				expecting = TRUE;
				optind--;	// reread optarg as an option
			} else {
				expecting = FALSE;
				cl.attrs[n].format = FN_ID_STRING;
				cl.attrs[n].value = optarg;
				n++;
			}
			break;
		case 'O':
		case 'U':
			if (!expecting) {
				usage();
			}
			expecting = FALSE;
			cl.attrs[n].format =
			    c == 'O' ? FN_ID_ISO_OID_STRING : FN_ID_DCE_UUID;
			cl.attrs[n].value = optarg;
			n++;
			break;
		default:
			usage();
		}
	}
	cl.num_attrs = n;
}


static void
process_cmdline_filter_args(int argc, char *argv[])
{
	cl.filter_args = new ident_t[argc - optind];
	mem_check(cl.filter_args);
	int n = 0;

	while (optind < argc) {
		int c;
		while ((c = getopt(argc, argv, "O:U:")) != EOF) {
			switch (c) {
			case 'O':
			case 'U':
				cl.filter_args[n].format =
				    c == 'O'
				    ? FN_ID_ISO_OID_STRING : FN_ID_DCE_UUID;
				cl.filter_args[n].value = optarg;
				n++;
				break;
			default:
				usage();
			}
		}
		if (optind == argc) {
			break;
		}
		cl.filter_args[n].format = FN_ID_STRING;
		cl.filter_args[n].value = argv[optind++];
		n++;
	}
	cl.num_filter_args = n;
}


static FN_attrset *
attrs_to_return()
{
	if (cl.num_attrs == 0) {
		return (NULL);	// request all attributes
	}
	FN_attrset *attr_ids = new FN_attrset();
	mem_check(attr_ids);
	for (int i = 0; i < cl.num_attrs; i++) {
		if (strlen(cl.attrs[i].value) == 0) {
			continue;
		}
		FN_attribute attr(
		    FN_identifier((unsigned char *)cl.attrs[i].value),
		    syntax_ascii);
		if (attr_ids->add(attr, FN_OP_SUPERCEDE) == 0) {
			mem_check(NULL);	// print error message
		}
	}
	return (attr_ids);
}


// Characters with special meaning within a filter expression.
static const char *specials = "=!<>~*'()%";


static bool_t
at_attr(const char *fe, int &attrlen)
{
	if (at_integer(fe, attrlen)) {
		return (FALSE);
	}
	int i;
	for (i = 0; fe[i] != '\0'; i++) {
		if (isspace(fe[i]) || strchr(specials, fe[i]) != NULL) {
			break;
		}
	}
	if (i == 0 ||
	    i == 2 && strncmp(fe, "or", 2) == 0 ||
	    i == 3 && strncmp(fe, "and", 3) == 0 ||
	    i == 3 && strncmp(fe, "not", 3) == 0) {
		return (FALSE);
	}
	attrlen = i;
	return (TRUE);
}


static bool_t
at_keyword(const char *fe, int &wordlen)
{
	int i;
	for (i = 0; fe[i] != '\0'; i++) {
		if (isspace(fe[i]) || strchr(specials, fe[i]) != NULL) {
			break;
		}
	}
	if (i == 2 && strncmp(fe, "or", 2) == 0 ||
	    i == 3 && strncmp(fe, "and", 3) == 0 ||
	    i == 3 && strncmp(fe, "not", 3) == 0) {
		wordlen = i;
		return (TRUE);
	}
	return (FALSE);
}


static bool_t
at_integer(const char *fe, int &wordlen)
{
	wordlen = 0;
	if (fe[wordlen] == '-') {
		wordlen++;
	}
	while (isdigit(fe[wordlen])) {
		wordlen++;
	}
	return (wordlen > 1 || isdigit(fe[0]));
}


static void
do_basic_search(FN_ctx &ctx,
    const FN_composite_name &cname,
    const FilterInfo &info,
    const FN_attrset *ret_attr_ids)
{
	FN_attrset *match_attrs = info.new_search_attrs();

	// Perform the search.

	FN_status status;
	FN_searchlist *sl = ctx.attr_search(cname, match_attrs, cl.return_refs,
	    ret_attr_ids, status);
	if (sl == NULL) {
		if (status.is_success()) {
			return;	// successful search, but nothing found
		} else {
			error(&status, gettext("Search failed"));
		}
	}

	// Print the results.

	bool_t return_attrs =	// might any attrs be returned?
	    ret_attr_ids == NULL ||
	    ret_attr_ids->count() > 0;

	FN_ref *ref = NULL;
	FN_attrset *attrs = NULL;
	FN_ref **refp = cl.return_refs ? &ref : NULL;
	FN_attrset **attrsp = return_attrs ? &attrs : NULL;
	FN_string *name;

	while ((name = sl->next(refp, attrsp, status)) != NULL) {
		print_match(*name, ref, attrs, TRUE, status);
		delete name;
		delete ref;
		delete attrs;
	}
	if (!status.is_success()) {
		error(&status, gettext("Error encountered during search"));
	}
}


static void
do_ext_search(FN_ctx &ctx,
    const FN_composite_name &cname,
    const FilterInfo &info,
    const FN_attrset *ret_attr_ids)
{
	FN_search_control *control = info.new_control(ret_attr_ids);
	FN_search_filter *filter = info.new_filter();

	// Perform the search.

	FN_status status;
	FN_ext_searchlist *sl =
	    ctx.attr_ext_search(cname, control, filter, status);
	if (sl == NULL) {
		if (status.is_success()) {
			return;	// successful search, but nothing found
		} else {
			error(&status, gettext("Search failed"));
		}
	}

	// Print the results.

	bool_t return_attrs =	// might any attrs be returned?
	    ret_attr_ids == NULL ||
	    ret_attr_ids->count() > 0;

	FN_ref *ref = NULL;
	FN_attrset *attrs = NULL;
	FN_ref **refp = cl.return_refs ? &ref : NULL;
	FN_attrset **attrsp = return_attrs ? &attrs : NULL;
	unsigned int relative;
	FN_composite_name *name;

	while ((name = sl->next(refp, attrsp, relative, status)) != NULL) {
		FN_string *name_string = name->string();
		mem_check(name_string);
		print_match(*name_string, ref, attrs, relative, status);
		delete name;
		delete name_string;
		delete ref;
		delete attrs;
	}
	if (!status.is_success()) {
		error(&status, gettext("Error encountered during search"));
	}
}


static void
print_match(const FN_string &name,
    const FN_ref *ref,
    const FN_attrset *attrs,
    bool_t relative,
    const FN_status &status)
{
	const unsigned char *name_str = name.str();
	mem_check(name_str);
	if (ref == NULL && attrs == NULL) {
		printf("%s\n", (const char *)name_str);
		return;
	}

	static bool_t first_match = TRUE;
	if (first_match) {
		first_match = FALSE;
	} else {
		printf("\n");
	}

	// Print name.
	printf("%s: %s\n",
	    relative ? gettext("Name") : gettext("Full name"), name_str);

	// Print reference.
	if (ref != NULL) {
		FN_string *desc = ref->description(cl.output_detail);
		if (desc) {
			const char *desc_str = (char *)desc->str();
			mem_check(desc_str);
			printf("%s", desc_str);
			delete desc;
		}
	}

	// Print attributes.
	if (attrs != NULL) {
		print_attrset(attrs);
	}
	printf("\n");

	// Print status on error.
	if (!status.is_success()) {
		fprintf(stderr, "%s: ", gettext("Error"));
		print_status(status);
		fprintf(stderr, "\n");
	}
}


static void
mem_check(const void *ptr)
{
	if (ptr == NULL) {
		error(NULL, gettext("Memory allocation failure"));
	}
}


static void
error(const FN_status *status, const char *fmt, ...)
{
	va_list args;

	fprintf(stderr, "%s: ", cl.command);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	int statcode = -1;
	if (status != NULL) {
		statcode = (int)status->code();
		fprintf(stderr, ": ");
		print_status(*status);
	}
	fprintf(stderr, "\n");
	exit(statcode);
}


static void
print_status(const FN_status &status)
{
	const char *desc = (const char *)(status.description()->str());
	if (desc == NULL) {
		fprintf(stderr, "%s = %u\n",
		    gettext("status code"), status.code());
	} else {
		fprintf(stderr, "%s\n", desc);
	}
}


// Implementation of FilterInfo class.

FilterInfo::FilterInfo()
{
	// An upper bound on the amount the filter expression
	// might expand through substitution is a factor of two.

	size_t len = strlen(cl.filter_expr);
	filter_expr = new char[len * 2 + 1];
	mem_check(filter_expr);

	// An upper bound on the number of filter args is one for
	// every other character in the filter expression.

	size_t maxargs = (len + 1) / 2;
	types = new FN_search_filter_type[maxargs];
	mem_check(types);

	args = new void*[maxargs];
	mem_check(args);

	numargs = 0;	// number of API-style args constructed so far
	args_used = 0;	// number of command line args consumed so far

	extended = cl.max > 0 || cl.follow_links || cl.force_extended ||
	    cl.scope != FN_SEARCH_ONE_CONTEXT;
}


void
FilterInfo::analyze_expr()
{
	char *fe = cl.filter_expr;	// command line filter expression
	char *fe2 = filter_expr;	// API filter expression

	// Walk through filter expression, looking for '%' and for
	// expansions to perform.  Also check for constructs that
	// require the use of extended search.  Parsing the expression
	// would be more straightforward than the lexical approach
	// used here, but would require a lot more code.

	int i = 0;	// index into command line filter expr "fe"
	int j = 0;	// index into API-style filter expr "fe2"
	char ext = '-';	// first letter in name of most recent extended op
	int wordlen;
	bool_t in_literal = FALSE;
	bool_t in_ext = FALSE;	// TRUE while in an extended op like 'name'()
	bool_t maybe_in_ext = FALSE;
	bool_t just_seen_attr = FALSE;
	int literal_start = -1;	// starting index of most recently seen literal

	while (fe[i] != '\0') {
		if (isspace(fe[i])) {
			goto next;
		}
		if (in_literal) {
			in_literal = (fe[i] != '\'');
			if (in_literal || extended) {
				goto next;
			}
			// Convert literal to "%v" if not part of a
			// wildcarded string or extended op.
			for (int k = i + 1; isspace(fe[k]); k++)
				;
			if (fe[k] == '*' || fe[k] == '(') {
				extended = TRUE;
				goto next;
			}
			i++;
			append_arg(FN_SEARCH_FILTER_ATTRVALUE, FN_ID_STRING,
			    fe2 + literal_start + 1, j - literal_start - 1);
			j = literal_start;	// back out literal
			fe2[j++] = '%';
			fe2[j++] = 'v';
			continue;
		}
		if (fe[i] == '*') {
			extended = TRUE;
		}
		if (just_seen_attr) {
			just_seen_attr = FALSE;
			extended |= (strchr("!<>~", fe[i]) != NULL);
			// Catch a common error:  "=" instead of "==".
			if (fe[i] == '=' && fe[i + 1] != '=') {
				error(NULL, gettext("Unknown operator"));
			}
		}
		if (maybe_in_ext) {
			maybe_in_ext = FALSE;
			in_ext = (fe[i] == '(');
			if (in_ext) {
				extended = TRUE;
				goto next;
			}
		}
		if (fe[i] == '%') {
			fe2[j++] = fe[i++];
			append_arg(fe[i]);
			just_seen_attr = (fe[i] == 'a');
			goto next;
		}
		if (in_ext) {
			if (fe[i] == '\'' && (ext == 'r' || ext == 'a')) {
				int len = 0;
				i++;
				char *start = fe + i;
				while (fe[i++] != '\'') {
					len++;
				}
				append_arg(FN_SEARCH_FILTER_IDENTIFIER,
				    FN_ID_STRING, start, len);
				fe2[j++] = '%';
				fe2[j++] = 'i';
			}
			in_ext = (fe[i] != ')');
			goto next;
		}
		if (strncmp(fe + i, "'name'", 6) == 0 ||
		    strncmp(fe + i, "'reftype'", 9) == 0 ||
		    strncmp(fe + i, "'addrtype'", 10) == 0) {
			maybe_in_ext = TRUE;
			ext = fe[i + 1];
		}
		if (fe[i] == '\'') {
			in_literal = TRUE;
			literal_start = j;
			goto next;
		}
		if (at_keyword(fe + i, wordlen)) {
			extended |= (fe[i] != 'a');	// 'a' is for "and"
			while (wordlen--) {
				fe2[j++] = fe[i++];
			}
			continue;
		}
		if (at_attr(fe + i, wordlen)) {
			just_seen_attr = TRUE;
			append_arg(FN_SEARCH_FILTER_ATTR, FN_ID_STRING,
			    fe + i, wordlen);
			i += wordlen;
			fe2[j++] = '%';
			fe2[j++] = 'a';
			continue;
		}
		if (!extended && at_integer(fe + i, wordlen)) {
			append_arg(FN_SEARCH_FILTER_ATTRVALUE, FN_ID_STRING,
			    fe + i, wordlen);
			i += wordlen;
			fe2[j++] = '%';
			fe2[j++] = 'v';
			continue;
		}
	next:
		fe2[j++] = fe[i++];
	}
	fe2[j++] = '\0';
	assert_all_args_used();
}


void
FilterInfo::append_arg(char token)
{
	if (args_used == cl.num_filter_args) {
		error(NULL, gettext("Too few filter arguments"));
	}

	FN_search_filter_type type;

	switch (token) {
	case 'a':
		type = FN_SEARCH_FILTER_ATTR;
		break;
	case 'i':
		type = FN_SEARCH_FILTER_IDENTIFIER;
		break;
	case 's':
		type = FN_SEARCH_FILTER_STRING;
		break;
	case 'v':
		type = FN_SEARCH_FILTER_ATTRVALUE;
		break;
	default:
		error(NULL, "%s: %%" "%c",
		    gettext("Invalid substitution token"), token);
	}
	append_arg(type, cl.filter_args[args_used].format,
	    cl.filter_args[args_used].value,
	    strlen(cl.filter_args[args_used].value));
	args_used++;
}


void
FilterInfo::append_arg(FN_search_filter_type type,
    unsigned int format,
    const char *value,
    size_t valuelen)
{
	const char *typedesc;
	void *arg;
	switch (type) {
	case FN_SEARCH_FILTER_ATTR:
		typedesc = "attr";
		arg = new FN_attribute(
		    FN_identifier(format, valuelen, value), syntax_ascii);
		break;
	case FN_SEARCH_FILTER_IDENTIFIER:
		typedesc = "ident";
		arg = new FN_identifier(format, valuelen, value);
		break;
	case FN_SEARCH_FILTER_STRING:
		typedesc = "string";
		arg = new FN_string((unsigned char *)value, valuelen);
		break;
	case FN_SEARCH_FILTER_ATTRVALUE:
		typedesc = "value";
		arg = new FN_attrvalue((unsigned char *)value, valuelen);
		break;
	}
	mem_check(arg);
	types[numargs] = type;
	args[numargs] = arg;
	numargs++;
	if (cl.debug) {
		printf("| %s ", typedesc);
		switch (format) {
		case FN_ID_ISO_OID_STRING:
			printf("-O ");
			break;
		case FN_ID_DCE_UUID:
			printf("-U ");
			break;
		}
		while (valuelen-- > 0) {
			printf("%c", *value++);
		}
		printf("\n");
	}
}


void
FilterInfo::assert_all_args_used() const
{
	if (args_used < cl.num_filter_args) {
		error(NULL, gettext("Too many filter arguments"));
	}
}


FN_search_control *
FilterInfo::new_control(const FN_attrset *ret_attr_ids) const
{
	unsigned int statcode;
	FN_search_control *control = new FN_search_control(
	    cl.scope, cl.follow_links, cl.max, cl.return_refs, ret_attr_ids,
	    statcode);
	if (control == NULL) {
		error(NULL, gettext("Could not construct search control"));
	}
	return (control);
}


FN_search_filter *
FilterInfo::new_filter() const
{
	unsigned int statcode;
	FN_search_filter *filter = new FN_search_filter(
	    statcode, (const unsigned char *)filter_expr, types, args);
	if (filter == NULL) {
		error(NULL, gettext("Could not construct search filter"));
	}
	return (filter);
}


FN_attrset *
FilterInfo::new_search_attrs() const
{
	if (numargs == 0) {
		return (NULL);	// return all names in context
	}
	FN_attrset *attrs = new FN_attrset;
	mem_check(attrs);

	// A filter expression meeting the requirements for a basic
	// search is simply a conjunction of "%a", "%a==%v", and "%a==%s"
	// subexpressions.  Any parentheses in the expression are
	// irrelevant, since conjunction is associative.  Therefore:
	//
	// The args/types arrays represents a list of attributes
	// (with type FN_SEARCH_FILTER_ATTR).  Each attribute may
	// optionally be followed by an attrvalue (type
	// FN_SEARCH_FILTER_ATTRVALUE) or by a string (type
	// FN_SEARCH_FILTER_STRING), which is the value to match for
	// that attribute.  If an attribute is followed by neither,
	// then test for the presence of that attribute.
	//
	// If the same attribute appears in the list more than once,
	// the associated values are joined together in the attrset
	// returned.

	for (int i = 0; i < numargs; i++) {
		if (types[i] != FN_SEARCH_FILTER_ATTR) {
			continue;
		}
		FN_attribute attr(*(FN_attribute *)args[i]);

		// Collect values from all attributes that are duplicates of i.
		for (int j = i; j < numargs - 1; j++) {
			if (!same_attr(i, j)) {
				continue;
			}
			switch (types[j + 1]) {
			case FN_SEARCH_FILTER_ATTRVALUE:
				attr.add(*(FN_attrvalue *)args[++j]);
				break;
			case FN_SEARCH_FILTER_STRING:
				attr.add(*(FN_string *)args[++j]);
				break;
			case FN_SEARCH_FILTER_ATTR:
				break;
			default:
				error(NULL, "Invalid filter expression");
			}
		}
		attrs->add(attr, FN_OP_EXCLUSIVE);
	}
	return (attrs);
}


bool_t
FilterInfo::same_attr(int i, int j) const
{
	if (types[i] != FN_SEARCH_FILTER_ATTR ||
	    types[j] != FN_SEARCH_FILTER_ATTR) {
		return (FALSE);
	}
	if (i == j) {
		return (TRUE);
	}
	const FN_identifier *id_i = ((FN_attribute *)args[i])->identifier();
	const FN_identifier *id_j = ((FN_attribute *)args[j])->identifier();
	return (*id_i == *id_j);
}
