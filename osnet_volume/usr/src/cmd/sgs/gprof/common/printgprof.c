/*
 * Copyright (c) 1990-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)printgprof.c	1.15	98/06/02 SMI"

#include <ctype.h>
#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include "gprof.h"

extern int find_run_directory(char *, char *, char *, char **, char *);
void print_demangled_name(int, nltype *);
void striped_name(char *, nltype **);


extern char *demangle();
char *strstr();
char *parsename();
char name_buffer[512];
extern long hz;

/*
 * Symbols that must never be printed, no matter what.
 */
char *splsym[] = {
	PRF_ETEXT,
	PRF_EXTSYM,
	PRF_MEMTERM,
	0
};

char *
demangled_name(nltype *selfp)
{
	char *name;
	if (!Cflag)
		return (selfp->name);

	name = (char *) sgs_demangle(selfp->name);
	return (name);
}

void
printprof()
{
	nltype	*np;
	nltype	**sortednlp;
	int	i, index;
	int 	print_count = number_funcs_toprint;
	bool	print_flag = TRUE;
	mod_info_t	*mi;

	actime = 0.0;
	printf("\f\n");
	flatprofheader();

	/*
	 *	Sort the symbol table in by time
	 */
	sortednlp = (nltype **) calloc(total_names, sizeof (nltype *));
	if (sortednlp == (nltype **) 0) {
		fprintf(stderr,
		    "[printprof] ran out of memory for time sorting\n");
	}

	index = 0;
	for (mi = &modules; mi; mi = mi->next) {
		for (i = 0; i < mi->nname; i++)
			sortednlp[index++] = &(mi->nl[i]);
	}

	qsort(sortednlp, total_names, sizeof (nltype *),
		(int(*)(const void *, const void *))timecmp);

	for (index = 0; (index < total_names) && print_flag; index += 1) {
		np = sortednlp[index];
		flatprofline(np);
		if (nflag) {
			if (--print_count == 0)
				print_flag = FALSE;
		}
	}
	actime = 0.0;
	free(sortednlp);
}

int
timecmp(nltype **npp1, nltype **npp2)
{
	double	timediff;
	long	calldiff;

	timediff = (*npp2)->time - (*npp1)->time;

	if (timediff > 0.0)
		return (1);

	if (timediff < 0.0)
		return (-1);

	calldiff = (*npp2)->ncall - (*npp1)->ncall;

	if (calldiff > 0)
		return (1);

	if (calldiff < 0)
		return (-1);

	return (strcmp((*npp1)->name, (*npp2)->name));
}

/*
 *	header for flatprofline
 */
void
flatprofheader()
{

	if (bflag)
		printblurb(FLAT_BLURB);

	if (old_style) {
		printf("\ngranularity: each sample hit covers %d byte(s)",
					    (long)scale * sizeof (UNIT));
		if (totime > 0.0) {
			printf(" for %.2f%% of %.2f seconds\n\n",
			    100.0/totime, totime / hz);
		} else {
			printf(" no time accumulated\n\n");
			/*
			 * this doesn't hurt since all the numerators will
			 * be zero.
			 */
			totime = 1.0;
		}
	}

	printf("%5.5s %10.10s %8.8s %8.8s %8.8s %8.8s %-8.8s\n",
	    "% ", "cumulative", "self ", "", "self ", "total ", "");
	printf("%5.5s %10.10s %8.8s %8.8s %8.8s %8.8s %-8.8s\n",
	    "time", "seconds ", "seconds", "calls",
	    "ms/call", "ms/call", "name");
}

