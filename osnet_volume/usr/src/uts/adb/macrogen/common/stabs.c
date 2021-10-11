/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)stabs.c	1.6	99/06/11 SMI"


/*
 * stabs.c - code for analysing the stabs information
 */
#include <math.h>
#include "stabs.h"
#include "gen.h"

int	found_match;
int	_sizeof_long, _sizeof_int, _sizeof_ptr;
void	set_model_defaults();

#define	MAX_INLINE_SIZE			128
#define	BITS_PER_BYTE			8

#define	INLINE_BEGIN_KWD		"{adb_inline_begin}\n"
#define	INLINE_END_KWD			"{adb_inline_end}"
#define	END_KWD_LEN			16

int line;	/* line number in .dbg file */

/* Linked list of name transformations we are to do, from "rename" commands. */
typedef struct rename_ {
	char *from;
	char *to;
	struct rename_ *next;
} rename_t;
rename_t *renames;

/* Linked list of type-format associations from "format" commands. */
typedef struct format_ {
	char *type;
	char *format;
	struct format_ *next;
} format_t;
format_t *formats;

/*
 * alloc_err_handler() gets installed to handle malloc/etc. failures.
 */
int
alloc_err_handler(size_t size)
{
	fprintf(stderr, "unable to allocate %d bytes of memory; giving up\n",
		size);
	exit(-1);
}

main(int argc, char **argv)
{
	(void) set_alloc_err_func(alloc_err_handler);
	parse_input();
	get_dbgs(argc, argv);
	exit(0);
}

/*
 * This routine will read the .dbg files and build a list of the structures
 * and fields that user is interested in. Any struct specified will get all
 * its fields included. If nested struct needs to be printed - then the
 * field name and name of struct type needs to be included in the next line.
 */
get_dbgs(int argc, char **argv)
{
	FILE *fp;

	for (argc--, argv++; argc != 0; argc--, argv++) {
		if ((fp = fopen(*argv, "r")) == NULL)
			fprintf(stderr, "Cannot open %s\n", *argv);
		/* add all types in this file to our table */
		parse_dbg(fp);
	}
}


/*
 * namex() extracts a name from the specified string and dup's it.
 * A pointer to the dup'ed name is stored at the specified address.
 * A pointer to the character following the name in the input string
 * is the function's return value.
 * A name is any sequence of alphanumeric/punctuation characters,
 * the first of which cannot be a numeral.
 * If there are no names left in the string, a NULL is returned
 * for both the name and the new pointer.
 */
char *
namex(char *cp, char **w)
{
	char *new, *orig, c;
	int len;

	if (!cp)
		return (NULL);

	/* Skip over whitespace; get next char in c, pointer to it in orig. */
	c = *cp++;
	while (isspace(c))
		c = *cp++;
	orig = cp - 1;

	/* If we're at then end of the input string, return NULLs. */
	if (c == '\0') {
		*w = NULL;
		return (NULL);
	}

	/* If the char is a quote, just look for a close quote. */
	if (c == '"') {

		/* Skip over the open quote. */
		orig++;

		/* Find the close quote. */
		for (c = *cp++; c != '"'; c = *cp++) {

			if (c == '\0') {
				fprintf(stderr,
					"Line %d has unmatched quote\n", line);
				return (NULL);
			}
		}
		len = cp - orig;

		/* Allocate space for a new string and copy it in. */
		*w = new = (char *)malloc(len);
		while (orig < cp - 1)
			*new++ = *orig++;
		*new = '\0';

		return (cp);
	}


	/* Just take all characters up to whitespace.  First find the end. */
	while (c && !isspace(c))
		c = *cp++;
	len = cp - orig;

	/* Allocate space for a new string and copy it in. */
	*w = new = (char *)malloc(len);
	while (orig < cp - 1)
		*new++ = *orig++;
	*new = '\0';

	return (cp-1);
}

/*
 * renamed() looks up a name in the renames list, and returns the name
 * it is to be renamed to.  If the name is not found in the renames list,
 * the specified name is returned unchanged.
 */
char *
renamed(char *from)
{
	rename_t *rename = renames;
	while (rename) {
		if (strcmp(rename->from, from) == 0)
			return (rename->to);
		rename = rename->next;
	}
	return (from);
}

