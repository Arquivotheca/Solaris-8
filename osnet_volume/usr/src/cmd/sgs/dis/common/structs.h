/*
 * Copyright (c) 1990-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)structs.h	1.5	97/09/07 SMI"

/*
 * definitions for data structure used by the disassembler;
 * nodes for linked lists of sections, functions, and
 * static/local/external symbols.
 */

#define	FAILURE 0
#define	SUCCESS 1
#define	TEXT	0
#define	DATA	1

typedef struct scnlist SCNLIST;
typedef struct funclist FUNCLIST;
typedef struct nfunction NFUNC;

/*
 * The linked list of scnlist's describes which sections are
 * to be disassembled
 */

struct scnlist {
	GElf_Shdr	*shdr;
	char		*scnam;
	SCNLIST		*snext;
	GElf_Word	scnum;
	int		stype;		/* disassemble as text or data   */
	FUNCLIST	*funcs;		/* the list of functions defined */
					/* in this section		 */
};

/*
 * A list of functions is associated with each section. This list is
 * used for printing the names of the functions and resyncing.
 */

struct funclist {
	char		*funcnm;
	GElf_Addr	faddr;		/* address of the function	*/
	long		fcnindex;	/* index of the function in	*/
					/* the symbol table		*/
	FUNCLIST	*nextfunc;
};

/*
 * If the -F option is specified, an array of nfunctions is set up
 * containing information about the functions
 */

struct nfunction {
	char	*funcnm;
	GElf_Addr	faddr;
	long	fcnindex;
	char	fnumaux;
	long	fsize;
	int	found;
	unsigned short	fscnum;
};


/* the following structures are used for symbolic disassembly */

/* structures for holding information about external and static symbols */

/* extern-static hash table structure */
typedef	struct node essymrec;
typedef struct node *pessymrec;
struct	node {
	char 	*name;
	GElf_Sxword	symval;
	struct node	*next;
};

/* extern-static union-array list structure */
typedef struct ua	uarec;
typedef	struct ua	*puarec;
struct	ua {
	char	*name;
	GElf_Sxword	symval;
	unsigned short type;
	int	range;
	struct ua	*next;
};


/*
 * structures for holding information about local symbols.
 * The local symbol structure requires the extra field containing
 * symbol storage class info. (sclass) to distinguish between automatic
 * symbols, function arguments and register symbols
 */

/* local hash table structure */
typedef	struct lnode locsymrec;
typedef struct lnode *plocsymrec;
struct	lnode {
	char 	*name;
	GElf_Sxword	symval;
	char	sclass;
	struct lnode	*next;
};

/* local union-array list structure */
typedef struct lua	locuarec;
typedef	struct lua	*plocuarec;
struct	lua {
	char	*name;
	GElf_Sxword	symval;
	unsigned short type;
	char	sclass;
	int	range;
	struct lua	*next;
};

#define	ESHTSIZ 500	/* external and static hash table size	*/
#define	LOCHTSIZ 50	/* local hash table size		*/

#define	V8_MODE		1	/* V8 */
#define	V9_MODE		2	/* V9 */
#define	V9_SGI_MODE	4	/* V9/SGI */


/*
 * function prototypes for common routines
 */
/* from debug.c */
extern void get_debug_info(void);
extern void get_line_info(void);
extern void print_line(long, unsigned char *, size_t);

/* from lists.c */
extern void build_sections(void);
extern void section_free(void);
extern void build_funcs(void);

/* from utls.c */
extern int  get_rel_section(GElf_Word);
extern void search_rel_data(GElf_Addr, GElf_Sxword, char **);
extern void locsympr(GElf_Sxword, int, char **);
extern void extsympr(GElf_Sxword, char **);
extern void build_labels(unsigned char *, GElf_Sxword);
extern void label_free(void);
extern void compoff(GElf_Sxword, char *);
extern void lookbyte(void);
extern void getbyte(void);
extern void convert(unsigned, char *, int);
extern void dis_data(GElf_Shdr *);
extern int  dfpconv(GElf_Sxword, GElf_Sxword, double *, short *);
extern void line_nums(void);
extern void looklabel(GElf_Sxword);
extern void printline(void);
extern void prt_offset(void);
extern int  resync(void);
extern int  sfpconv(long, double *);
extern void fatal(char *);
extern long bext(int);
extern long hext(int);
extern char *demangled_name(char *);