void
flatprofline(nltype *np)
{
	if (zflag == 0 && np->ncall == 0 && np->time == 0)
		return;

	/*
	 * Do not print certain special symbols, like PRF_EXTSYM, etc.
	 * even if zflag was on.
	 */
	if (is_special_sym(np))
		return;

	actime += np->time;

	printf("%5.1f %10.2f %8.2f",
	    100 * np->time / totime, actime / hz, np->time / hz);

	if (np->ncall != 0) {
		printf(" %8lld %8.2f %8.2f  ", np->ncall,
		    1000 * np->time / hz / np->ncall,
		    1000 * (np->time + np->childtime) / hz / np->ncall);
	} else {
		if (!Cflag)
			printf(" %8.8s %8.8s %8.8s ", "", "", "");
		else
			printf(" %8.8s %8.8s %8.8s  ", "", "", "");
	}

	printname(np);

	if (Cflag)
		print_demangled_name(55, np);

	printf("\n");
}

void
gprofheader()
{

	if (bflag)
		printblurb(CALLG_BLURB);

	if (old_style) {

		printf("\ngranularity: each sample hit covers %d byte(s)",
						(long)scale * sizeof (UNIT));

		if (printtime > 0.0) {
			printf(" for %.2f%% of %.2f seconds\n\n",
			    100.0/printtime, printtime / hz);
		} else {
			printf(" no time propagated\n\n");
			/*
			 * this doesn't hurt, since all the numerators
			 * will be 0.0
			 */
			printtime = 1.0;
		}
	} else {
		printf("\ngranularity: each pc-hit is considered 1 tick");
		if (hz != 1) {
			printf(" (@ %4.3f seconds per tick)",
							(double) 1.0 / hz);
		}
		puts("\n\n");
	}

	printf("%6.6s %5.5s %7.7s %11.11s %7.7s/%-7.7s     %-8.8s\n",
	    "", "", "", "", "called", "total", "parents");
	printf("%-6.6s %5.5s %7.7s %11.11s %7.7s+%-7.7s %-8.8s\t%5.5s\n",
	    "index", "%time", "self", "descendents",
	    "called", "self", "name", "index");
	printf("%6.6s %5.5s %7.7s %11.11s %7.7s/%-7.7s     %-8.8s\n",
	    "", "", "", "", "called", "total", "children");
	printf("\n");
}

void
gprofline(nltype *np)
{
	char	kirkbuffer[BUFSIZ];

	sprintf(kirkbuffer, "[%d]", np->index);
	printf("%-6.6s %5.1f %7.2f %11.2f", kirkbuffer,
	    100 * (np->propself + np->propchild) / printtime,
	    np->propself / hz, np->propchild / hz);

	if ((np->ncall + np->selfcalls) != 0) {
		printf(" %7lld", np->ncall);

		if (np->selfcalls != 0)
			printf("+%-7lld ", np->selfcalls);
		else
			printf(" %7.7s ", "");
	} else {
		printf(" %7.7s %7.7s ", "", "");
	}

	printname(np);

	if (Cflag)
		print_demangled_name(50, np);

	printf("\n");
}

static bool
is_special_sym(nltype *nlp)
{
	int	i;

	if (nlp->name == NULL)
		return (FALSE);

	for (i = 0;  splsym[i]; i++)
		if (strcmp(splsym[i], nlp->name) == 0)
			return (TRUE);

	return (FALSE);
}

void
printgprof(nltype **timesortnlp)
{
	int	index;
	nltype	*parentp;
	int 	print_count = number_funcs_toprint;
	bool	count_flag = TRUE;

	/*
	 * Print out the structured profiling list
	 */
	gprofheader();

	for (index = 0; index < total_names + ncycle && count_flag; index++) {
		parentp = timesortnlp[index];
		if (zflag == 0 && parentp->ncall == 0 &&
		    parentp->selfcalls == 0 && parentp->propself == 0 &&
		    parentp -> propchild == 0)
			continue;

		if (!parentp->printflag)
			continue;

		/*
		 * Do not print certain special symbols, like PRF_EXTSYM, etc.
		 * even if zflag was on.
		 */
		if (is_special_sym(parentp))
			continue;

		if (parentp->name == 0 && parentp->cycleno != 0) {
			/*
			 *	cycle header
			 */
			printcycle(parentp);
			printmembers(parentp);
		} else {
			printparents(parentp);
			gprofline(parentp);
			printchildren(parentp);
		}

		printf("\n");
		printf("-----------------------------------------------\n");
		printf("\n");

		if (nflag) {
			--print_count;
			if (print_count == 0)
				count_flag = FALSE;
		}
	}
	free(timesortnlp);
}