/*
 * get_format() looks up a struct name in the formats list, and returns
 * the corresponding format, or NULL if there is none.
 */
static char *
get_format(char *name)
{
	format_t *format = formats;
	while (format) {
		if (strcmp(format->type, name) == 0)
			return (format->format);
		format = format->next;
	}
	return (NULL);
}

struct mlist *
find_member(struct tdesc *tdp, char *name)
{
	struct mlist *mlp;

	while (tdp->type == TYPEOF)
		tdp = tdp->data.tdesc;
	if (tdp->type != STRUCT && tdp->type != UNION)
		return (NULL);
	for (mlp = tdp->data.members; mlp != NULL; mlp = mlp->next)
		if (strcmp(mlp->name, name) == 0)
			return (mlp);
	return (NULL);
}

/*
 * add this struct to our table of structs/fields that the user has
 * requested in the .dbg files
 */
struct node *
getnode(char *cp)
{
	struct node *np;
	char *name;

	/* Get the struct name. */
	cp = namex(cp, &name);
	if (!name)
		return (NULL);

	/* Allocate a node and add the name. */
	np = ALLOC(struct node);
	np->struct_name = name;

	/* Get optional prefix; empty string means "none" (placeholder). */
	cp = namex(cp, &np->prefix);
	if (np->prefix && !*np->prefix) {
		free(np->prefix);
		np->prefix = 0;
	}

	/* Get optional name to use; default to struct name. */
	if (cp)
		cp = namex(cp, &np->name);
	else
		np->name = NULL;
	if (!np->name)
		np->name = strdup(np->struct_name);

	/* Initialize list of children. */
	np->child = NULL;
	np->last_child_link = &np->child;

	return (np);
}

/*
 * add this field to our table of structs/fields that the user has
 * requested in the .dbg files
 */

/* 0 = empty line, 1 = nonempty (check for more children) */
int
addchild(char *cp, struct node *np, FILE *sp)
{
	struct child *chp;
	char *w;
	pat_handle_t pattern;
	static char	tmpbuf[MAXLINE];
	int		in_line_code_size = 1; /* for terminating '\0' */
	int		org_line, cur_max;
	char		*p;

	/*
	 * Check if this is in-line adb code and not some member
	 * description. For now, we do an exact match for the
	 * keyword INLINE_BEGIN_KWD at the beginning of a line.
	 * Should be changed (eventually) to accept white spaces
	 * before and after the keyword.
	 */
	org_line = line;
	if (strcmp(cp, INLINE_BEGIN_KWD) == 0) {

	    /* Allocate a new child struct and add to end of node's list */
	    *np->last_child_link = chp = ALLOC(struct child);
	    np->last_child_link = &chp->next;
	    chp->next = NULL;

	    /* Special case: set pattern to NULL, format to "inline" */
	    chp->pattern = NULL;
	    chp->format = strdup("inline");

	    /* Make label contain inline-code, newlines and all */
	    cur_max = MAX_INLINE_SIZE;
	    if ((chp->label = (char *) malloc(MAX_INLINE_SIZE)) == NULL)
		return (0);

	    do {
		/*
		 * Copy lines from input into chp->label until we see
		 * INLINE_END_KWD
		 */
		line++;
		cp = fgets(tmpbuf, MAXLINE, sp);
		if (!cp) {
		    fprintf(stderr,
			"In-line code from line %d incomplete\n", org_line+1);
		    return (0);
		}

		if (strncmp(cp, INLINE_END_KWD, END_KWD_LEN) == 0)
		    break;

		in_line_code_size += strlen(cp);
		if (in_line_code_size > cur_max) {
		    cur_max = 2 * cur_max;
		    if ((chp->label = realloc(chp->label, cur_max)) == NULL)
			return (0);
		}
		strcat(chp->label, cp);
	    } while (cp);

	    return (1);
	}

	/* Get the pattern for members to which this node applies. */
	cp = namex(cp, &w);
	if (!w)
	    return (0);

	if (0 != pat_compile(w, NULL, &pattern)) {
	    fprintf(stderr, "Bad pattern at line %d: %s\n", line, w);
	    return (1);
	}

	/* Allocate a new child struct and add to end of node's list. */
	*np->last_child_link = chp = ALLOC(struct child);
	np->last_child_link = &chp->next;
	chp->next = NULL;
	chp->pattern = pattern;

	/* Get the format and label (both are optional). */
	cp = namex(cp, &chp->format);
	if (chp->format && !*chp->format) {
	    free(chp->format);
	    chp->format = NULL;
	}

	/* Get optional label. */
	if (cp)
	    namex(cp, &chp->label);
	else
	    chp->label = NULL;

	return (1);
}

