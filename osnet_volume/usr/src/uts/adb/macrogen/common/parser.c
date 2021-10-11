
/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)parser.c	1.6	99/06/17 SMI"

#include "stabs.h"

jmp_buf	resetbuf;

char	*whitesp(), *name(), *id(), *decl(), *number(), *offsize();
char	*tdefdecl(), *intrinsic(), *arraydef();
void	addhash();

static int line_number = 0;
static int debug_line  = 0;
static char linebuf[MAXLINE];


/* Report unexpected syntax in stabs. */
void
expected(
	char *who,	/* what function, or part thereof, is reporting */
	char *what,	/* what was expected */
	char *where)	/* where we were in the line of input */
{
	fprintf(stderr, "%s, input line %d: expecting \"%s\" at \"%s\"\n",
		who, line_number, what, where);
	exit(1);
}

/* Read a line from stdin into linebuf and increment line_number. */
char *
get_line()
{
	char *cp = fgets(linebuf, MAXLINE, stdin);
	line_number++;

	/* For debugging, you can set debug_line to a line to stop on. */
	if (line_number == debug_line) {
		fprintf(stderr, "Hit debug line number %d\n", line_number);
		for (;;)
			sleep(1);
	}
	return (cp);
}

/* Get the continuation of the current input line. */
char *
get_continuation()
{
	char *cp = get_line();
	if (!cp) {
		fprintf(stderr,
			"expecting continuation line, but end of input\n");
		exit(1);
	}

	/* Skip to the quoted stuff. */
	while (*cp++ != '"')
		;
	return (cp);
}

parse_input()
{
	char *cp;
	int i = 0;

	while (i++ < BUCKETS) {
		hash_table[i] = NULL;
		name_table[i] = NULL;
	}

	/*
	 * get a line at a time from the .s stabs file and parse.
	 */
	while (cp = get_line())
		parseline(cp);
}

/*
 * Parse each line of the .s file (stabs entry) gather meaningful information
 * like name of type, size, offsets of fields etc.
 */
parseline(cp)
	char *cp;
{
	struct tdesc *tdp;
	char c, *w;
	int h, tagdef;
	int debug;

	/*
	 * setup for reset()
	 */
	if (setjmp(resetbuf))
		return;

	/*
	 * Look for lines of the form
	 *	.stabs	"str",n,n,n,n
	 * The part in '"' is then parsed.
	 */
	cp = whitesp(cp);
#define	STLEN	6
	if (strncmp(cp, ".stabs", STLEN) != 0)
		reset();
	cp += STLEN;
#undef STLEN
	cp = whitesp(cp);
	if (*cp++ != '"')
		reset();

	/*
	 * name:type		variable (ignored)
	 * name:ttype		typedef
	 * name:Ttype		struct tag define
	 */
	cp = name(cp, &w);
	switch (c = *cp++) {
	case 't': /* type */
		tagdef = 0;
		break;
	case 'T': /* struct, union, enum */
		tagdef = 1;
		break;
	default:
		reset();
	}

	/*
	 * The type id and definition follow.
	 */
	cp = id(cp, &h);
	if (*cp++ != '=')
		expected("parseline", "=", cp - 1);
	if (tagdef) {
		tagdecl(cp, &tdp, h, w);
	} else {
		tdefdecl(cp, &tdp);
		tagadd(w, h, tdp);
	}
}

/*
 * Check if we have this node in the hash table already
 */
struct tdesc *
lookup(int h)
{
	int hash = HASH(h);
	struct tdesc *tdp = hash_table[hash];

	while (tdp != NULL) {
		if (tdp->id == h)
			return (tdp);
		tdp = tdp->hash;
	}
	return (NULL);
}

char *
whitesp(cp)
	char *cp;
{
	char *orig, c;

	orig = cp;
	for (c = *cp++; isspace(c); c = *cp++)
		;
	if (--cp == orig)
		reset();
	return (cp);
}

char *
name(cp, w)
	char *cp, **w;
{
	char *new, *orig, c;
	int len;

	orig = cp;
	c = *cp++;
	if (c == ':')
		*w = NULL;
	else if (isalpha(c) || c == '_') {
		for (c = *cp++; isalnum(c) || c == ' ' || c == '_'; c = *cp++)
			;
		if (c != ':')
			reset();
		len = cp - orig;
		new = (char *)malloc(len);
		while (orig < cp - 1)
			*new++ = *orig++;
		*new = '\0';
		*w = new - (len - 1);
	} else
		reset();

	return (cp);
}

char *
number(cp, n)
	char *cp;
	long *n;
{
	char *next;

	*n = strtol(cp, &next, 10);
	if (next == cp)
		expected("number", "<number>", cp);
	return (next);
}

char *
id(cp, h)
	char *cp;
	int *h;
{
	long n1, n2;

	if (*cp++ != '(')
		expected("id", "(", cp - 1);
	cp = number(cp, &n1);
	if (*cp++ != ',')
		expected("id", ",", cp - 1);
	cp = number(cp, &n2);
	if (*cp++ != ')')
		expected("id", ")", cp - 1);
	*h = n1 * 1000 + n2;
	return (cp);
}

