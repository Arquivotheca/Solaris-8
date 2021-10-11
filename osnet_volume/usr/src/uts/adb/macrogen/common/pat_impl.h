#include "pat.h"

/*
 * A pattern gets compiled into a tree of integers, called cells; if a cell
 * is > 0, it is a character to be matched.  Otherwise it is an operator.
 * Some operators store offsets, in bytes, to another part of the pattern.
 * Here are the operator codes.
 */
#define	LEFT_PAREN  -1
#define	RIGHT_PAREN -2
#define	MATCH_ANY   -3
#define	MATCH_ONE   -4
#define	NOT	    -5	/* next cells contain offset to end, min, max */
#define	OR	    -6	/* next cells contain offsets to alternative 2, end */
#define	END	    -7