/*
 * parse_dbg() parses a .dbg file, generating code for each struct and its
 * children.
 *
 * Entries in .dbg files should be renames or struct specifications:
 *
 * rename OldStructName NewStructName
 * format StructName Format
 *
 * StructName [ MemberPrefixToRemove [ NameToUse ] ]
 *	MemberPattern	[ Format [ Label ] ]
 *	...
 *
 * For example,
 * rename foodlebar foo
 * format kcondvar x
 * foodlebar foo_
 *	bar		d
 *	^{zot,blit}	x
 *
 * means that for struct foodlebar, member foo_bar should be displayed in format
 * d (presumably decimal), and that all other fields except foo_zot and foo_blit
 * should be displayed in format x.  The names used to label the output should
 * not contain the foo_ prefix.  The macro should be called foo, not foodlebar.
 * Any members of tag/type kcondvar should be displayed in hex.
 */
parse_dbg(FILE *sp)
{
	char *cp;
	struct tdesc *tp;
	struct node *np;
	static char linebuf[MAXLINE];
	int copy_flag = 0;
	char rename_word[] = "rename ";
	char format_word[] = "format ";

	/* grab each line and add them to our table */
	for (line = 1; cp = fgets(linebuf, MAXLINE, sp); line++) {

		if (*cp == '\n')
			continue;
		if (*cp == '\\')
			continue;

		/*
		 * For the syntax "rename From To", allocate a rename_t
		 * to hold the info.  We will use this to change certain
		 * names we send to the generator.
		 */
		if (strncmp(cp, rename_word, sizeof (rename_word) - 1) == 0) {
		    rename_t *rename = ALLOC(rename_t);

		    cp += sizeof (rename_word) - 1;
		    if ((cp = namex(cp, &rename->from)) &&
				    (cp = namex(cp, &rename->to))) {
		    tp = lookupname(rename->to);
		    if (tp != NULL) {
			fprintf(stderr,
			"line %d: rename clobbers existing defn [%s]\n",
							line, rename->to);
		    }
		    rename->next = renames;
		    renames = rename;
		    } else {
			fprintf(stderr,
			"line %d: Syntax error in \"rename\" command\n", line);
			free(rename);
		    }
		    continue;
		}

		/*
		 * For the syntax "format Type Format", allocate a format_t
		 * to hold the info.  We will use this to change certain
		 * default formats we send to the generator.
		 */
		if (strncmp(cp, format_word, sizeof (format_word) - 1) == 0) {
			format_t *format = ALLOC(format_t);
			cp += sizeof (format_word) - 1;
			if ((cp = namex(cp, &format->type)) &&
			    (cp = namex(cp, &format->format))) {
				format->next = formats;
				formats = format;
			} else {
			    fprintf(stderr,
			    "Syntax error in \"format\" command (line %d)\n",
								line);
			    free(format);
			}
			continue;
		}

		/* Not a keyword; must be a struct line. */
		np = getnode(cp);
		if (np) {
			do {
				line++;
				cp = fgets(linebuf, MAXLINE, sp);
			} while (cp && addchild(cp, np, sp));
			printnode(np);
		}
	}
}

printnode(struct node *np)
{
	struct tdesc *tdp;

	set_model_defaults();

	tdp = lookupname(np->struct_name);
	if (tdp == NULL) {

		if (tdp == NULL) {
			fprintf(stderr,
			    "macrogen: warning: Can't find struct name: %s\n",
			    np->struct_name);
			exit(1);
		}

	}
again:
	switch (tdp->type) {
	    case UNION:	/* fall through */
	    case STRUCT:
		do_sou(tdp, np);
		break;
	    case TYPEOF:
		tdp = tdp->data.tdesc;
		goto again;
	    default:
		fprintf(stderr, "macrogen: warning: %s isn't a struct\n",
			np->struct_name);
		exit(1);
	}
}

