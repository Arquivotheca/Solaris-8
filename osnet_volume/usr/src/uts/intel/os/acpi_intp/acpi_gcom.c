/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_gcom.c	1.1	99/05/21 SMI"


/* ACPI grammar compiler */

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <sys/inttypes.h>

#include <sys/acpi.h>
#include <sys/acpi_prv.h>
#include "acpi_exc.h"


/* simple linked list */
typedef struct _element {
	int index;		/* used to order grammar constructs */
	int flag;		/* mark to check consistency */
	struct _element *next;
	char *name;
	char *value;
} element;

/* special element index values */
#define	INDEX_NONE (-1)
#define	INDEX_SKIP (-2)		/* skip index equality test */

/* mark whether elements have been used for consistency checking */
#define	FLAG_NONE (0x0)
#define	FLAG_MARK (0x1)		/* used in current list */
#define	FLAG_DIST (0x2)		/* distinguished element */
#define	FLAG_ONCE (0x4)		/* used once in global list */

void
list_add(element **listp, char *name, char *value, int flags)
{
	int name_len, value_len, size;
	char *ptr;
	element *ep;

	name_len = name ? strlen(name) : 0;
	value_len = value ? strlen(value) : 0;
	size = sizeof (element) + name_len + value_len + 2; /* 2 NULLs */

	if ((ptr = malloc(size)) == NULL)
		(void) exc_panic("malloc error");
	bzero(ptr, size);
	ep = (element *)ptr;

	if (name) {
		ep->name = ptr + sizeof (element);
		(void) strcpy(ep->name, name);
	}
	ep->index = INDEX_NONE;
	ep->flag = flags;
	if (value) {
		ep->value = ptr + sizeof (element) + name_len + 1; /* 1 NULL */
		(void) strcpy(ep->value, value);
	}

	ep->next = *listp;
	*listp = ep;
}

/*
 * Skip name test, if NULL.
 * Index could be 0, INDEX_NONE, INDEX_SKIP = don't bother with test.
 */
element *
list_member(element *list, char *name, int index)
{
	int name_test, index_test;

	for (; list; list = list->next) {
		if (name)
			name_test = (strcmp(name, list->name) == 0) ? 1 : 0;
		else
			name_test = 1;
		if (index != INDEX_SKIP)
			index_test = (index == list->index) ? 1 : 0;
		else
			index_test = 1;
		if (name_test && index_test)
			return (list);
	}
	return (NULL);
}

void
list_clear_flag(element *list)
{
	for (; list; list = list->next)
		list->flag &= ~FLAG_MARK;
}

void
list_unused(element *list)
{
	for (; list; list = list->next)
		if ((list->flag & FLAG_ONCE) == 0)
			(void) exc_warn("%s not used", list->name);
}


/* number terminals */
void
number_term(element *list, int *indp)
{
	int index;

	for (index = *indp; list; list = list->next)
		if (list->index == INDEX_NONE)
			list->index = index++;
	*indp = index;
}

/*
 * number non-terminals (and rules) based on rule membership
 *
 * indexp = ptr to a counter, next number to assign.
 * maxp = records the highest value used, including any we come across that
 *	were previously assigned.
 */
int
number_nt(element *rules, element *nt, int *indexp, int *maxp)
{
	element *ptr;
	int index, max;

	max = -1;
	for (index = *indexp; rules; rules = rules->next) {
		if ((ptr = list_member(nt, rules->name, INDEX_SKIP)) == NULL)
			return (exc_warn("missing non-terminal %s",
			    rules->name));
		if (ptr->flag & FLAG_MARK)
			return (exc_warn("non-terminal %s already used",
			    rules->name));
		if (ptr->index == INDEX_NONE) {
			ptr->index = index;
			rules->index = index;
			index++;
		} else
			rules->index = ptr->index;
		if (max < ptr->index)
			max = ptr->index;
		ptr->flag |= FLAG_MARK|FLAG_ONCE;
	}
	*indexp = index;
	*maxp = max;
	return (ACPI_OK);
}

