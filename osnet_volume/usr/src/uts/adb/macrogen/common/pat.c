/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pat.c	1.2	98/11/10 SMI"

#include "pat_impl.h"
#include <stddef.h>
#include <setjmp.h>

#undef NULL
#define	NULL ((void *) 0)

#undef MIN
#undef MAX
#define	MIN(a, b) ((a) < (b) ? (a) : (b))
#define	MAX(a, b) ((a) > (b) ? (a) : (b))

/* Define the struct used to form a stack of further pattern pieces to match. */
struct pat_rest {
	const pat_rest_t *next;
	const pat_cell_t *cells;
	const pat_cell_t *cells_end;
};

/* Struct used during compilation to pass back results of each step. */
typedef struct {
	pat_cell_t	*pat;	/* next cell of intermediate rep. to process */
	pat_cell_t	*cells;	/* next place to store a cell */
	long		 min;	/* minimum # chars matched at pat */
	long		 max;	/* maximum # chars matched at pat */
} results_t;

static void compile(pat_cell_t *, pat_cell_t *, long, long,
		    results_t *, jmp_buf *);

/* Default configuration (used if they specify 0). */
const pat_config_t pat_default_config = { '{', '}', '*', '?', '^', ',', '\\' };


/*
 * pat_match_() determines whether a set of characters matches a pattern.
 * Users actually call PAT_MATCH() and PAT_STRMATCH(), but they both come
 * here.  Using macros exposes a little of the implementation, for efficiency.
 * VERY frequently a match will fail on the very first character, so we get
 * significantly better performance by providing macros which call directly
 * into the recursive matcher.
 */
