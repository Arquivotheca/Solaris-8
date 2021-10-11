/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fs_parse_utils.cc	1.3	95/08/05 SMI"


// Parsing-related utilities for fncreate_fs.


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <libintl.h>
#include <rpc/rpc.h>
#include <xfn/xfn.hh>
#include "Tree.hh"
#include "fncreate_fs.hh"


// Add a name/location pair to the input tree.  name and location
// become the property of the tree; they cannot subsequently be used
// or freed by the caller.

static void add_entry(Dir *tree, char *name, char *location);


// Read and parse the next entry to be processed.  Set name and
// location to newly-allocated strings containing the input;
// location contains both the mount options and the mount location
// (eg: "-ro svr1:/export").  Return true if name and location
// are set, otherwise false.  If false, set eof to indicate whether
// the cause was the end of input.

static bool_t parse_next_entry(char *&name, char *&location, bool_t &eof);


// Get a line from the input file (using read_line()) or from the
// command line, as appropriate.  Return a newly allocated string, or
// NULL on EOF.

static char *get_line();


// Read a line of arbitrary length from the input file.  Ignore
// comments, which consist of a '#' at the beginning of a line or
// prefixed by whitespace, through the end of the line.  Treat '\' at
// the end of a (non-commented) line as a continuation marker.  Return
// a newly allocated string, or NULL on EOF.

static char *read_line();


Dir *
parse_input()
{
	FN_string *dot = new FN_string((unsigned char *)".");
	mem_check(dot);
	Dir *root = new Dir(dot);

	char *name;
	char *location;
	bool_t eof;
	bool_t bad_input = FALSE;	// was there any bad input?
	bool_t good;

	while ((good = parse_next_entry(name, location, eof)) || !eof) {
		bad_input |= !good;
		if (verbose >= 2 && good) {
			info("parsed: %s %s", name, location);
		}
		if (good) {
			add_entry(root, name, location);
		}
	}
	if (verbose >= 2) {
		info("");
		root->print();
		info("");
	}
	if (bad_input) {
		// There's no way to (recursively) free root right now.
		// No big deal.
		root = NULL;
	}
	return (root);
}


static void
add_entry(Dir *tree, char *name, char *location)
{
	if (strcmp(name, ".") == 0) {
		delete[] tree->location;
		tree->location = location;
		delete[] name;
		return;
	}

	FN_composite_name *nm;
	if (strncmp(name, "./", 2) == 0) {
		nm = composite_name_from_str(name + 2, TRUE);
	} else {
		nm = composite_name_from_str(name, TRUE);
	}
	delete[] name;

	void *iter;
	const FN_string *last = nm->last(iter);
	const FN_string *atom;
	for (atom = nm->first(iter); atom != NULL; atom = nm->next(iter)) {
		FN_string *copy = new FN_string(*atom);
		mem_check(copy);
		Dir *subtree = new Dir(copy, (atom == last) ? location : NULL);
		mem_check(subtree);
		tree = tree->insert(subtree);
		mem_check(tree);
	}
	delete nm;
}