/* print non-terminal or terminal index */
void
emit_index(FILE *fp, element *list, int min, int max, char *tag, char *errstr)
{
	int i, dist;
	element *ptr;

	dist = -1;
	(void) fprintf(fp, "\n/* %ss */\n", errstr);
	for (i = min; i <= max; i++) {
		if ((ptr = list_member(list, 0, i)) == NULL)
			(void) exc_panic("missing %s %d", errstr, i);
		if (ptr->flag & FLAG_DIST)
			dist = i;
		(void) fprintf(fp, "#define\t%s %d\n", ptr->name, i);
	}
	(void) fprintf(fp, "#define\tMAX_%s %d\n", tag, max);
	(void) fprintf(fp, "#define\tSPEC_%s %d\n", tag, dist);
}

/* debug strings */
void
emit_debug(FILE *fp, element *nt, int nt_max, element *term, int t_max)
{
	int i;
	element *ptr;

	(void) fprintf(fp, "\n/* debug strings */\n");
	(void) fprintf(fp, "char *debug_elem_strings[] = {\n");
	for (i = 0; i <= nt_max; i++) {
		if ((ptr = list_member(nt, 0, i)) == NULL)
			(void) exc_panic("missing debug %d", i);
		(void) fprintf(fp, "\t\"%s\",\n", ptr->name);
	}
	for (; i <= t_max; i++) {
		if ((ptr = list_member(term, 0, i)) == NULL)
			(void) exc_panic("missing debug %d", i);
		(void) fprintf(fp, "\t\"%s\",\n", ptr->name);
	}
	(void) fprintf(fp, "};\n");
}

/* print rules */
void
emit_rules(FILE *fp, element *list, int max,
    char *tag, char *max_tag, int tflag, int split)
{
	int i;
	element *ep;
	char *ptr;

	(void) fprintf(fp, "\n/* %s rules */\n", tag);
	if (tflag)
		for (i = 0; i <= max; i++)
			if (ep = list_member(list, 0, i))
				if (split) {
					for (ptr = ep->value;
						*ptr != ',' && *ptr != NULL;
						ptr++)
						;
					if (*ptr == ',') {
						*ptr = NULL;
						ptr++;
					}
					(void) fprintf(fp,
					    "%s_entry_t %s_entry_%d[]"
					    " = { %s };\n",
					    tag, tag, i, ptr);
				} else
					(void) fprintf(fp,
					    "%s_entry_t %s_entry_%d[]"
					    " = { %s };\n",
					    tag, tag, i, ep->value);
	for (i = 0; i <= max; i++)
		if (ep = list_member(list, 0, i))
			if (split)
				(void) fprintf(fp, "%s_rule_t %s_%d = {R_%s|"
				    "sizeof (%s_entry_%d)/"
				    "sizeof (%s_entry_t), %s, "
				    "&%s_entry_%d[0]};\n",
				    tag, tag, i, max_tag, tag, i, tag,
				    ep->value, tag, i);
			else if (tflag)
				(void) fprintf(fp, "%s_rule_t %s_%d = {R_%s|"
				    "sizeof(%s_entry_%d)/"
				    "sizeof(%s_entry_t), "
				    "&%s_entry_%d[0]};\n",
				    tag, tag, i, max_tag, tag, i, tag, tag, i);
			else
				(void) fprintf(fp, "%s_rule_t %s_%d = "
				    "{R_%s, %s};\n",
				    tag, tag, i, max_tag, ep->value);
}

/* print table */
void
emit_table(FILE *fp, element *lr, element *ar, element *pr, element *xr,
    int ml, int ma, int mp, int mx)
{
	int i;

	(void) fprintf(fp, "\n/* parse table */\n");
	(void) fprintf(fp, "void *parse_table[] = {\n");
	for (i = 0; i <= ml; i++)
		if (list_member(lr, 0, i))
			(void) fprintf(fp, "\t&list_%d,\n", i);
		else
			(void) fprintf(fp, "\t0,\n");
	for (; i <= ma; i++)
		if (list_member(ar, 0, i))
			(void) fprintf(fp, "\t&alt_%d,\n", i);
		else
			(void) fprintf(fp, "\t0,\n");
	for (; i <= mp; i++)
		if (list_member(pr, 0, i))
			(void) fprintf(fp, "\t&prod_%d,\n", i);
		else
			(void) fprintf(fp, "\t0,\n");
	for (; i <= mx; i++)
		if (list_member(xr, 0, i))
			(void) fprintf(fp, "\t&lex_%d,\n", i);
		else
			(void) fprintf(fp, "\t0,\n");
	(void) fprintf(fp, "\t};\n");
}

/* some simple I/O routines */
#define	MAX_LINE 1024
char line_buf[MAX_LINE];
int line_num = 1;

