/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)list.c	1.2	99/08/25 SMI"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "list.h"
#include "proto_list.h"

/*
 * clear 'length' bytes at str to zero
 */
void
clear_block(char *str, int length)
{
	if (length < 0)
		return;

	while (length--)
		*(str++) = 0;

}


void
init_list(elem_list *list, int hsize)
{
	int	i;

	list->type = 0;
	list->list = (elem**)malloc(sizeof (elem *) * hsize);
	list->num_of_buckets = hsize;
	for (i = 0; i < list->num_of_buckets; i++)
		list->list[i] = NULL;
}


void
print_elem(FILE *fp, elem *e)
{
	elem		p;
	pkg_list	*l;
	char		maj[TYPESIZE], min[TYPESIZE];

	/*
	 * If this is a LINK to another file, then adopt
	 * the permissions of that file.
	 */
	if (e->link_parent) {
		p = *((elem *)e->link_parent);
		(void) strcpy(p.name, e->name);
		p.symsrc = e->symsrc;
		p.file_type = e->file_type;
		e = &p;
	}

	if (e->major == -1) {
		maj[0] = '-';
		maj[1] = '\0';
	} else
		(void) sprintf(maj, "%d", e->major);

	if (e->minor == -1) {
		min[0] = '-';
		min[1] = '\0';
	} else
		(void) sprintf(min, "%d", e->minor);

	if (!e->symsrc)
		e->symsrc = "-";
	(void) fprintf(fp, "%c %-30s %-20s %4d %-5s %-5s %6d %2d %2s %2s   ",
	    e->file_type, e->name, e->symsrc, e->perm, e->owner, e->group,
	    e->inode, e->ref_cnt, maj, min);
	/*
	 * dump package list - if any.
	 */
	if (!e->pkgs)
		(void) fputs(" proto", fp);

	for (l = e->pkgs; l; l = l->next) {
		(void) fputc(' ', fp);
		(void) fputs(l->pkg_name, fp);
	}
	(void) fputc('\n', fp);
}

#ifdef DEBUG
void
examine_list(elem_list *list)
{
	int	i;
	elem	*cur;
	int	buck_count;

	for (i = 0; i < list->num_of_buckets; i++) {
		buck_count = 0;
		for (cur = list->list[i]; cur; cur = cur->next)
			buck_count++;
		(void) printf("bucket[%4d] contains %5d entries\n",
		    i, buck_count);
	}
}


/*
 * print all elements of a list
 *
 * Debugging routine
 */
void
print_list(elem_list *list)
{
	int	i;
	elem	*cur;

	for (i = 0; i < list->num_of_buckets; i++) {
		for (cur = list->list[i]; cur; cur = cur->next)
			print_elem(stdout, cur);
	}
}


/*
 * print all elements of a list of type 'file_type'
 *
 * Debugging routine
 */
void
print_type_list(elem_list *list, char file_type)
{
	int	i;
	elem	*cur;

	for (i = 0; i < list->num_of_buckets; i++) {
		for (cur = list->list[i]; cur; cur = cur->next) {
			if (cur->file_type == file_type)
				print_elem(stdout, cur);
		}
	}
}
#endif

unsigned int
hash(const char *str)
{
	unsigned int	h = 7;

	for (; *str; str++)
		h *= (int)*str;
	return (h);
}


static int
name_compare(elem *i, elem *j)
{
	int	n;

	if ((n = strncmp(i->name, j->name, MAXNAME)) != 0)
		return (n);
	else
		return (j->arch - i->arch);
}


/*
 * find_elem:
 *
 * possible values for flag.
 * 			flag = FOLLOW_LINK
 *			flag = NO_FOLLOW_LINK
 */
elem *
find_elem(elem_list *list, elem *key, int flag)
{
	elem	*e;

	for (e = list->list[hash(key->name) % list->num_of_buckets]; e;
	    e = e->next) {
		if (!name_compare(e, key))
			if (e->link_parent && flag == FOLLOW_LINK)
				return (e->link_parent);
			else
				return (e);
	}

	return (NULL);
}


/*
 * find_elem_isa:
 *
 * flags - same as find_elem()
 */
elem *
find_elem_isa(elem_list *list, elem *key, int flag)
{
	short	orig_arch;
	elem	*e;

	orig_arch = key->arch;
	key->arch = P_ISA;
	e = find_elem(list, key, flag);
	key->arch = orig_arch;
	return (e);
}

/*
 * find_elem_mach:
 *
 * flags - same as find_elem()
 */
elem *
find_elem_mach(elem_list *list, elem *key, int flag)
{
	elem	*e;

	for (e = list->list[hash(key->name) % list->num_of_buckets]; e;
	    e = e->next) {
		if ((e->arch != P_ISA) && (strcmp(key->name, e->name) == 0))
			if (e->link_parent && flag == FOLLOW_LINK)
				return (e->link_parent);
			else
				return (e);
	}

	return (NULL);
}