int	/* 1 if chars match pattern, else 0 */
pat_match_(
	const pat_cell_t	*cells,
	const pat_cell_t	*cells_end,
	const char		*chars,
	const char		*chars_end,
	const pat_rest_t	*rest)
{
	for (;;) {

		pat_cell_t c = (pat_cell_t)(*((unsigned char *)chars));
		pat_cell_t cell;

		/*
		 * Check to see whether the next character matches the
		 * next cell.  We do this before knowing whether we're
		 * past the end of the characters we're trying to
		 * match, and before knowing whether the next cell is a
		 * character to be matched.  If it *does* match, then it
		 * must have been a valid comparison.  If it doesn't,
		 * then we check to see whether it was valid.  The purpose
		 * of all of this is to optimize for the case where we're
		 * trying to match a character.  Note that this algorithm
		 * relies on the fact that following the characters is a
		 * terminator (typically a \0, but not necessarily), that
		 * cannot be in the pattern.
		 */
top:
		cell = *cells;
		if (c == cell) {
			cells++;
			chars++;
			continue;
		}

		/* Didn't match.  If it was supposed to, then no match! */
		if (cell > 0)
			return (0);

		/* Wasn't supposed to match.  Are we at end of pattern? */
		if (cells == cells_end) {

			/* If out of cells, better be out of chars. */
			if (!rest)
				return (chars == chars_end);

			/* Load up new pattern fragment and try again. */
			cells	  = rest->cells;
			cells_end = rest->cells_end;
			rest = rest->next;
			goto top;
		}
		cells++;

		if (cell == MATCH_ANY) {

			/*
			 * The next two cells are:
			 *	minimum # of chars matchable by rest of pattern
			 *	maximum # of chars matchable by rest of pattern
			 *		(or -1 if there is no maximum)
			 */
			long  min = *cells++;
			long  max;
			const char *first;	/* first char in chars to try */
			const char *last = chars_end - min; /* last .. to try */

			/* See if we have enough characters left for a match. */
			if (last < chars)
				return (0);

			/* If # of chars is fixed, just go on from there. */
			max = *cells++;
			if (max == min) {
				chars = last;
				continue;
			}

			/* Figure out where to start trying to match. */
			first = (max == -1) ? chars : (chars_end - max);
			if (first < chars)
				first = chars;

			/*
			 * Look for an X in [first, last] such that
			 *	[X, chars_end)  matches  [cells, cells_end)
			 * If we know what character must be at X, we can
			 * just search for it and try to match at those points.
			 */

			/* See if the next cell is a character. */
			if (*cells > 0) {

				/* It is; search for it and match rest. */
				char c = *cells++;
				do {
					if ((c == *first++) &&
					    pat_match_(cells, cells_end, first,
						chars_end, rest)) {
						return (1);
					}
				} while (first <= last);

				return (0);

			}

			/* Next cell was not a char; try to match all. */
			do {
				if (pat_match_(cells, cells_end,
				    first++, chars_end, rest))
					return (1);
			} while (first <= last);

			return (0);

		} else if (cell == NOT) {

			/*
			 * The next three cells are:
			 *	byte offset to pattern past NOTted part
			 *	minimum # of chars matchable by rest of pattern
			 *	maximum # of chars matchable by rest of pattern
			 *		(or -1 if there is no maximum)
			 */
			pat_cell_t *after = (pat_cell_t *)
				((char *)cells + *cells++);
			long  min = *cells++;
			long  max;
			const char *first;	/* first char in chars to try */
			const char *last = chars_end - min; /* last .. to try */
			char  c;

			/* See if we have enough characters left for a match. */
			if (last < chars)
				return (0);

			/*
			 * If the rest of the pattern is NOTted, just check
			 * for a match and return the opposite result.
			 * e.g.  foo^bar
			 */
			if ((after == cells_end) && !rest) {
				return (!pat_match_(cells, after - 1,
						    chars, chars_end, NULL));
			}

			/* Figure out where to start trying to match. */
			max = *cells++;
			first = (max == -1) ? chars : (chars_end - max);
			if (first < chars)
				first = chars;

			/*
			 * Look for an X in [first, last] such that
			 *	 [chars, X)  doesn't match  [cells, after)
			 * and
			 *   [X, chars_end)  matches  [after, cells_end)
			 * The reason for the after-1 is that there is an
			 * END cell after the NOTted pattern.
			 * If we know what character must be at X, we can
			 * just search for it and try to match at those points.
			 */

			/* See if next cell (after NOTted part) is a char. */
			if (*after > 0) {

				/* Yes; search for it and try to match there. */
				char c = *after;
				do {
					if ((c == *first) &&
					    !pat_match_(cells, after - 1,
						chars, first, NULL) &&
					    pat_match_(after + 1, cells_end,
						first + 1, chars_end, rest)) {
						return (1);
					}
				} while (++first <= last);

				return (0);

			}

			/* Next cell was not a char; try to match all. */
			do {
				if (!pat_match_(cells, after - 1,
				    chars, first, NULL) &&
				    pat_match_(after, cells_end,
					first, chars_end, rest)) {
					return (1);
				}
			} while (++first <= last);

			return (0);

		} else if (cell == OR) {

			/*
			 * The next cell is a byte offset that separates the
			 * first alternative from the second alternative.
			 */
			pat_cell_t *alt =
				(pat_cell_t *)((char *)cells + *cells++);
			pat_rest_t new_rest;

			/*
			 * The next cell is an offset that separates the
			 * alternatives from the rest of the pattern.
			 */
			new_rest.cells =
				(pat_cell_t *)((char *)cells + *cells++);
			new_rest.cells_end = cells_end;
			new_rest.next = rest;

			/* Recurse to see if first alternative is a match. */
			if (pat_match_(cells, alt - 1, chars, chars_end,
			    &new_rest))
				return (1);

			/* If not, just keep checking following alternative. */
			cells = alt;

		} else if (cell == MATCH_ONE) {

			/* If there's no char, then pat doesn't match. */
			if (chars++ == chars_end)
				return (0);

		}
	}
}

/*
 * pat_destroy() frees the cells array allocated for a pat_t by pat_compile().
 */
void
pat_destroy(pat_handle_t handle)
{
	free(handle);
}