static bool_t
parse_next_entry(char *&name, char *&location, bool_t &eof)
{
	static char *line = NULL;
	static char *clone;	// copy of line that we are free to munge
	static char *nm;	// start of name ("name" = "nm/offset")
	static char *word;	// next word to process on line

	bool_t line_new;	// true if we read a new line on this call
	line_new = (line == NULL);

	char *offset = "";
	char *loc = concat("");

	// Get a line.

	if (line == NULL) {
		line = get_line();
		if (eof = (line == NULL)) {
			return (FALSE);
		}
		clone = concat(line);

		nm = strtok(clone, " \t");	// start of name
		if (nm == NULL) {		// empty line -- ignore it
			delete[] line;
			delete[] clone;
			line = NULL;
			return (parse_next_entry(name, location, eof));
		}
		if ((nm[0] == '/') ||
		    (nm[0] == '.' && nm[1] != '\0')) {
			goto bad_input;
		}
		word = strtok(NULL, " \t");
	} else {
		eof = FALSE;
	}

	// Parse next entry.

	if (word != NULL && word[0] == '-') {	// options
		delete[] loc;
		loc = concat(word);
		word = strtok(NULL, " \t");
	}
	if (word != NULL && word[0] == '/') {	// offset
		if (loc[0] != '\0' && strcmp(word, "/") != 0) {
			goto have_entry;
		}
		offset = word;
		word = strtok(NULL, " \t");
	}
	if (word != NULL && word[0] == '-') {	// options, may override prior
		delete[] loc;
		loc = concat(word);
		word = strtok(NULL, " \t");
	}
	while (word != NULL && word[0] != '-' && word[0] != '/') {  // location
		char *newloc = (loc[0] == '\0')
				? concat(word)
				: concat(loc, word, ' ');
		delete[] loc;
		loc = newloc;
		word = strtok(NULL, " \t");
	}
	if (word != NULL && word[0] != '/') {
		goto bad_input;
	}

have_entry:
	name = concat(nm, offset);
	location = loc;

	if (word == NULL) {	// end of the line
		delete[] line;
		delete[] clone;
		line = NULL;
	}
	return (TRUE);

bad_input:
	fprintf(stderr, "%s: %s\n", gettext("Bad input"),
		(cmdline_location != NULL) ? cmdline_location : line);
	// Flush rest of line.
	delete[] line;
	delete[] clone;
	delete[] loc;
	line = NULL;
	return (FALSE);
}


static char *
get_line()
{
	static bool_t used_cmdline_location = FALSE;

	if (infile != NULL) {
		return (read_line());
	}
	if (used_cmdline_location) {
		return (NULL);
	}
	used_cmdline_location = TRUE;

	return (concat(". ", cmdline_location));
}


static char *
read_line()
{
	const size_t initial_bufsz = 128;

	if (feof(infile)) {
		return (NULL);
	}

	size_t bufsz = initial_bufsz;
	char *line = new char[bufsz];
	mem_check(line);
	size_t len = 0;
	int c;

	while ((c = getc(infile)) != EOF) {
		if (c == '\n') {
			// Check for '\' at end of line.
			if ((len >= 1) && (line[len - 1] == '\\')) {
				len--;
				continue;
			} else {
				break;
			}
		} else if ((c == '#') &&
			    (len == 0 || isspace(line[len - 1]))) {
			// Skip comments.
			while (((c = getc(infile)) != EOF) && (c != '\n'))
				;
			break;
		}
		if (len + 2 == bufsz) {		// allow for '\0'
			// Build a bigger buffer.
			bufsz *= 2;
			char *newline = new char[bufsz];
			mem_check(newline);
			memcpy(newline, line, len);
			delete[] line;
			line = newline;
		}
		line[len++] = c;
	}
	if ((len == 0) && (c == EOF)) {
		free(line);
		return (NULL);
	}
	line[len] = '\0';
	return (line);
}


// Implementation of class Dir.

Dir::~Dir()
{
	delete name;
	delete[] location;
}

Dir::Dir(FN_string *nm, char *loc)
{
	name = nm;
	location = loc;
}

int
Dir::compare_root(const Tree *tree) const
{
	const Dir *dir = (const Dir *)tree;

	return (this != dir
		? name->compare(*dir->name)
		: 0);
}

void
Dir::assign_root(const Tree *tree)
{
	// This routine is only called when the two names are the same.
	// If that ever changes, uncomment this line.
	// *name = *((Dir *)tree)->name;

	char *newloc = ((Dir *)tree)->location;
	if (newloc != NULL) {
		delete[] location;
		location = new char[strlen(newloc) + 1];
		mem_check(location);
		strcpy(location, newloc);
	}
}

Dir *
Dir::insert(Dir *dir)
{
	return ((Dir *)(Tree::insert(dir)));
}

void
Dir::print(unsigned int depth)
{
	const unsigned int offset = 4;

	printf("%*s%s ", depth * offset, "", str(name));
	if (location != NULL) {
		printf("\"%s\"", location);
	}
	printf("\n");
	Tree *subtree;
	void *iter;
	for (subtree = first(iter); subtree != NULL; subtree = next(iter)) {
		((Dir *)subtree)->print(depth + 1);
	}
}

void
Dir::print_name_hierarchy()
{
	if (parent == NULL) {
		printf("%s", str(name));
	} else {
		((Dir *)parent)->print_name_hierarchy();
		printf("/%s", str(name));
	}
}
