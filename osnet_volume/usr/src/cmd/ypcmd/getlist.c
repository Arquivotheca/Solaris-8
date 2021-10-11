/*
 * Copyright (c) 1986-1992 by Sun Microsystems Inc.
 * All rights reserved.
 */
							    

#include <stdio.h>
#include <stdlib.h>
#include "ypsym.h"

extern void free();
extern char *strdup();

/*
 * Add a name to the list
 */
static listofnames *
newname(str)
char *str;
{
	listofnames *it;
	char *copy;

	if (str == NULL)
		return (NULL);
	copy = strdup(str);
	if (copy == NULL)
		return (NULL);
	it = (listofnames *) malloc(sizeof (listofnames));
	if (it == NULL) {
		free(copy);
		return (NULL);
	}
	it->name = copy;
	it->nextname = NULL;
	return (it);
}

/*
 * Assemble the list of names
 */
listofnames *
names(filename)
char *filename;
{
	listofnames *nameslist;
	listofnames *end;
	listofnames *nname;
	FILE *fyle;
	char line[256];
	char name[256];

	fyle = fopen(filename, "r");
	if (fyle == NULL) {
		return (NULL);
	}
	nameslist = NULL;
	while (fgets(line, sizeof (line), fyle)) {
		if (line[0] == '#') continue;
		if (line[0] == '\0') continue;
		if (line[0] == '\n') continue;
		nname = newname(line);
		if (nname) {
			if (nameslist == NULL) {
					nameslist = nname;
					end = nname;
			} else {
				end->nextname = nname;
				end = nname;
			}
		} else
			fprintf(stderr,
		"file %s bad malloc %s\n", filename, name);
	}
	fclose(fyle);
	return (nameslist);
}

void
free_listofnames(locallist)
listofnames *locallist;
{
	listofnames *next = (listofnames *)NULL;

	for (; locallist; locallist = next) {
		next = locallist->nextname;
		if (locallist->name)
			free(locallist->name);
		free((char *)locallist);
	}
}


#ifdef MAIN
main(argc, argv)
char **argv;
{
	listofnames *list;
	list = names(argv[1]);
#ifdef DEBUG
	print_listofnames(list);
#endif
	free_listofnames(list);
#ifdef DEBUG
	printf("Done\n");
#endif
}
#endif

#ifdef DEBUG
void
print_listofnames(list)
listofnames *list;
{
	if (list == NULL)
		printf("NULL\n");
	for (; list; list = list->nextname)
		printf("%s\n", list->name);
}
#endif