/*
 * pat_compile() is the client's routine for compiling a pattern into an opaque
 * handle that can be used with PAT_MATCH() and PAT_STRMATCH().
 * compile() is the private, recursive part of it, along with
 * compile_alternative().  The former handles the OR (comma) operator.
 * adjust_set_len() resolves an interaction between NOT and MATCH_ANY; see
 * below.
 *
 * We proceed from right to left (that is, backwards) through the specified
 * pattern string, creating a tree of pat_cell_t structures to represent the
 * operators and literal text within it.  That tree is stored in an array.
 * The reason we do things from the end to the front is that we need info
 * that comes from looking further down the pattern (the minimum and maximum
 * lengths matchable by that part of the pattern); doing things in reverse
 * means that we have already accumulated that info by the time we need it.
 *
 * A pattern may contain the following metacharacters:
 *	\	the following character is not a metacharacter
 *	{}	grouping
 *	*	matches any characters
 *	?	matches any single character
 *	^	matches if the string does NOT match the remainder of pattern
 *	,	separates alternatives
 * '^' has higher precedence than ','.
 *
 * Actually, these are the default values; the actual characters used are
 * configurable.
 *
 * Possible errors:
 *	foo\		can't end in a backslash (except as \\).
 *	ab{cd,ef	unbalanced braces
 *	ab}cdef		unbalanced braces
 *
 * Processing of NOT involves matching on a substring, which means that we
 * have to adjust the lengths of MATCH_ANY entries within the NOTted pattern
 * (to exclude the length which is really after the substring).  For example,
 * example, the * in {^a*b}c should have a min and max of 1 (for the 'b'),
 * not 2 as it does in {d,a*b}c.  We initially assign lengths by substring,
 * and then accumulate them where appropriate in adjust_lengths().
 *
 * Pattern characters * ^ , are the interesting ones.  They look like this
 * when compiled:
 *
 *	*pat1		  MATCH_ANY pat1_min_len pat1_max_len pat1
 *
 *	{^pat1}pat2	  NOT offset pat2_min_len pat2_max_len pat1 END pat2
 *			         \_____________________________________/
 *			                ________________
 *			               /                \
 *	{pat1,pat2}pat3	  OR offset offset pat1 END pat2 pat3
 *			        \__________________/
 *
 * OR is right-associative, so pat1,pat2,pat3 is pat1,{pat2,pat3}.  This
 * is more efficient because it avoids deep nesting.  For example,
 *	{{pat1,pat2},pat3},pat4
 * requires that we recurse 3 levels, whereas
 *	pat1,{pat2,{pat3,pat4}}}
 * requires that we make 3 calls in succession.  On SPARC machines, the
 * deep recursion could require window overflow and underflow handling,
 * which is expensive.
 */

void
adjust_lengths(
	pat_cell_t	*cells,
	pat_cell_t	*cells_end,
	long		 min,
	long		 max)
{
	while (cells > cells_end) {

		pat_cell_t cell;

		cell = *--cells;

		if (cell == OR) {

			pat_cell_t *alt2;
			pat_cell_t *rest;

			/*
			 * Adjust both alternatives. The next cell is a byte
			 * offset that separates them.  The cell after that
			 * is a byte offset that separates the second
			 * alternative from the rest of the pattern.
			 */
			--cells;
			alt2 = (pat_cell_t *)((char *)(cells + 1) - *cells);
			--cells;
			rest = (pat_cell_t *)((char *)(cells + 1) - *cells);

			adjust_lengths(cells, alt2 + 1 /* END */, min, max);
			adjust_lengths(alt2, rest, min, max);

			cells = rest;

		} else if (cell == NOT) {

			pat_cell_t *rest;

			/*
			 * Adjust the min and max lengths, since they're for
			 * the pattern following the NOTted pattern.  Then
			 * skip over the pattern (it's matched as a subpattern,
			 * so the length is already correct).
			 */
			--cells;
			rest = (pat_cell_t *)((char *)(cells + 1) - *cells);
			--cells;
			*cells += min;
			--cells;
			*cells = ((max == -1) || (*cells == -1)) ?
					-1 : max + *cells;
			cells = rest;

		} else if (cell == MATCH_ANY) {

			/* Found one!  Adjust it. */
			--cells;
			*cells += min;
			--cells;
			*cells = ((max == -1) || (*cells == -1)) ?
					-1 : max + *cells;

		}

		/* MATCH_ONE and characters we can just skip over. */
	}
}