/*
 * get_label() chooses the label for a struct member based on the member name,
 * whether we were supposed to delete a prefix, and whether a label for this
 * field was explicitly specified.
 */
static char *
get_label(char *name, char *prefix)
{
	if (prefix) {
		int prefix_len = strlen(prefix);
		if (0 == memcmp(name, prefix, prefix_len))
			name += prefix_len;
	}

	return (name);
}

do_sou(struct tdesc *tdp, struct node *np)
{
	struct child	*chp;
	char		*prefix = np->prefix;
	struct mlist	*mlp;
	char		*format;
	char		*label;
	void		do_complete_match();
	char		*get_pat_name();

	gen_struct_begin(renamed(np->struct_name),
					np->prefix, np->name, tdp->size);
	if (tdp->type == UNION)
	    printf("\t*** warning: %s is a union\n", np->struct_name);

	/*
	 * Process members that should be displayed.
	 *
	 * for each member of struct
	 *	if no members were specified
	 *	    process member
	 *	else
	 *	    for each member specification (in user-specified order)
	 *		if member matches pattern
	 *		    process member
	 *		    break out of inner loop
	 */

	/*
	 * Do it the reverse way! Otherwise, we'll end up printing the members
	 * of the structure in the same order as they appear in the structure
	 * EVEN if the user specifies a different order!!
	 */

	/* If no children were specified, process all struct elements */
	chp = np->child;
	if (!chp) {
	    for (mlp = tdp->data.members; mlp != NULL; mlp = mlp->next) {
		switch_on_type(mlp, mlp->fdesc, NULL,
				get_label(mlp->name, np->prefix), 0);
	    }
	    gen_struct_end();
	    return;
	}

	/* Otherwise, if there are some children, process them one by one */
	for (chp = np->child; chp != NULL; chp = chp->next) {

	    /* If this child contains inline code, add it as dummy member */
	    if (chp->format) {
		if (strcmp(chp->format, "inline") == 0) {
		    gen_add_struct_member(chp->label, 0, 0, 0, "inline");
		    free(chp->format);
		    chp->format = strdup("inline-over");
		    continue;		/* work on the next member */
		} else if (strcmp(chp->format, "inline-over") == 0)
		    continue;		/* already processed, skip it */
	    }

	    /* Search the whole list to check for multiple matches */
	    found_match = 0;
	    for (mlp = tdp->data.members; mlp != NULL; mlp = mlp->next)
		do_complete_match(tdp, chp, mlp, np);

	    /* If no matches for this child pattern, print a warning */
	    if (!found_match) {
		fprintf(stderr,
		    "warning: no match for %s in struct %s (macro %s)\n",
		    get_pat_name(chp->pattern), np->struct_name, np->name);
	    }
	}

	gen_struct_end();
}

static void
call_gen_struct_member(char *label, struct mlist *mlp,
				dimension_t *dimension, char *format)
{
	int	size = mlp->size;

	/* If this is an array, get the size of an individual element. */
	if (dimension) {

		dimension_t *dim  = dimension;

		do {
			size /= dim->num_elements;
			dim = dim->next;
		} while (dim != dimension);
	}

	/* If we still don't have a format, default to hex. */
	if (format == NULL)
		format = "hex";

	gen_struct_member(label, mlp->offset, dimension, size, format);
}

do_enum(struct tdesc *tdp, struct node *np)
{
	/* We don't do anything with enums. */
}

print_forward(struct mlist *mlp, struct tdesc *tdp, char *format,
				char *label, dimension_t *dimension)
{
	fprintf(stderr, "%s never defined\n", mlp->name);
}