/*
 *	sort by decreasing propagated time
 *	if times are equal, but one is a cycle header,
 *		say that's first (e.g. less, i.e. -1).
 *	if one's name doesn't have an underscore and the other does,
 *		say the one is first.
 *	all else being equal, sort by names.
 */
int
totalcmp(nltype **npp1, nltype **npp2)
{
	nltype	*np1 = *npp1;
	nltype	*np2 = *npp2;
	double	diff;

	diff = (np1->propself + np1->propchild) -
	    (np2->propself + np2->propchild);

	if (diff < 0.0)
		return (1);
	if (diff > 0.0)
		return (-1);
	if (np1->name == 0 && np1->cycleno != 0)
		return (-1);
	if (np2->name == 0 && np2->cycleno != 0)
		return (1);
	if (np1->name == 0)
		return (-1);
	if (np2->name == 0)
		return (1);

	if (*(np1->name) != '_' && *(np2->name) == '_')
		return (-1);
	if (*(np1->name) == '_' && *(np2->name) != '_')
		return (1);
	if (np1->ncall > np2->ncall)
		return (-1);
	if (np1->ncall < np2->ncall)
		return (1);
	return (strcmp(np1->name, np2->name));
}

void
printparents(nltype *childp)
{
	nltype	*parentp;
	arctype	*arcp;
	nltype	*cycleheadp;

	if (childp->cyclehead != 0)
		cycleheadp = childp -> cyclehead;
	else
		cycleheadp = childp;

	if (childp->parents == 0) {
		printf("%6.6s %5.5s %7.7s %11.11s %7.7s %7.7s"
		    "     <spontaneous>\n", "", "", "", "", "", "");
		return;
	}

	sortparents(childp);

	for (arcp = childp->parents; arcp; arcp = arcp->arc_parentlist) {
		parentp = arcp -> arc_parentp;
		if (childp == parentp || (childp->cycleno != 0 &&
		    parentp->cycleno == childp->cycleno)) {
			/*
			 *	selfcall or call among siblings
			 */
			printf("%6.6s %5.5s %7.7s %11.11s %7lld %7.7s     ",
			    "", "", "", "", arcp->arc_count, "");
			printname(parentp);

			if (Cflag)
				print_demangled_name(54, parentp);

			printf("\n");
		} else {
			/*
			 *	regular parent of child
			 */
			printf("%6.6s %5.5s %7.2f %11.2f %7lld/%-7lld     ", "",
			    "", arcp->arc_time / hz, arcp->arc_childtime / hz,
			    arcp->arc_count, cycleheadp->ncall);
			printname(parentp);

			if (Cflag)
				print_demangled_name(54, parentp);

			printf("\n");
		}
	}
}

void
printchildren(nltype *parentp)
{
	nltype	*childp;
	arctype	*arcp;

	sortchildren(parentp);

	for (arcp = parentp->children; arcp; arcp = arcp->arc_childlist) {
		childp = arcp->arc_childp;
		if (childp == parentp || (childp->cycleno != 0 &&
		    childp->cycleno == parentp->cycleno)) {
			/*
			 * self call or call to sibling
			 */
			printf("%6.6s %5.5s %7.7s %11.11s %7lld %7.7s     ",
			    "", "", "", "", arcp->arc_count, "");
			printname(childp);

			if (Cflag)
				print_demangled_name(54, childp);

			printf("\n");
		} else {
			/*
			 *	regular child of parent
			 */
			if (childp->cyclehead)
				printf("%6.6s %5.5s %7.2f %11.2f "
				    "%7lld/%-7lld     ", "", "",
				    arcp->arc_time / hz,
				    arcp->arc_childtime / hz, arcp->arc_count,
				    childp->cyclehead->ncall);
			else
				printf("%6.6s %5.5s %7.2f %11.2f "
				    "%7lld %7.7s    ",
				    "", "", arcp->arc_time / hz,
				    arcp->arc_childtime / hz, arcp->arc_count,
				    "");

			printname(childp);

			if (Cflag)
				print_demangled_name(54, childp);

			printf("\n");
		}
	}
}