int
find(FILE *fp, char *token, char *buf, int len)
{
	int tlen = strlen(token);

	for (;;) {
		if (fgets(buf, len, fp) == NULL)
			return (ACPI_EXC);
		line_num++;
		if (strncmp(token, buf, tlen) == 0)
			return (ACPI_OK);
	}
}

/* returns SKIP if we found the token */
#define	SKIP (1)

int
next_line(FILE *fp, char *token, char *buf, int len)
{
	int tlen = strlen(token);

	for (;;) {
		if (fgets(buf, len, fp) == NULL)
			return (ACPI_EXC);
		line_num++;
		/* '#' = comment character */
		if (buf[0] != '#' && buf[0] != '\n' && buf[0] != NULL)
			break;
	}
	if (strncmp(token, buf, tlen) == 0)
		return (SKIP);
	return (ACPI_OK);
}

void
list_read(FILE *fp, element **listp, int valuef, int *nump,
    char *token, char *errstr)
{
	int ret, i, index;
	char *name, *value;

	*listp = NULL;
	for (index = 0; ; index++) {
		ret = next_line(fp, token, line_buf, MAX_LINE);
		if (ret == ACPI_EXC)
			(void) exc_panic("EOF in %s", errstr);
		if (ret == SKIP) {
			*nump = index;
			return;
		}

		/* name */
		for (i = 0; line_buf[i] == ' ' || line_buf[i] == '\t' ||
		    line_buf[i] == '\n'; i++)
			;
		if (line_buf[i] == NULL)
			(void) exc_panic("line %d, bad %s", line_num, errstr);
		name = line_buf + i;
		for (; line_buf[i] != ' ' && line_buf[i] != '\t' &&
		    line_buf[i] != '\n' && line_buf[i] != NULL; i++)
			;
		line_buf[i] = NULL;

		/* value */
		if (valuef) {
			for (i++; line_buf[i] == ' ' || line_buf[i] == '\t' ||
			    line_buf[i] == '\n'; i++)
				;
			if (line_buf[i] == NULL)
				(void) exc_panic("line %d, bad %s", line_num,
				    errstr);
			if (line_buf[i] != ':' || line_buf[i + 1] != '=')
				(void) exc_panic("line %d, bad %s", line_num,
				    errstr);
			i += 2;
			for (; line_buf[i] == ' ' || line_buf[i] == '\t'; i++)
				;
			value = line_buf + i;
			for (; line_buf[i] != '\n' && line_buf[i]; i++)
				;
			line_buf[i] = NULL;
		} else
			value = NULL;
		list_add(listp, name, value,
		    (valuef == 0 && index == 0) ? FLAG_DIST : FLAG_NONE);
	}
}