print_typeof(struct mlist *mlp, struct tdesc *tdp, char *format,
				char *label, dimension_t *dimension)
{
	char format_buf[128];

	/*
	 * If no format was specified, and this is a struct, see if there is
	 * a format specified for it.  If not, just use .StructName as the
	 * format.  Make sure there _is_ a type name.
	 */
	if (!format && tdp->data.tdesc->type == STRUCT &&
	    tdp->name && *tdp->name) {

		format = get_format(tdp->name);
		if (!format) {
			*format_buf = '.';
			strcpy(format_buf+1, renamed(tdp->name));
			format = format_buf;
		}
	}
	switch_on_type(mlp, tdp->data.tdesc, format, label, dimension);
}

print_const(struct mlist *mlp, struct tdesc *tdp, char *format,
					char *label, dimension_t *dimension)
{
	switch_on_type(mlp, tdp->data.tdesc, format, label, dimension);
}

print_volatile(struct mlist *mlp, struct tdesc *tdp, char *format,
					char *label, dimension_t *dimension)
{
	switch_on_type(mlp, tdp->data.tdesc, format, label, dimension);
}

print_intrinsic(struct mlist *mlp, struct tdesc *tdp, char *format,
					char *label, dimension_t *dimension)
{
	call_gen_struct_member(label, mlp, dimension, format);
}

/* is_char() returns TRUE if the specified descriptor represents a char. */
static int
is_char(struct tdesc *tdp)
{
	while (tdp->type == TYPEOF)
		tdp = tdp->data.tdesc;
	return ((tdp->type == INTRINSIC) && (0 == strcmp(tdp->name, "char")));
}

/* is_funcptr() returns TRUE if the specified descriptor represents a char. */
static int
is_funcptr(struct tdesc *tdp)
{
	while (tdp->type == TYPEOF) {
		/* printf("tdp->name = %s:%d\n", tdp->name, tdp->type ); */
		tdp = tdp->data.tdesc;
	}
	return ((tdp->type == FUNCTION) &&
				(0 == strcmp(tdp->name, "function")));
}

print_pointer(struct mlist *mlp, struct tdesc *tdp, char *format,
					char *label, dimension_t *dimension)
{
	/* As a special case, default format for type char * is "string". */
	if (!format && (0 == strcmp(tdp->name, "caddr_t")))
		format = "hex";
	if (!format && is_char(tdp->data.tdesc))
		format = "string";
	if (!format && is_funcptr(tdp->data.tdesc))
		format = "symbolic";
	call_gen_struct_member(label, mlp, dimension, format);
}

print_array(struct mlist *mlp, struct tdesc *tdp, char *format,
					char *label, dimension_t *dimension)
{
	struct ardef *ap = tdp->data.ardef;
	int items, inc, limit;
	dimension_t this_dimension;

	items = ap->indices->range_end - ap->indices->range_start + 1;
	inc = (mlp->size / items) / 8;
	limit = mlp->size / 8;

	/* Add a dimension to the end of the dimension list. */
	this_dimension.num_elements = items;
	if (dimension) {	/* list not empty (multi-dimensional array) */

		this_dimension.next = dimension;
		this_dimension.prev = dimension->prev;

		dimension->prev->next = &this_dimension;
		dimension->prev	= &this_dimension;
	} else {		/* list empty (this is first dimension) */
		this_dimension.next = &this_dimension;
		this_dimension.prev = &this_dimension;
		dimension = &this_dimension;
	}

	switch_on_type(mlp, ap->contents, format, label, dimension);
}

print_function(struct mlist *mlp, struct tdesc *tdp, char *format,
					char *label, dimension_t *dimension)
{
	fprintf(stderr, "function in struct %s\n", tdp->name);
}

print_struct(struct mlist *mlp, struct tdesc *tdp, char *format,
					char *label, dimension_t *dimension)
{
	char format_buf[128];

	if (!format)
	/*
	 * If we still don't have a format, look for a relevant "format" entry.
	 * If there is none, use .Tag (if the struct has a tag).
	 */
	if (!format) {

		/* Make sure the struct has a tag. */
		if (!tdp->name || !*tdp->name) {
			fprintf(stderr, "WARNING: "
				"substruct with no tag/type ignored: %s\n",
				mlp->name);
			return;
		}

		/* Get associated format or default to .Tag. */
		format = get_format(tdp->name);
		if (!format) {
			*format_buf = '.';
			strcpy(format_buf+1, renamed(tdp->name));
			format = format_buf;
		}
	}
	call_gen_struct_member(label, mlp, dimension, format);
}