void
printname(nltype *selfp)
{
	char  *c;
	c = demangled_name(selfp);

	if (selfp->name != 0) {
		if (!Cflag)
			printf("%s", selfp->name);
		else
			printf("%s", c);

#ifdef DEBUG
		if (debug & DFNDEBUG)
			printf("{%d} ", selfp->toporder);

		if (debug & PROPDEBUG)
			printf("%5.2f%% ", selfp->propfraction);
#endif DEBUG
	}

	if (selfp->cycleno != 0)
		printf("\t<cycle %d>", selfp->cycleno);

	if (selfp->index != 0) {
		if (selfp->printflag)
			printf(" [%d]", selfp->index);
		else
			printf(" (%d)", selfp->index);
	}
}

void
print_demangled_name(int n, nltype *selfp)
{
	char *c;
	int i;

	c = selfp->name;

	if (strcmp(c, demangled_name(selfp)) == 0)
		return;
	else {
		printf("\n");
		for (i = 1; i < n; i++)
			printf(" ");
		printf("[%s]", selfp->name);
	}
}

char *exotic();

void
sortchildren(nltype *parentp)
{
	arctype	*arcp;
	arctype	*detachedp;
	arctype	sorted;
	arctype	*prevp;

	/*
	 *	unlink children from parent,
	 *	then insertion sort back on to sorted's children.
	 *	    *arcp	the arc you have detached and are inserting.
	 *	    *detachedp	the rest of the arcs to be sorted.
	 *	    sorted	arc list onto which you insertion sort.
	 *	    *prevp	arc before the arc you are comparing.
	 */
	sorted.arc_childlist = 0;

	for ((arcp = parentp->children) && (detachedp = arcp->arc_childlist);
	    arcp;
	    (arcp = detachedp) && (detachedp = detachedp->arc_childlist)) {
		/*
		 *	consider *arcp as disconnected
		 *	insert it into sorted
		 */
		for (prevp = &sorted; prevp->arc_childlist;
		    prevp = prevp->arc_childlist) {
			if (arccmp(arcp, prevp->arc_childlist) != LESSTHAN)
				break;
		}

		arcp->arc_childlist = prevp->arc_childlist;
		prevp->arc_childlist = arcp;
	}

	/*
	 *	reattach sorted children to parent
	 */
	parentp->children = sorted.arc_childlist;
}

void
sortparents(nltype *childp)
{
	arctype	*arcp;
	arctype	*detachedp;
	arctype	sorted;
	arctype	*prevp;

	/*
	 *	unlink parents from child,
	 *	then insertion sort back on to sorted's parents.
	 *	    *arcp	the arc you have detached and are inserting.
	 *	    *detachedp	the rest of the arcs to be sorted.
	 *	    sorted	arc list onto which you insertion sort.
	 *	    *prevp	arc before the arc you are comparing.
	 */
	sorted.arc_parentlist = 0;

	for ((arcp = childp->parents) && (detachedp = arcp->arc_parentlist);
	    arcp;
	    (arcp = detachedp) && (detachedp = detachedp->arc_parentlist)) {
		/*
		 *	consider *arcp as disconnected
		 *	insert it into sorted
		 */
		for (prevp = &sorted; prevp->arc_parentlist;
		    prevp = prevp->arc_parentlist) {
			if (arccmp(arcp, prevp->arc_parentlist) != GREATERTHAN)
				break;
		}
		arcp->arc_parentlist = prevp->arc_parentlist;
		prevp->arc_parentlist = arcp;
	}

	/*
	 *	reattach sorted arcs to child
	 */
	childp->parents = sorted.arc_parentlist;
}