pkg_list *
add_pkg(pkg_list *head, const char *pkgname)
{
	pkg_list	*cur, *prev = NULL;
	static pkg_list	*new = NULL;

	if (!new)
		new = (pkg_list *)malloc(sizeof (pkg_list));

	(void) strcpy(new->pkg_name, pkgname);

	for (cur = head; cur; cur = cur->next) {
		if (strcmp(cur->pkg_name, pkgname) >= 0)
			break;
		prev = cur;
	}

	if (!head) {
		new->next = head;
		head = new;
		new = NULL;
		return (head);
	}

	if (!cur) {
		prev->next = new;
		new->next = NULL;
		new = NULL;
		return (head);
	}

	if (strcmp(cur->pkg_name, pkgname) == 0)	/* a duplicate */
		return (NULL);

	if (!prev) {
		new->next = cur;
		cur = new;
		new = NULL;
		return (cur);
	}

	prev->next = new;
	new->next = cur;
	new = NULL;
	return (head);
}

void
add_elem(elem_list *list, elem *e)
{
	elem	*last = NULL;
	elem	*cur;
	int	bucket;

	bucket = hash(e->name) % list->num_of_buckets;
	if (list->list[bucket]) {
		for (cur = list->list[bucket]; cur; cur = cur->next) {
			if (strcmp(cur->name, e->name) > 0)
				break;
			last = cur;
		}

		if (last) {
			last->next = e;
			e->next = cur;
			return;
		}
	}

	/*
	 * insert at head of list
	 */
	e->next = list->list[bucket];
	list->list[bucket] = e;
}

/*
 * elem_compare(a,b)
 *
 * Args:
 *	a 		- element a
 *	b 		- element b
 *	different_types -
 *		value = 0  -> comparing two elements of same
 *			      type (eg: protodir elem vs. protodir elem).
 *		value != 0 -> comparing two elements of different type
 *			      (eg: protodir elem vs. protolist elem).
 *
 * Returns:
 *	0   - elements are identical
 *	>0  - elements differ
 *	      check flags to see which fields differ.
 */
int
elem_compare(elem *a, elem *b, int different_types)
{
	int	res = 0;
	elem	*i, *j;

	/*
	 * if these are hard links to other files - those are the
	 * files that should be compared.
	 */
	i = a->link_parent ? a->link_parent : a;
	j = b->link_parent ? b->link_parent : b;

	/*
	 * We do not compare inodes - they always differ.
	 * We do not compare names because we assume that was
	 * checked before.
	 */

	/*
	 * Special rules for comparison:
	 *
	 * 1) if directory - ignore ref_cnt.
	 * 2) if sym_link - only check file_type & symlink
	 * 3) elem type of FILE_T, EDIT_T, & VOLATILE_T are equivilant when
	 *    comparing a protodir entry to a protolist entry.
	 */
	if (i->file_type != j->file_type) {
		if (different_types) {
			/*
			 * Check to see if filetypes are FILE_T vs.
			 * EDIT_T/VOLATILE_T/LINK_T comparisons.
			 */
			if ((i->file_type == FILE_T) &&
			    ((j->file_type == EDIT_T) ||
			    (j->file_type == VOLATILE_T) ||
			    (j->file_type == LINK_T))) {
				/*EMPTY*/
			} else if ((j->file_type == FILE_T) &&
			    ((i->file_type == EDIT_T) ||
			    (i->file_type == VOLATILE_T) ||
			    (i->file_type == LINK_T))) {
				/*EMPTY*/
			} else
				res |= TYPE_F;
		} else
			res |= TYPE_F;
	}

	/*
	 * if symlink - check the symlink value and then
	 * return.  symlink is the only field of concern
	 * in SYMLINKS.
	 */
	if (check_sym && ((res == 0) && (i->file_type == SYM_LINK_T))) {
		if ((!i->symsrc) || (!j->symsrc))
			res |= SYM_F;
		else {
			/*
			 * if either symlink starts with a './' strip it off,
			 * its irrelavant.
			 */
			if ((i->symsrc[0] == '.') && (i->symsrc[1] == '/'))
				i->symsrc += 2;
			if ((j->symsrc[0] == '.') && (j->symsrc[1] == '/'))
				j->symsrc += 2;

			if (strncmp(i->symsrc, j->symsrc, MAXNAME) != 0)
				res |= SYM_F;
		}
		return (res);
	}

	if ((i->file_type != DIR_T) && check_link &&
	    (i->ref_cnt != j->ref_cnt))
		res |= REF_F;
	if (check_user && (strncmp(i->owner, j->owner, TYPESIZE) != 0))
		res |= OWNER_F;
	if (check_group && (strncmp(i->group, j->group, TYPESIZE) != 0))
		res |= GROUP_F;
	if (check_perm && (i->perm != j->perm))
		res |= PERM_F;
	if (check_majmin && ((i->major != j->major) || (i->minor != j->minor)))
		res |= MAJMIN_F;

	return (res);
}