print_union(struct mlist *mlp, struct tdesc *tdp, char *format,
				char *label, dimension_t *dimension)
{
	call_gen_struct_member(label, mlp, dimension, format);
}

print_enum(struct mlist *mlp, struct tdesc *tdp, char *format,
				char *label, dimension_t *dimension)
{
	call_gen_struct_member(label, mlp, dimension, format);
}

switch_on_type(struct mlist *mlp, struct tdesc *tdp, char *format,
				char *label, dimension_t *dimension)
{
	switch (tdp->type) {

	case INTRINSIC:
		print_intrinsic(mlp, tdp, format, label, dimension);
		break;
	case POINTER:
		print_pointer(mlp, tdp, format, label, dimension);
		break;
	case ARRAY:
		print_array(mlp, tdp, format, label, dimension);
		break;
	case FUNCTION:
		print_function(mlp, tdp, format, label, dimension);
		break;
	case UNION:
		print_union(mlp, tdp, format, label, dimension);
		break;
	case ENUM:
		print_enum(mlp, tdp, format, label, dimension);
		break;
	case FORWARD:
		print_forward(mlp, tdp, format, label, dimension);
		break;
	case TYPEOF:
		print_typeof(mlp, tdp, format, label, dimension);
		break;
	case STRUCT:
		print_struct(mlp, tdp, format, label, dimension);
		break;
	case CONST:
		print_const(mlp, tdp, format, label, dimension);
		break;
	case VOLATILE:
		print_volatile(mlp, tdp, format, label, dimension);
		break;
	default:
		fprintf(stderr, "Switch to Unknown type\n");
		break;
	}
}

void
do_complete_match(tdp, chp, mlp, np)
struct tdesc	*tdp;
struct child	*chp;
struct mlist	*mlp;
struct node	*np;
{
	char		*p;
	char		*label, *pattern, *org_pattern;
	int		nested, operated;
	int		offset, size, len = 0;
	int		*ip;
	void		switch_on_leaf_type();

	/* Allocate for the pattern string */
	len = chp->pattern->cells_end - chp->pattern->cells;
	pattern = (char *) malloc(len+1);

	/*
	 *  The pat_handle_t contains the input characters in integer
	 *  slots in addition to the operators NOT, AND, etc. If the value
	 *  is >0, it is a character to be matched, otherwise it is an
	 *  operator.
	 */
	nested = operated = 0;
	ip = chp->pattern->cells;
	for (p = pattern; ip < chp->pattern->cells_end; p++, ip++) {
	    if ((*ip) > 0) {
		*p = (char) (*ip);	/* hope this works! */
		if (*p == '.')
		    nested = 1;
	    } else
		operated = 1;
	}
	*p = NULL;

	/*
	 * If this is the pattern for a leaf member, do the
	 * simple thing.
	 */
	if (!nested) {
		/* If it doesn't match this pattern, skip it. */
		label = get_label(mlp->name, np->prefix);
		if (!PAT_STRMATCH(chp->pattern, label))
		    return;

		/* A new match; process it and mark it matched. */
		switch_on_type(mlp, mlp->fdesc, chp->format,
					chp->label ? chp->label : label, 0);
		found_match = 1;
		return;
	} else if (operated) {
		/*
		 * The situation is hopeless; we have to print a nested
		 * member and the first part is a regex. We don't have the
		 * original <raw> pattern string to do a recompile of the
		 * first part.
		 */
		fprintf(stderr,
			"warning: unexpected regex in %s\n", np->name);
		return;
	} else {
		org_pattern = strdup(pattern);

		/*
		 * sigh!.. somebody wants to print out a s/u member
		 * inside this parent structure
		 */
		p = strchr(pattern, '.');
		*p++ = NULL;

		/* No more regular expression matches */

		/* Check first part */
		label = get_label(mlp->name, np->prefix);
		if (strcmp(pattern, label) != 0)
		    return;

		/* The offset/size to print (for now) */
		offset = mlp->offset;
		size = mlp->size;

		/* Start matching subsequent parts */
		pattern = p;	/* point to char after '.' */
		while (pattern) {
		    if ((p = strchr(pattern, '.')) != NULL)
			*p++ = NULL;

		    if ((mlp = find_member(mlp->fdesc, pattern)) == NULL)
			return;

		    offset += mlp->offset;
		    size = mlp->size;
		    pattern = p;
		}

		/*
		 * It is better to print the whole pattern if the user has
		 * not supplied any label
		 */
		switch_on_leaf_type(mlp, chp->format,
			chp->label ? chp->label : org_pattern, offset, size);


		free(pattern);
		free(org_pattern);

		found_match = 1;
	}
}