void
printcycle(nltype *cyclep)
{
	char	kirkbuffer[BUFSIZ];

	sprintf(kirkbuffer, "[%d]", cyclep->index);
	printf("%-6.6s %5.1f %7.2f %11.2f %7lld", kirkbuffer,
	    100 * (cyclep->propself + cyclep->propchild) / printtime,
	    cyclep -> propself / hz, cyclep -> propchild / hz,
	    cyclep -> ncall);

	if (cyclep->selfcalls != 0)
		printf("+%-7lld", cyclep->selfcalls);
	else
		printf(" %7.7s", "");

	printf(" <cycle %d as a whole>\t[%d]\n", cyclep->cycleno,
	    cyclep->index);
}

/*
 *	print the members of a cycle
 */
void
printmembers(nltype *cyclep)
{
	nltype	*memberp;

	sortmembers(cyclep);

	for (memberp = cyclep->cnext; memberp; memberp = memberp->cnext) {
		printf("%6.6s %5.5s %7.2f %11.2f %7lld", "", "",
		    memberp->propself / hz, memberp->propchild / hz,
		    memberp->ncall);

		if (memberp->selfcalls != 0)
			printf("+%-7lld", memberp->selfcalls);
		else
			printf(" %7.7s", "");

		printf("     ");
		printname(memberp);
		if (Cflag)
			print_demangled_name(54, memberp);
		printf("\n");
	}
}

/*
 * sort members of a cycle
 */
void
sortmembers(nltype *cyclep)
{
	nltype	*todo;
	nltype	*doing;
	nltype	*prev;

	/*
	 *	detach cycle members from cyclehead,
	 *	and insertion sort them back on.
	 */
	todo = cyclep->cnext;
	cyclep->cnext = 0;

	for ((doing = todo) && (todo = doing->cnext);
	    doing; (doing = todo) && (todo = doing->cnext)) {
		for (prev = cyclep; prev->cnext; prev = prev->cnext) {
			if (membercmp(doing, prev->cnext) == GREATERTHAN)
				break;
		}
		doing->cnext = prev->cnext;
		prev->cnext = doing;
	}
}

/*
 *	major sort is on propself + propchild,
 *	next is sort on ncalls + selfcalls.
 */
int
membercmp(nltype *this, nltype *that)
{
	double	thistime = this->propself + this->propchild;
	double	thattime = that->propself + that->propchild;
	actype	thiscalls = this->ncall + this->selfcalls;
	actype	thatcalls = that->ncall + that->selfcalls;

	if (thistime > thattime)
		return (GREATERTHAN);

	if (thistime < thattime)
		return (LESSTHAN);

	if (thiscalls > thatcalls)
		return (GREATERTHAN);

	if (thiscalls < thatcalls)
		return (LESSTHAN);

	return (EQUALTO);
}

/*
 *	compare two arcs to/from the same child/parent.
 *	- if one arc is a self arc, it's least.
 *	- if one arc is within a cycle, it's less than.
 *	- if both arcs are within a cycle, compare arc counts.
 *	- if neither arc is within a cycle, compare with
 *		arc_time + arc_childtime as major key
 *		arc count as minor key
 */