tagadd(char *w, int h, struct tdesc *tdp)
{
	struct tdesc *otdp, *hash;

	tdp->name = w;
	if (!(otdp = lookup(h)))
		addhash(tdp, h);
	else if (otdp != tdp) {
		fprintf(stderr, "duplicate entry\n");
		fprintf(stderr, "old: %s %d %d %d\n",
			otdp->name ? otdp->name : "NULL",
			otdp->type, otdp->id / 1000, otdp->id % 1000);
		fprintf(stderr, "new: %s %d %d %d\n",
			tdp->name ? tdp->name : "NULL",
			tdp->type, tdp->id / 1000, tdp->id % 1000);
	}
}

tagdecl(cp, rtdp, h, w)
	char *cp;
	struct tdesc **rtdp;
	int h;
	char *w;
{
	if (*rtdp = lookup(h)) {
		if ((*rtdp)->type != FORWARD)
			fprintf(stderr, "found but not forward: %s \n", cp);
	} else {
		*rtdp = ALLOC(struct tdesc);
		(*rtdp)->name = w;
		addhash(*rtdp, h);
	}

	switch (*cp++) {
	case 's':
		soudef(cp, STRUCT, rtdp);
		break;
	case 'u':
		soudef(cp, UNION, rtdp);
		break;
	case 'e':
		enumdef(cp, rtdp);
		break;
	default:
		expected("tagdecl", "<tag type s/u/e>", cp - 1);
	}
}

char *
tdefdecl(cp, rtdp)
	char *cp;
	struct tdesc **rtdp;
{
	struct tdesc *tdp, *ntdp;
	char *w;
	int c, h;

	/* Type codes */
	switch (*cp) {
	case 'b': /* integer */
		c = *++cp;
		if (c != 's' && c != 'u')
			expected("tdefdecl/b", "[su]", cp - 1);
		c = *++cp;
		if (c == 'c')
			cp++;
		cp = intrinsic(cp, rtdp);
		break;
	case 'R': /* fp */
		cp += 3;
		cp = intrinsic(cp, rtdp);
		break;
	case '(': /* equiv to another type */
		cp = id(cp, &h);
		ntdp = lookup(h);
		if (ntdp == NULL) {  /* if that type isn't defined yet */
			if (*cp++ != '=')  /* better be defining it now */
				expected("tdefdecl/'('", "=", cp - 1);
			cp = tdefdecl(cp, rtdp);
			addhash(*rtdp, h); /* for *(x,y) types */
		} else { /* that type is already defined */
			*rtdp = ALLOC(struct tdesc);
			(*rtdp)->type = TYPEOF;
			(*rtdp)->data.tdesc = ntdp;
		}
		break;
	case '*':
		cp = tdefdecl(cp + 1, &ntdp);
		*rtdp = ALLOC(struct tdesc);
		(*rtdp)->type = POINTER;
		(*rtdp)->size = sizeof (void *);
		(*rtdp)->name = "pointer";
		(*rtdp)->data.tdesc = ntdp;
		break;
	case 'f':
		cp = tdefdecl(cp + 1, &ntdp);
		*rtdp = ALLOC(struct tdesc);
		(*rtdp)->type = FUNCTION;
		(*rtdp)->size = sizeof (void *);
		(*rtdp)->name = "function";
		(*rtdp)->data.tdesc = ntdp;
		break;
	case 'a':
		cp++;
		if (*cp++ != 'r')
			expected("tdefdecl/a", "r", cp - 1);
		*rtdp = ALLOC(struct tdesc);
		(*rtdp)->type = ARRAY;
		(*rtdp)->name = "array";
		cp = arraydef(cp, rtdp);
		break;
	case 'x':
		c = *++cp;
		if (c != 's' && c != 'u' && c != 'e')
			expected("tdefdecl/x", "[sue]", cp - 1);
		cp = name(cp + 1, &w);
		*rtdp = ALLOC(struct tdesc);
		(*rtdp)->type = FORWARD;
		(*rtdp)->name = w;
		break;
	case 'B': /* volatile */
		cp = tdefdecl(cp + 1, &ntdp);
		*rtdp = ALLOC(struct tdesc);
		(*rtdp)->type = VOLATILE;
		(*rtdp)->size = 0;
		(*rtdp)->name = "volatile";
		(*rtdp)->data.tdesc = ntdp;
		break;
	case 'k': /* const */
		cp = tdefdecl(cp + 1, &ntdp);
		*rtdp = ALLOC(struct tdesc);
		(*rtdp)->type = CONST;
		(*rtdp)->size = 0;
		(*rtdp)->name = "const";
		(*rtdp)->data.tdesc = ntdp;
		break;
	default:
		expected("tdefdecl", "<type code>", cp);
	}
	return (cp);
}

char *
intrinsic(cp, rtdp)
	char *cp;
	struct tdesc **rtdp;
{
	struct tdesc *tdp;
	long size;