void
switch_on_leaf_type(mlp, format, label, offset, size)
struct mlist	*mlp;
char		*format, *label;
int		offset, size;
{
	struct tdesc		*tdp = mlp->fdesc;

	/*
	 * We don't handle leaf elements of array/union/volatile/struct
	 * types in multi-level members
	 */
	switch (tdp->type) {
	    case ENUM:			/* fall through */
	    case INTRINSIC:		/* fall through */
	    case POINTER:		/* fall through */
	    case TYPEOF:
	    case CONST:
	    case VOLATILE:
		if (!format)
		    format = "hex";
		gen_struct_member(label, offset, NULL, size, format);
		break;

	    case FUNCTION:
		fprintf(stderr, "function in struct %s\n", tdp->name);
		break;
	    case FORWARD:
		fprintf(stderr, "%s never defined\n", mlp->name);
		break;

	    /* Unsupported types for leaf member in multi-level members */
	    case UNION:
	    case ARRAY:
	    case STRUCT:
	    default:
		fprintf(stderr, "Switch to Unknown type %s (%d) in %s\n",
					tdp->name, tdp->type, mlp->name);
		break;
	}
}

int
get_offset(struct_name, member_name)
char	*struct_name;
char	*member_name;
{
	struct tdesc	*tp;
	struct mlist	*mlp;
	char		*p, *q, *tmp_mem_name;
	char		*name;
	int		offset, index;
	int		nelem, elem_size;
	char		*get_base_name();

	if ((tp = lookupname(struct_name)) == NULL) {
		fprintf(stderr,
			"warning: can't find struct name %s\n", struct_name);
		return (-1);
	}

	/* check if "member_name" is a sub-structure's member name */
	if ((p = strchr(member_name, '.')) == NULL) {
		if ((q = strchr(member_name, '[')) == NULL) {
		    for (mlp = tp->data.members;
					mlp != NULL; mlp = mlp->next) {
			if (strcmp(mlp->name, member_name) == NULL)
			    return (mlp->offset / BITS_PER_BYTE);
		    }
		    return (-1);
		}

		/* member_name is an array element! */
		name = get_base_name(member_name, q);
		sscanf(q+1, "%d", &index);

		for (mlp = tp->data.members; mlp != NULL; mlp = mlp->next) {
		    if (strcmp(mlp->name, name) == NULL) {
			offset = mlp->offset;
			break;
		    }
		}

		if (mlp == NULL)
		    return (-1);

		/*
		 * The offset determination for an array member requires
		 * some calculation involving size of each array element.
		 * But the structure 'mlp' will have only the total
		 * size occupied by the entire structure. We need to divide
		 * it by the number of elements to get individual size and
		 * then calculate the offset to the exact array element
		 */
		if (mlp->fdesc->type != ARRAY) {
		    fprintf(stderr,
			"warning: %s not an array memb.\n", member_name);
		    free(name);
		    return (-1);
		}

		if (index) {
		    nelem = mlp->fdesc->data.ardef->indices->range_end -
			    mlp->fdesc->data.ardef->indices->range_start + 1;
		    elem_size = mlp->size / nelem;
		}

		/* Add the offset of the indexed element in the array */
		offset += (elem_size * index);

		free(name);
		return (offset / BITS_PER_BYTE);
	}

	/* make a local copy of member name */
	tmp_mem_name = strdup(member_name);

	/* Set up member name */
	member_name = tmp_mem_name;
	p = strchr(member_name, '.');

	/* check each part and update offset */
	offset = 0;
	while (member_name) {
	    if (p != NULL)
		*p++ = NULL;


	    /* check if the member is an array */
	    if (q = strchr(member_name, '[')) {
		name = get_base_name(member_name, q);
		sscanf(q+1, "%d", &index);
		if ((mlp = find_member(tp, name)) == NULL) {
		    free(name);
		    free(tmp_mem_name);
		    return (-1);
		}

		/*
		 * The offset determination for an array member requires
		 * some calculation involving size of each array element.
		 * But the structure 'mlp' will have only the total
		 * size occupied by the entire structure. We need to divide
		 * it by the number of elements to get individual size and
		 * then calculate the offset to the exact array element
		 */
		if (mlp->fdesc->type != ARRAY) {
		    fprintf(stderr,
			"warning: %s not array memb\n", member_name);
		    free(name);
		    free(tmp_mem_name);
		    return (-1);
		}

		if (index) {
		    nelem = mlp->fdesc->data.ardef->indices->range_end -
			    mlp->fdesc->data.ardef->indices->range_start + 1;
		    elem_size = mlp->size/nelem;
		}

		/*
		 * Add offset OF the array in the structure later. Now just
		 * add the offset of the indexed element IN the array
		 */
		offset += (elem_size * index);
	    } else if ((mlp = find_member(tp, member_name)) == NULL) {
		free(tmp_mem_name);
		return (-1);
	    }

	    /* Update offset and move to this substructure */
	    offset += mlp->offset;
	    if (mlp->fdesc->type == ARRAY)
		tp = mlp->fdesc->data.ardef->contents;
	    else
		tp = mlp->fdesc;

	    /* Move tmp_mem_name, p so they point to next part */
	    member_name = p;
	    if (member_name)
		p = strchr(member_name, '.');
	}

	/* free local copy */
	free(tmp_mem_name);

	return (offset/BITS_PER_BYTE);
}