int
main(int argc, char *argv[])
{
	FILE *fp;
	int count;

	element *non_terms;
	element *terms;
	element *list_rules;
	element *alt_rules;
	element *prod_rules;
	element *lex_rules;

	int num_non_terms;
	int num_terms;
	int num_list_rules;
	int num_alt_rules;
	int num_prod_rules;
	int num_lex_rules;

	int max_non_terms;
	int max_terms;
	int max_list_rules;
	int max_alt_rules;
	int max_prod_rules;
	int max_lex_rules;

	exc_setlevel(ACPI_DVERB_WARN);
	exc_setpanic(exit);
	exc_settag("gcom");
	if (argc < 4)
		(void) exc_panic("missing file arguments");
	if ((fp = fopen(argv[1], "r")) == NULL)
		(void) exc_panic("can't open file");

#define	TOKEN_NON_TERM "\t****\tnon-terminals"
#define	TOKEN_TERM "\t****\tterminals"
#define	TOKEN_LIST "\t****\tlist rules"
#define	TOKEN_ALT "\t****\talternative rules"
#define	TOKEN_PROD "\t****\tproduction rules"
#define	TOKEN_LEX "\t****\tlex rules"
#define	TOKEN_END "\t****\tend"

	if (find(fp, TOKEN_NON_TERM, line_buf, MAX_LINE))
		(void) exc_panic("no non-terminals");

	/* read definitions */
	list_read(fp, &non_terms, 0, &num_non_terms, TOKEN_TERM,
	    "non-terminal");
	list_read(fp, &terms, 0, &num_terms, TOKEN_LIST, "terminal");
	list_read(fp, &list_rules, 1, &num_list_rules, TOKEN_ALT,
	    "list rules");
	list_read(fp, &alt_rules, 1, &num_alt_rules, TOKEN_PROD,
	    "alternative rules");
	list_read(fp, &prod_rules, 1, &num_prod_rules, TOKEN_LEX,
	    "production rules");
	list_read(fp, &lex_rules, 1, &num_lex_rules, TOKEN_END,
	    "lex rules");
	(void) fclose(fp);

	/* number elements */
	count = 0;
	if (number_nt(list_rules, non_terms, &count, &max_list_rules))
		(void) exc_panic("error in list rules");
	list_clear_flag(non_terms);
	if (number_nt(alt_rules, non_terms, &count, &max_alt_rules))
		(void) exc_panic("error in alternative rules");
	list_clear_flag(non_terms);
	if (number_nt(prod_rules, non_terms, &count, &max_prod_rules))
		(void) exc_panic("error in production rules");
	max_non_terms = count - 1;
	if (count < num_non_terms) {
		list_unused(non_terms);
		(void) exc_panic("some non-terminals unused");
	}

	/* actually doing terminals here */
	if (number_nt(lex_rules, terms, &count, &max_lex_rules))
		(void) exc_panic("error in lex rules");
	number_term(terms, &count);
	max_terms = count - 1;

	/* lex file */
	if ((fp = fopen(argv[2], "w")) == NULL)
		(void) exc_panic("can't open %s", argv[2]);
	(void) fprintf(fp, "/*\n * Copyright (c) 1999 by Sun Microsystems,"
	    " Inc.\n * All rights reserved.\n */\n\n");
	(void) fprintf(fp, "/*\n * lex file generated from file\n"
	    " * %s\n */\n\n", argv[1]);
	(void) fprintf(fp, "#ifndef\t_ACPI_ELEM_H\n");
	(void) fprintf(fp, "#define\t_ACPI_ELEM_H\n\n");
	(void) fprintf(fp, "#ifdef\t__cplusplus\n");
	(void) fprintf(fp, "extern \"C\" {\n");
	(void) fprintf(fp, "#endif\n\n");
	emit_index(fp, non_terms, 0, max_non_terms,
	    "NON_TERM", "non-terminal");
	emit_index(fp, terms, num_non_terms, max_terms,
	    "TERM", "terminal");
	(void) fprintf(fp, "\n/* parse table length */\n"
	    "#define\tMAX_PARSE_ENTRY %d\n\n", max_lex_rules);
	(void) fprintf(fp, "#ifdef __cplusplus\n");
	(void) fprintf(fp, "}\n");
	(void) fprintf(fp, "#endif\n\n");
	(void) fprintf(fp, "#endif /* _ACPI_ELEM_H */\n");
	(void) fclose(fp);

	/* rules file */
	if ((fp = fopen(argv[3], "w")) == NULL)
		(void) exc_panic("can't open %s", argv[3]);
	(void) fprintf(fp, "/*\n * Copyright (c) 1999 by Sun Microsystems,"
	    " Inc.\n * All rights reserved.\n */\n\n");
	(void) fprintf(fp, "/*\n * parse rules generated from file\n"
	    " * %s\n */\n\n", argv[1]);
	(void) fprintf(fp, "#include \"acpi_bst.h\"\n");
	(void) fprintf(fp, "#include \"acpi_par.h\"\n");
	(void) fprintf(fp, "#include \"acpi_lex.h\"\n");
	(void) fprintf(fp, "#include \"acpi_elem.h\"\n");
	(void) fprintf(fp, "#include \"acpi_act.h\"\n\n");
	emit_rules(fp, list_rules, max_list_rules, "list", "LIST", 0, 0);
	emit_rules(fp, alt_rules, max_alt_rules, "alt", "ALT", 1, 1);
	emit_rules(fp, prod_rules, max_prod_rules, "prod", "PROD", 1, 1);
	emit_rules(fp, lex_rules, max_lex_rules, "lex", "LEX", 0, 0);
	emit_table(fp, list_rules, alt_rules, prod_rules, lex_rules,
	    max_list_rules, max_alt_rules, max_prod_rules, max_lex_rules);
	emit_debug(fp, non_terms, max_non_terms, terms, max_terms);
	(void) fprintf(fp, "\n\n/* eof */\n");
	(void) fclose(fp);

	return (0);
}


/* eof */