static void
compile_alternative(
	pat_cell_t	 *pat,
	pat_cell_t	 *cells,
	results_t	 *results,
	jmp_buf		 *out)
{
	pat_cell_t	 *initial_cells = cells;
	long		  min = 0;
	long		  max = 0;

	for (;;) {

		/* If we're done with our part, return. */
		pat_cell_t item = *pat;
		if ((item == END) || (item == LEFT_PAREN) || (item == OR))
			break;
		pat--;

		/* Handle various metacharacters. */
		if (item == MATCH_ANY) {

			*cells++ = max;
			*cells++ = min;
			*cells++ = MATCH_ANY;
			max = -1;

		} else if (item == MATCH_ONE) {

			*cells++ = MATCH_ONE;
			min++;
			if (max >= 0)
				max++;

		} else if (item == NOT) {

			/* Now insert an END cell. */
			memmove(initial_cells + 1, initial_cells,
				(char *)cells++ - (char *)(initial_cells));
			*initial_cells = END;

			/*
			 * Set min/max for pattern AFTER NOTted part to 0.
			 * Adjust_lengths() may add to it later.
			 */
			*cells++ = 0;
			*cells++ = 0;

			/*
			 * Save a byte offset to the part of the pattern
			 * following the stuff to be NOTted.
			 */
			*cells = sizeof (pat_cell_t)
				* (cells - initial_cells + 1);
			cells++;

			*cells++ = NOT;
			min = 0;
			max = -1;

		} else if (item == RIGHT_PAREN) {

			compile(pat, cells, min, max, results, out);
			pat   = results->pat;
			cells = results->cells;
			min   = results->min;
			max   = results->max;
			if (*pat-- != LEFT_PAREN)
				longjmp(*out, PAT_ERR_PAREN);

		} else {

			*cells++ = item;
			min++;
			if (max >= 0)
				max++;
		}
	}

	results->pat   = pat;
	results->cells = cells;
	results->min   = min;
	results->max   = max;
}

static void
compile(
	pat_cell_t	 *pat,
	pat_cell_t	 *cells,
	long		  min,
	long		  max,
	results_t	 *results,
	jmp_buf		 *out)
{
	pat_cell_t	 *end1;		/* end of alternative 1 */
	pat_cell_t	 *end2 = cells;	/* end of alternative 2 */
	long		  min1;
	long		  max1;
	long		  min2;
	long		  max2;

	/* Compile alternative 2. */
	compile_alternative(pat, cells, results, out);
	pat   = results->pat;
	cells = results->cells;
	min2  = results->min;
	max2  = results->max;

	while (*pat == OR) {

		end1  = cells;
		*cells++ = END;  /* marks the end of alternative 1 */

		/* Compile alternative 1. */
		compile_alternative(pat - 1 /* skip OR */, cells, results, out);
		pat   = results->pat;
		cells = results->cells;
		min1  = results->min;
		max1  = results->max;

		/*
		 * Insert the following (in reverse order).
		 *	OR
		 *	offset to end of alternative 1 (beginning of 2)
		 *	offset to end of alternative 2 (beginning of rest)
		 */
		*cells = sizeof (pat_cell_t) * (cells - end2 + 1);
		cells++;
		*cells = sizeof (pat_cell_t) * (cells - end1 + 1);
		cells++;
		*cells++ = OR;

		/*
		 * Combine min and max for the two alternatives.  The entire
		 * OR construct becomes alternative 2 for the next round.
		 */
		min2 = MIN(min1, min2);
		max2 = ((max1 == -1) || (max2 == -1)) ? -1 : MAX(max1, max2);
	}

	/* Figure out min and max for those alternatives plus what follows. */
	results->cells = cells;
	results->min = min + min2;
	results->max = ((max == -1) || (max2 == -1)) ? -1 : max + max2;

	/* Add our incoming min and max to cells where appropriate. */
	if (max != 0)	/* if max is 0, then min is 0, so no need */
		adjust_lengths(cells, end2, min, max);
}