char *
get_base_name(cur, end)
char	*cur;
char	*end;
{
	char	*p, *base_name;

	/* allocate for name */
	p = base_name = malloc(end - cur + 1);

	/* get the array name and index count */
	while ((cur < end) && *cur && (!isspace(*cur)))
	    *p++ = *cur++;
	*p = NULL;

	return (base_name);
}

char *
get_pat_name(pat_val)
pat_handle_t	pat_val;
{
	int		len = 0;
	int		*ip;
	char		*p, *pattern;

	/* Allocate for the pattern string */
	len = pat_val->cells_end - pat_val->cells;
	pattern = (char *) malloc(len+1);

	/*
	 *  The pat_handle_t contains the input characters in integer
	 *  slots in addition to the operators NOT, AND, etc. If the value
	 *  is >0, it is a character to be matched, otherwise it is an
	 *  operator.
	 */
	ip = pat_val->cells;
	for (p = pattern; ip < pat_val->cells_end; p++, ip++) {
	    if ((*ip) > 0)
		*p = (char) (*ip);
	    else
		*p = '%';	/* to show it is an operator here! */
	}
	*p = NULL;

	return (pattern);
}

void
set_model_defaults()
{
	struct mlist	*mlp;
	struct tdesc	*tdp;

	/* set up default sizes (in bytes) for ILP32 model */
	_sizeof_int = _sizeof_long = _sizeof_ptr = (32 / BITS_PER_BYTE);

	if ((tdp = lookupname("__dummy")) == NULL) {
	    fprintf(stderr,
		"warning: cannot determine data model, assuming ILP32.\n");
	} else {
	    for (mlp = tdp->data.members; mlp != NULL; mlp = mlp->next) {
		    if (strcmp(mlp->name, "long_val") == 0)
			    _sizeof_long = mlp->size / BITS_PER_BYTE;
		    else if (strcmp(mlp->name, "ptr_val") == 0)
			    _sizeof_ptr = mlp->size / BITS_PER_BYTE;
		    else if (strcmp(mlp->name, "int_val") == 0)
			    _sizeof_int = mlp->size / BITS_PER_BYTE;
	    }
	}
}