int
arccmp(arctype *thisp, arctype *thatp)
{
	nltype	*thisparentp = thisp->arc_parentp;
	nltype	*thischildp = thisp->arc_childp;
	nltype	*thatparentp = thatp->arc_parentp;
	nltype	*thatchildp = thatp->arc_childp;
	double	thistime;
	double	thattime;

#ifdef DEBUG
	if (debug & TIMEDEBUG) {
		printf("[arccmp] ");
		printname(thisparentp);
		printf(" calls ");
		printname(thischildp);
		printf(" %f + %f %lld/%lld\n", thisp->arc_time,
		    thisp->arc_childtime, thisp->arc_count,
		    thischildp->ncall);
		printf("[arccmp] ");
		printname(thatparentp);
		printf(" calls ");
		printname(thatchildp);
		printf(" %f + %f %lld/%lld\n", thatp->arc_time,
		    thatp->arc_childtime, thatp->arc_count,
		    thatchildp->ncall);
		printf("\n");
	}
#endif DEBUG

	if (thisparentp == thischildp) {
		/*
		 * this is a self call
		 */
		return (LESSTHAN);
	}

	if (thatparentp == thatchildp) {
		/*
		 * that is a self call
		 */
		return (GREATERTHAN);
	}

	if (thisparentp->cycleno != 0 && thischildp->cycleno != 0 &&
	    thisparentp->cycleno == thischildp->cycleno) {
		/*
		 * this is a call within a cycle
		 */
		if (thatparentp->cycleno != 0 && thatchildp->cycleno != 0 &&
		    thatparentp->cycleno == thatchildp->cycleno) {
			/*
			 * that is a call within the cycle, too
			 */
			if (thisp->arc_count < thatp->arc_count)
				return (LESSTHAN);

			if (thisp->arc_count > thatp->arc_count)
				return (GREATERTHAN);

			return (EQUALTO);
		} else {
			/*
			 * that isn't a call within the cycle
			 */
			return (LESSTHAN);
		}
	} else {
		/*
		 * this isn't a call within a cycle
		 */
		if (thatparentp->cycleno != 0 && thatchildp->cycleno != 0 &&
		    thatparentp->cycleno == thatchildp->cycleno) {
			/*
			 * that is a call within a cycle
			 */
			return (GREATERTHAN);
		} else {
			/*
			 * neither is a call within a cycle
			 */
			thistime = thisp->arc_time + thisp->arc_childtime;
			thattime = thatp->arc_time + thatp->arc_childtime;

			if (thistime < thattime)
				return (LESSTHAN);

			if (thistime > thattime)
				return (GREATERTHAN);

			if (thisp->arc_count < thatp->arc_count)
				return (LESSTHAN);

			if (thisp->arc_count > thatp->arc_count)
				return (GREATERTHAN);

			return (EQUALTO);
		}
	}
}

void
printblurb(char *blurbname)
{
	FILE	*blurbfile;
	int	input;
	char	blurb_directory[MAXPATHLEN];
	char	cwd[MAXPATHLEN];

	cwd[0] = '.';
	cwd[1] = '\0';

	if (find_run_directory(prog_name, cwd, blurb_directory,
	    NULL, getenv("PATH")) != 0) {
		(void) fprintf(stderr, "Error in finding run directory.");
		return;
	} else {
		strcat(blurb_directory, blurbname);
	}

	blurbfile = fopen(blurb_directory, "r");
	if (blurbfile == NULL) {
		perror(blurb_directory);
		return;
	}

	while ((input = getc(blurbfile)) != EOF)
		putchar(input);

	fclose(blurbfile);
}

char *s1, *s2;

int
namecmp(nltype **npp1, nltype **npp2)
{
	if (!Cflag)
		return (strcmp((*npp1)->name, (*npp2)->name));
	else {
		striped_name(s1, npp1);
		striped_name(s2, npp2);
		return (strcmp(s1, s2));
	}
}

void
striped_name(char *s, nltype **npp)
{
	char *d, *c;

	c = (char *)s;
	d = demangled_name(*npp);

	while ((*d != '(') && (*d != '\0')) {
		if (*d != ':')
			*c++ = *d++;
		else
			d++;
	}
	*c = '\0';
}

/*
 * Checks if the current symbol name is the same as its neighbour and
 * returns TRUE if it is.
 */
static bool
does_clash(nltype **nlp, int ndx, int nnames)
{
	/*
	 * same as previous (if there's one) ?
	 */
	if (ndx && (strcmp(nlp[ndx]->name, nlp[ndx-1]->name) == 0))
		return (TRUE);

	/*
	 * same as next (if there's one) ?
	 */
	if ((ndx < (nnames - 1)) &&
			    (strcmp(nlp[ndx]->name, nlp[ndx+1]->name) == 0)) {
		return (TRUE);
	}

	return (FALSE);
}