int
pat_compile(
	const char	   *raw_pat,
	const pat_config_t *config,
	pat_handle_t	   *return_handle)
{
	pat_handle_t	    handle;
	int		    length = strlen(raw_pat);
	pat_cell_t	   *cells;
	pat_cell_t	   *cells_end;
	pat_cell_t	   *pat;
	pat_cell_t	   *p;
	jmp_buf		    out;
	int		    code;
	results_t	    results;

	/*
	 * Allocate memory for handle struct.  The struct will contain
	 * memory for an array of cells to represent the pattern.
	 * At most we will need space for 5 cells for each character,
	 * since NOT uses 5 cells (NOT, offset, min, max, END),
	 * plus one for the END cell for the entire pattern.
	 */
	handle = (pat_handle_t)
		malloc(offsetof(struct pat_handle, cells[5 * length + 1]));
	if (!handle)
		return (PAT_ERR_MEMORY);
	cells = handle->cells;

	/*
	 * Allocate memory for intermediate representation.  Here we only
	 * need one cell per character, plus one for a terminator.
	 */
	pat = (pat_cell_t *) malloc(sizeof (pat_cell_t) * (length + 1));
	if (!pat) {
		free(handle);
		return (PAT_ERR_MEMORY);
	}

	/* Further errors will be handled by longjmp'ing here. */
	code = setjmp(out);
	if (code) {
		pat_destroy(handle);
		return (code);
	}

	/* If they didn't specify a configuration, use our default. */
	if (!config)
		config = &pat_default_config;

	/*
	 * Create the intermediate representation (pat) from the raw
	 * pattern (raw_pat).  This is a straightforward translation
	 * of characters to cells, applying backslashes and replacing
	 * metacharacters with non-characters.
	 */
	p = pat;
	*p = END;	/* terminator (pat is processed backwards) */
	for (;;) {

		char c = *raw_pat++;
		if (c == '\0')
			break;
		if (c == config->left_paren)
			*++p = LEFT_PAREN;
		else if (c == config->right_paren)
			*++p = RIGHT_PAREN;
		else if (c == config->match_any)
			*++p = MATCH_ANY;
		else if (c == config->match_one)
			*++p = MATCH_ONE;
		else if (c == config->not)
			*++p = NOT;
		else if (c == config->or)
			*++p = OR;
		else {
			if (c == config->backslash) {
				c = *raw_pat++;
				if (c == '\0')
					longjmp(out, PAT_ERR_BACKSLASH);
			}
			*++p = (pat_cell_t)((unsigned char)c);
		}
	}

	/* Compile the pattern and return the handle if all went well. */
	*cells = END;
	compile(p, cells + 1, 0, 0, &results, &out);
	if (results.pat != pat)
		longjmp(out, PAT_ERR_PAREN); /* must be an unmatched paren */
	free(pat);
	cells_end = results.cells;

	/* Realloc cells array (in handle) to just the part we used. */
	length = cells_end - cells;
	handle = (pat_handle_t)
		realloc(handle, offsetof(struct pat_handle, cells[length]));
	cells = handle->cells;
	cells_end = cells + length;

	handle->cells_end = cells_end - 1;	/* don't count END */

	/* Reverse the order of the cells in the array. */
	while (cells < --cells_end) {

		pat_cell_t cell = *cells;
		*cells++ = *cells_end;
		*cells_end = cell;
	}

	/* Return the handle. */
	*return_handle = handle;
	return (0);
}