	cp = number(cp, &size);
	tdp = ALLOC(struct tdesc);
	tdp->type = INTRINSIC;
	tdp->size = size;
	tdp->name = NULL;
	*rtdp = tdp;
	return (cp);
}

soudef(cp, type, rtdp)
	char *cp;
	enum type type;
	struct tdesc **rtdp;
{
	struct mlist *mlp, **prev;
	char *w;
	int h, i = 0;
	long size;
	struct tdesc *tdp;

	cp = number(cp, &size);
	(*rtdp)->size = size;
	(*rtdp)->type = type; /* s or u */
	/*
	 * An '@' here indicates a bitmask follows.   This is so the
	 * compiler can pass information to debuggers about how structures
	 * are passed in the v9 world.  We don't need this information
	 * so we skip over it.
	 */
	if (cp[0] == '@')
		cp += 3;

	prev = &((*rtdp)->data.members);
	/* now fill up the fields */
	while ((*cp != '"') && (*cp != ';')) { /* signifies end of fields */
		mlp = ALLOC(struct mlist);
		*prev = mlp;
		cp = name(cp, &w);
		mlp->name = w;
		cp = id(cp, &h);
		/*
		 * find the tdesc struct in the hash table for this type
		 * and stick a ptr in here
		 */
		tdp = lookup(h);
		if (tdp == NULL) { /* not in hash list */
			if (*cp++ != '=')
				expected("soudef", "=", cp - 1);
			cp = tdefdecl(cp, &tdp);
			addhash(tdp, h);
		}

		mlp->fdesc = tdp;
		cp = offsize(cp, mlp);
		/* cp is now pointing to next field */
		prev = &mlp->next;
		/* could be a continuation */
		if (*cp == '\\')
			cp = get_continuation();
	}
}

char *
offsize(cp, mlp)
	char *cp;
	struct mlist *mlp;
{
	long offset, size;

	if (*cp++ != ',')
		expected("offsize/1", ",", cp - 1);
	cp = number(cp, &offset);
	if (*cp++ != ',')
		expected("offsize/2", ",", cp - 1);
	cp = number(cp, &size);
	if (*cp++ != ';')
		expected("offsize/3", ";", cp - 1);
	mlp->offset = offset;
	mlp->size = size;
	return (cp);
}

char *
arraydef(char *cp, struct tdesc **rtdp)
{
	int h;
	long start, end;

	cp = id(cp, &h);
	if (*cp++ != ';')
		expected("arraydef/1", ";", cp - 1);

	(*rtdp)->data.ardef = ALLOC(struct ardef);
	(*rtdp)->data.ardef->indices = ALLOC(struct element);
	(*rtdp)->data.ardef->indices->index_type = lookup(h);

	cp = number(cp, &start);
	if (*cp++ != ';')
		expected("arraydef/2", ";", cp - 1);
	cp = number(cp, &end);
	if (*cp++ != ';')
		expected("arraydef/3", ";", cp - 1);
	(*rtdp)->data.ardef->indices->range_start = start;
	(*rtdp)->data.ardef->indices->range_end = end;
	cp = tdefdecl(cp, &((*rtdp)->data.ardef->contents));
	return (cp);
}

enumdef(char *cp, struct tdesc **rtdp)
{
	char *next;
	struct elist *elp, **prev;
	char *w;

	(*rtdp)->type = ENUM;
	(*rtdp)->data.emem = NULL;

	prev = &((*rtdp)->data.emem);
	while (*cp != ';') {
		elp = ALLOC(struct elist);
		elp->next = NULL;
		*prev = elp;
		cp = name(cp, &w);
		elp->name = w;
		cp = number(cp, &elp->number);
		prev = &elp->next;
		if (*cp++ != ',')
			expected("enumdef", ",", cp - 1);
		if (*cp == '\\')
			cp = get_continuation();
	}
}

/*
 * Add a node to the hash queues.
 */
void
addhash(tdp, num)
	struct tdesc *tdp;
	int num;
{
	int hash = HASH(num);

	tdp->id = num;
	tdp->hash = hash_table[hash];
	hash_table[hash] = tdp;

	if (tdp->name) {
		hash = compute_sum(tdp->name);
		tdp->next = name_table[hash];
		name_table[hash] = tdp;
	}
}

struct tdesc *
lookupname(name)
	char *name;
{
	int hash = compute_sum(name);
	struct tdesc *tdp, *ttdp = NULL;

	for (tdp = name_table[hash]; tdp != NULL; tdp = tdp->next) {
		if (tdp->name != NULL && strcmp(tdp->name, name) == 0) {
			if (tdp->type == STRUCT || tdp->type == UNION ||
			    tdp->type == ENUM)
				return (tdp);
			if (tdp->type == TYPEOF)
				ttdp = tdp;
		}
	}
	return (ttdp);
}

int
compute_sum(char *w)
{
	char c;
	int sum;

	for (sum = 0; c = *w; sum += c, w++)
		;
	return (HASH(sum));
}

reset()
{
	longjmp(resetbuf, 1);
	/* NOTREACHED */
}
