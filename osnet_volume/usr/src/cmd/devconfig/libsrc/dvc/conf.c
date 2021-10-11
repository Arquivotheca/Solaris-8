/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)conf.c 1.8 99/03/26 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <libintl.h>

#include "dvc.h"
#include "dvc_msgs.h"
#include "conf.h"

static char *
suck_in_file(FILE *fp)
{
	char *file = 0;
	size_t size;

	if (fseek(fp, 0, SEEK_END))
		return (NULL);

	size = (size_t)ftell(fp);
	if (fseek(fp, 0, SEEK_SET))
		return (NULL);

	file = (char *)xmalloc(size+1);
	if (file == NULL)
		return (NULL);

	if (size && (fread(file, size, 1, fp) == 0)) {
		xfree(file);
		return (NULL);
	}

	file[size] = '\0';

	return (file);
}

conf_list_t *
read_conf(FILE *fp)
{
	char *file;
	conf_list_t *cf;

	file = suck_in_file(fp);
	if (file == NULL)
		return (NULL);

	cf = parse_conf(file);
	xfree(file);
	return (cf);
}

static void
printval(FILE *fp, val_list_t *vp)
{
	switch (vp->val_type) {
	case VAL_NUMERIC:  /* should get rid of check now with VAL_UNUMERIC */
		if ((vp->val.integer >= 0) && (vp->val.integer < 16))
			fprintf(fp, "%d", vp->val.integer);
		else
			fprintf(fp, "0x%X", (unsigned int) vp->val.integer);
		break;

	case VAL_UNUMERIC:
		if (vp->val.uinteger < 16)
			fprintf(fp, "%u", vp->val.uinteger);
		else
			fprintf(fp, "0x%X", vp->val.uinteger);
		break;

	case VAL_STRING:
		fprintf(fp, "\"%s\"", vp->val.string);
		break;
	}
}

static FILE *write_fp = stdout;

static void
write_val(val_list_t *vp)
{
	if (vp->next)
		write_val(vp->next), fprintf(write_fp, ",");
	printval(write_fp, vp);
}

static void
write_attr(struct attr_list *at)
{
	static char *re;

	if (at->next)
		write_attr(at->next), fprintf(write_fp, " ");

	/*
	 * Filter out all attributes that start and end
	 * with double underscores.
	 */
	if (re == NULL)
		re = regcmp("^__.*__$", NULL);
	if (regex(re, at->name))
		return;

	fprintf(write_fp, "%s=", at->name);
	if (at->vlist)
		write_val(at->vlist);
}

static int found;

static void
wr_conf(conf_list_t *cf)
{
	if (cf->next)
		wr_conf(cf->next);
	if (cf->alist)
		write_attr(cf->alist);
	fprintf(write_fp, ";\n");
	++found;
}

void
write_conf(FILE *fp, conf_list_t *cf)
{
	write_fp = fp;
	found = 0;
	if (cf)
		wr_conf(cf);
}

conf_list_t *
dup_conf(conf_list_t *cf)
{
	conf_list_t *new = (conf_list_t *)xzmalloc(sizeof (conf_list_t));

	new->alist = dup_alist(cf->alist);

	return (new);
}

void
free_conf(conf_list_t *cf)
{
	if (cf) {
		if (cf->next)
			free_conf(cf->next);
		free_attr(cf->alist);
		xfree(cf);
	}
}

#ifdef CONF_TEST_FRAME

void
ui_notice(char *text)
{
	fprintf(stderr, "NOTICE: ");
	fprintf(stderr, text);
	fflush(stderr);
}

void
ui_error_exit(char *text)
{
	fprintf(stderr, "ERROR: ");
	fprintf(stderr, text);
	fflush(stderr);
	exit(1);
}

void
write_comments(FILE *fp, char *file)
{
	while (*file == '#' || *file == '\n') {
		do {
			if (*file == '\0')
				return;
			fputc(*file, fp);
		} while (*file++ != '\n');
	}
}

static void
read_write_conf(char *filename)
{
	FILE *fp;

	struct conf_list *cf = NULL;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		vrb(MSG(READERR), filename);
		return;
	}

	cf = read_conf(fp);
	fclose(fp);

	fp = fopen(filename, "w");
	if (fp == NULL) {
		vrb(MSG(CANTOPEN), filename);
		return;
	}

	{
		char *file = suck_in_file(fp);
		if (file != NULL)
			write_comments(fp, file);
		xfree(file);
	}

	write_conf(fp, cf);
	fclose(fp);
	free_conf(cf);
}


int
main(int ac, char *av[])
{
	int  i;
	char opt;
	extern int dvc_verbose;

	while ((opt = getopt(ac, av, "v")) != -1) {
		switch (opt) {
		case 'v':
			++dvc_verbose;
			break;

		case '?':
			fprintf(stderr, "Usage: parse [-v] [filenames]\n");
			return (1);
		}
	}

	for (i = optind; i < ac; ++i)
		read_write_conf(av[i]);

	return (0);
}

#endif /* CONF_TEST_FRAME */