void
printmodules()
{
	mod_info_t	*mi;

	printf("\f\nObject modules\n\n");
	for (mi = &modules; mi; mi = mi->next)
		printf(" %d: %s\n", mi->id, mi->name);
}

#define	IDFMT(id)	((id) < 10 ? 1 : 2)
#define	NMFMT(id)	((id) < 10 ? 17 : 16)

void
printindex()
{
	nltype	**namesortnlp;
	nltype	*nlp;
	int	index, nnames, todo, i, j;
	char	peterbuffer[BUFSIZ];
	mod_info_t	*mi;

	/*
	 *	Now, sort regular function name alphabetically
	 *	to create an index.
	 */
	namesortnlp = calloc(total_names + ncycle, sizeof (nltype *));

	if (namesortnlp == NULL)
		fprintf(stderr, "%s: ran out of memory for sorting\n", whoami);

	nnames = 0;
	for (mi = &modules; mi; mi = mi->next) {
		for (index = 0; index < mi->nname; index++) {
			if (zflag == 0 && (mi->nl[index]).ncall == 0 &&
						(mi->nl[index]).time == 0) {
				continue;
			}

			/*
			 * Do not print certain special symbols, like
			 * PRF_EXTSYM, etc. even if zflag was on.
			 */
			if (is_special_sym(&(mi->nl[index])))
				continue;

			namesortnlp[nnames++] = &(mi->nl[index]);
		}
	}

	if (Cflag) {
		s1 = malloc(500 * sizeof (char));
		s2 = malloc(500 * sizeof (char));
	}

	qsort(namesortnlp, nnames, sizeof (nltype *),
	    (int(*)(const void *, const void *))namecmp);

	for (index = 1, todo = nnames; index <= ncycle; index++)
		namesortnlp[todo++] = &cyclenl[index];

	printf("\f\nIndex by function name\n\n");

	if (!Cflag)
		index = (todo + 2) / 3;
	else
		index = todo;

	for (i = 0; i < index; i++) {
		if (!Cflag) {
			for (j = i; j < todo; j += index) {
				nlp = namesortnlp[j];

				if (nlp->printflag) {
					sprintf(peterbuffer,
					    "[%d]", nlp->index);
				} else {
					sprintf(peterbuffer,
					    "(%d)", nlp->index);
				}

				if (j < nnames) {
					if (does_clash(namesortnlp,
								j, nnames)) {
						printf("%6.6s %*d:%-*.*s",
							peterbuffer,
							IDFMT(nlp->module->id),
							nlp->module->id,
							NMFMT(nlp->module->id),
							NMFMT(nlp->module->id),
							nlp->name);
					} else {
						printf("%6.6s %-19.19s",
						    peterbuffer, nlp->name);
					}
				} else {
					printf("%6.6s ", peterbuffer);
					sprintf(peterbuffer,
					    "<cycle %d>", nlp->cycleno);
					printf("%-19.19s", peterbuffer);
				}
			}
		} else {
			nlp = namesortnlp[i];

			if (nlp->printflag)
				sprintf(peterbuffer, "[%d]", nlp->index);
			else
				sprintf(peterbuffer, "(%d)", nlp->index);

			if (i < nnames) {
				char *d = demangled_name(nlp);

				if (does_clash(namesortnlp, i, nnames)) {
					printf("%6.6s %d:%s\n", peterbuffer,
							nlp->module->id, d);
				} else
					printf("%6.6s %s\n", peterbuffer, d);

				if (d != nlp->name)
					printf("%6.6s   [%s]", "", nlp->name);
			} else {
				printf("%6.6s ", peterbuffer);
				sprintf(peterbuffer, "<cycle %d>",
				    nlp->cycleno);
				printf("%-33.33s", peterbuffer);
			}
		}
		printf("\n");
	}
	free(namesortnlp);
}


char dname[500];

