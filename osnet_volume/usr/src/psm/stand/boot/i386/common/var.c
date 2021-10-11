/*
 * Copyright (c) 1992-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)var.c	1.9	96/04/08 SMI"

/* boot shell variable handling routines */

#include <sys/types.h>
#include <sys/bsh.h>
#include <sys/bootconf.h>
#include <sys/salib.h>

extern struct bootops	*bop;
extern int expr(int argc, char *argv[]);
extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);

void int_to_hex(int n, char *hp);


struct var varhdr = {NULL, NULL, NULL};

unsigned char *
var_ops(unsigned char *name, unsigned char *value, int op)
{
	struct var *varp;
	struct var *prevp;
	unsigned char *varname;
	int	namsiz;
	int	valsiz;
	int	totalsiz;
	int	rc;
	static char envvar[WORDSIZ];


	/*
	 *  printf("var_ops('%s','%s',%d)\n", name,
	 *  (value != NULL) ? value : (unsigned char *)"", op);
	 */

	/* find the variable name */
	for (prevp = &varhdr; (varp = prevp->next) != NULL; prevp = varp) {
	    varname = (unsigned char *)varp + sizeof (struct var);
	    if ((rc = strcmp((char *)name, (char *)varname)) > 0)
		continue;
	    else if (rc == 0) {		/* name found */
		if (op == FIND_VAR)
		    return (varname + varp->namsiz);
		/* SET_VAR or UNSET_VAR */
		prevp->next = varp->next;   /* remove element */
		totalsiz = sizeof (struct var) + varp->namsiz + varp->valsiz;
		bkmem_free((caddr_t)varp, totalsiz);
		if (op == SET_VAR)
		    goto installvar;
		else
		    return (NULL);	  /* UNSET_VAR */
	    }
	    else
		break;
	}

	/* name not found */
	if (op == FIND_VAR || op == UNSET_VAR) {
		/*
		 * See if there's an "environment variable" by this name.
		 * Kind of risky to support this because the property might
		 * not be a string.  To protect ourselves in this binary
		 * property case we'll tell bgetprop we only have room for
		 * WORDSIZ-1 bytes and always add a null character.
		 */
		if (bgetprop(bop, (char *)name, envvar, WORDSIZ, 0) != -1) {
		    envvar[WORDSIZ-1] = '\0';
		    return ((unsigned char *)envvar);
		}
		return (NULL);
	}

	/* install variable */
installvar:
	namsiz = strlen((char *)name) + 1;
	valsiz = strlen((char *)value) + 1;
	totalsiz = sizeof (struct var) + namsiz + valsiz;
	if ((varp = (struct var *)bkmem_alloc(totalsiz)) == NULL) {
	    printf("boot: var_ops(): no memory for variable '%s'\n", name);
	    return (NULL);
	}
	(void) strcpy((char *)varp + sizeof (struct var), (char *)name);
	(void) strcpy((char *)varp + sizeof (struct var) + namsiz,
			(char *)value);
	varp->namsiz = namsiz;
	varp->valsiz = valsiz;
	varp->next = prevp->next;
	prevp->next = varp;

	return (NULL);
}

void
set_cmd(int argc, char *argv[])
{
	struct var *varp;
	int	n;
	char	h[11];

	/*
	 *  printf("set_cmd(%x, %x)\n", argc, argv);
	 */
	if (argc == 1) {	/* display variable values */
		for (varp = &varhdr; (varp = varp->next) != NULL; ) {
			printf("%s = '%s'\n",
			    (unsigned char *)varp + sizeof (struct var),
			    (unsigned char *)varp + sizeof (struct var) +
				varp->namsiz);
		}
	} else if (argc == 2)	/* set to null string */
		(void) var_ops((unsigned char *)argv[1],
			(unsigned char *)"", SET_VAR);
	else if (argc == 3)	/* set to arg string */
		(void) var_ops((unsigned char *)argv[1],
			(unsigned char *)argv[2], SET_VAR);
	else {	/* set to expression value */
		n = expr(argc - 2, &argv[2]);
		int_to_hex(n, h);
		(void) var_ops((unsigned char *)argv[1],
			(unsigned char *)h, SET_VAR);
	}
}

void
unset_cmd(int argc, char *argv[])
{
	if (argc == 2)
		(void) var_ops((unsigned char *)argv[1], NULL, UNSET_VAR);
	else
		printf("boot: unset: bad argument count\n");
}


/* int_to_hex - convert an integer to hexadecimal notation */

void
int_to_hex(int n, char *hp)
{
	int	i;
	int	d;
	int	start;
	static char hex[] = "0123456789ABCDEF";

	*hp++ = '0';
	*hp++ = 'x';
	start = 0;
	for (i = 8; --i >= 0; ) {
		d = (unsigned int)n >> 28;
		n <<= 4;
		if (d != 0 || start || i == 0) {
			++start;
			*hp++ = hex[d];
		}
	}
	*hp = '\0';
}