char *
exotic(char *s)
{
	char *name;
	int i = 0, j;
	char *p, *s1 = "static constructor function for ";

	name = malloc(500 * sizeof (char));

	if (strncmp(s, "__sti__", 7) == 0) {
		i = 0;
		s += 7;

		if ((p = strstr(s, "_c_")) == NULL) {
			if ((p = strstr(s, "_C_")) == NULL) {
				if ((p = strstr(s, "_cc_")) == NULL) {
					if ((p = strstr(s, "_cxx_")) == NULL) {
						if ((p =
						    strstr(s, "_h_")) == NULL)
							return (NULL);
					}
				}
			}
		} else {
			p += 3;
			*p = '\0';
		}

		for (i = 0; s1[i] != '\0'; i++)
			dname[i] = s1[i];
		j = i;

		for (i = 0; s[i] != '\0'; i++)
			dname[j + i] = s[i];
		dname[j + i] = '\0';

		free(name);
		return (dname);
	}

	if (strncmp(s, "__std__", 7) == 0) {
		char *s1 = "static destructor function for ";
		i = 0;
		s += 7;

		if ((p = strstr(s, "_c_")) == NULL) {
			if ((p = strstr(s, "_C_")) == NULL) {
				if ((p = strstr(s, "_cc_")) == NULL) {
					if ((p = strstr(s, "_cxx_")) == NULL) {
						if ((p =
						    strstr(s, "_h_")) == NULL)
							return (NULL);
					}
				}
			}
		} else {
			p += 3;
			*p = '\0';
		}

		for (i = 0; s1[i] != '\0'; i++)
			dname[i] = s1[i];
		j = i;

		for (i = 0; s[i] != '\0'; i++)
			dname[j + i] = s[i];
		dname[j + i] = '\0';

		free(name);
		return (dname);
	}

	if (strncmp(s, "__vtbl__", 8) == 0) {
		char *s1 = "virtual table for ";
		char *printname, *return_p = dname;

		s += 8;
		printname = parsename(s);
		return_p = '\0';
		strcat(return_p, s1);
		strcat(return_p, printname);

		free(name);
		return (dname);
	}

	if (strncmp(s, "__ptbl__", 8) == 0) {
		char *s1 = "pointer to the virtual table for ";
		char *printname, *return_p = dname;

		s += 8;
		printname = parsename(s);
		return_p = '\0';
		strcat(return_p, s1);
		strcat(return_p, printname);

		free(name);
		return (return_p);
	}

	free(name);
	return (s);
}

char *
parsename(char *s)
{
	char *d = name_buffer;
	int len;
	char c_init;
	char *len_pointer = s;

	*d = '\0';

	strcat(d, "class ");

	while (isdigit(*s))
		s++;
	c_init = *s;
	*s = '\0';

	len = atoi(len_pointer);
	*s = c_init;

	/*
	 * only one class name
	 */
	if (*(s + len) == '\0') {
		strcat(d, s);
		return (d);
	} else {
		/*
		 * two classname  %drootname__%dchildname
		 */
		char *child;
		char *root;
		int child_len;
		char *child_len_p;
		root = s;
		child = s + len + 2;
		child_len_p = child;

		if (!isdigit(*child)) { /* ptbl file name */
			c_init = *(root + len);
			*(root + len) = '\0';
			strcat(d, root);
			*(root + len) = c_init;
			strcat(d, " in ");
			strcat(d, child);
			return (d);
		}

		while (isdigit(*child))
			child++;
		c_init = *child;
		*child = '\0';
		child_len = atoi(child_len_p);
		*child = c_init;
		if (*(child + child_len) == '\0') {
			strcat(d, child);
			strcat(d, " derived from ");
			c_init = *(root + len);
			*(root + len) = '\0';
			strcat(d, root);
			*(root + len) = c_init;
			return (d);
		} else { /* %drootname__%dchildname__filename */
			c_init = *(child + child_len);
			*(child + child_len) = '\0';
			strcat(d, child);
			*(child+ child_len) = c_init;
			strcat(d, " derived from ");
			c_init = *(root + len);
			*(root + len) = '\0';
			strcat(d, root);
			*(root + len) = c_init;
			strcat(d, " in ");
			strcat(d, child + child_len + 2);
			return (d);
		}
	}
}
