/*
 *[]------------------------------------------------------------[]
 * | Forth Interpreter in C					|
 * | I'm writting this interpreter so that I can completely 	|
 * | test the dos file system code which I've just completed 	|
 * | writting. I thought about writting specific test cases in 	|
 * | 'C'. It seemed to inflexable and I needed an interpreter.	|
 * |								|
 * | Space is the major concern here. Because of this I haven't |
 * | bothered with directory alignment. I know I should. I can 	|
 * | hide the alignment stuff in macros that will be noops on 	|
 * | x86 machines.						|
 * |								|
 * | Author: Rick McNeal					|
 * | Date  : 19-May-1995					|
 *[]------------------------------------------------------------[]
 */

/* ---- struct and typedefs are here ---- */
typedef struct builtin {
	struct builtin *p;	/* ... previous built in function */
	unsigned short	t;	/* ... type of struct */
	char		*n;	/* ... pointer to name */
	void		(*f)();	/* ... pointer to func if valid */
} builtin_t, *builtin_p;

typedef struct cache {
	struct cache	*prev, *next;
	unsigned short	len, hits, flush;
	builtin_p	b;
} cache_t, *cache_p;

/* ---- type identifiers for builtin_t ---- */
#define T_CODE	0x0001		/* ... entry is a function */
#define T_CONST	0x0002		/* ...            constant value */
#define T_VAR	0x0003		/* ... 		  variable address */
#define T_COLON	0x0004		/* ...		  compiled code */
#define T_MASK	0x00ff		/* ... mask for types */
#define T_IMMED	0x8000		/* ... perform function even in compile mode */

/* ---- common defines ---- */
#define TRUE	-1		/* ... true is a non zero value */
#define FALSE	0
#define IMMED	1

/* ---- resource size defines ---- */
#ifdef unix
# define DICTSZ 0x1000		/* ... size of dict for unix, remember that */
#else				/*     an int is four bytes for unix and only */
# define DICTSZ	0x0800		/*     two bytes in realmode which greatly */
#endif				/*     effects the dictionary size */
#define STKSZ	64		/* ... size of stack pointer */
#define RPSTKSZ	64		/* ... size of return stack */
#define CACHE_NODE_SIZE 16	/* ... size of each cache */
#define TIBSZ	80		/* ... size of The Input Buffer */

/* ---- common function macros ---- */
#define FWORD(n, an, p, t, v) \
	builtin_t n = {&p, t, an, (void (*)())v};
#define FCODE(n, an, p) \
	void BT##n(); \
	builtin_t n = {&p, T_CODE, an, BT##n}; \
	void BT##n()
#define FCODEI(n, an, p) \
	void BT##n(); \
	builtin_t n = {&p, T_CODE|T_IMMED, an, BT##n}; \
	void BT##n()
#define setval(b, v)	(b)->f = (void (*)())v
#define getval(b)	(int)(b)->f
#define gettype(b)	((b)->t & T_MASK)
#define spush(v)	*(--sp) = (int)v
#define spop		*sp++
#define rpush(v)	*(--rp) = (int)v
#define rpop		*rp++
#define jump(v)		ip = (int *)v
#define unary(op)	sp[0] = op sp[0]
#define binary(op)	sp[1] = sp[1] op sp[0] ; sp++
#define relation(op)	sp[1] = sp[1] op sp[0] ? TRUE : FALSE ; sp++
#define LOG2(v) (v < 2 ? 0 : ( v < 4 ? 1 : ( v < 8 ? 2 : ( v < 16 ? 3 : ( v < 32 ? 4 : 5 )))))

/* ---- globals ---- */
int *sp;			/* ... stack pointer, grows down */
int *tos;			/* ... top of stack */
int *dp;			/* ... directory pointer, grows up */
int *rp;			/* ... return pointer, grows down */
int *rpt;			/* ... return pointer top */
int *ip;			/* ... thread pointer */
int inner;			/* ... state of machine */
char *tib;			/* ... The Input Buffer */
int iostate = FALSE;		/* ... next ioget() should print prompt */
int startingdict;		/* ... ip points here which has the addres
				       of the terminate function */
int debugvar = 0;		/* ... only used if -DDEBUG */
int *hs[2];			/* ... hot spots to check */
cache_p hash[LOG2(100)] = {0};

/* ---- forward references ---- */
void BTword();
void BTcomma();
void BTswap();
extern builtin_t leave;
/* ---- Things to use when compiling under Solaris ---- */
#ifdef unix
# include <stdio.h>
# include <sys/param.h>
# include <sys/stat.h>
# include <sys/mman.h>
# define bcopy_util(s, d, l) memcpy(d, s, l)
# define bzero_util(s, c) memset(s, 0, c)
# define printf_util printf
# define strtol_util strtol
# define malloc_util malloc
# define strcmp_util strcmp
# define strlen_util strlen
# define open_dos(f, p) open((f), (p), 0666)
# define forthWrite(f, p, c) write(f, p, c)
# define EQ(a,b) (strcmp(a, b) == 0)

int inputfd;			/* ... input fd, by default stdin */
int lastfd;			/* ... when inputfd returns -1, use this */
char *memp;			/* ... mmap'd area of input file */
int meml;			/* ... length of area */
main(int argc, char **argv)
{
	char *b, *p;
	struct stat s;

	debugvar = 0x10;
	if (argc > 1)
	  inputfd = open(argv[1], 0);
	else if (p = (char *)getenv("HOME")) {
		strcpy(b = (char *)malloc(MAXPATHLEN), p);
		inputfd = open(strcat(b, "/.forth"), 0);
		if (inputfd) {
			fstat(inputfd, &s);
			meml = s.st_size;
			memp = mmap(0, meml, PROT_READ, MAP_PRIVATE,
				inputfd, 0);
			if (memp == MAP_FAILED) inputfd = meml = 0;
			close(inputfd);
		}
		free(b);
	}
	setbuf(stdout, NULL);
	forth_init();
	forth_call();
}
putc_util(char c) {write(1, &c, 1);}
iogetc()
{
	char c;
	
	if (meml) { meml--; return *memp++; }
		
	while (read(inputfd, &c, 1) != 1) {
		inputfd = lastfd;
		iostate = TRUE;
		ioprompt();
	}
	return c;
}
#else
# include "disk.h"
_file_desc_p inputfd = 0;
_file_desc_p lastfd = 0;
extern int Meml, Memsize;
extern char *Memp, *Mems;
# define forthWrite(f, p, cc) write_dos((_file_desc_p)(f), p, cc)

iogetc()
{
	char c;

	if (Meml > 0) {Meml--; return *Memp++;}
	if (!Meml) {Meml = -1; free_util((u_int)Mems, Memsize); }
		
	if (inputfd) {
		if (read_dos(inputfd, &c, 1) == 1)
		  return c;
		else {
			inputfd = lastfd;
			iostate = TRUE;
			ioprompt();
		}
	}
	return getc_util();
}
#endif

/* ---- Debug stuff ---- */
#ifdef DEBUG
# undef DPrint
# define HOTSPOT 0xfeed
toscheck() 
{
	int quit = 0;
	if (sp > tos) { printf_util("sp overflow\n"); quit = 1; }
	if (*hs[0] != HOTSPOT) {
		printf_util("HotSpot.0: dp %x, sp %x\n", dp, sp);
		quit = 1;
	}
	if (*hs[1] != HOTSPOT) {
		printf_util("HotSpot.1: sp %x, rp %x\n", sp, rp);
		quit = 1;
	}
	if (quit) {
		inner = FALSE;
		setval(&leave, TRUE);
	}
}
# define DPrint(f, x) if (debugvar & f) printf_util x
# define DBG_INNER	0x0001
# define DBG_SEARCH	0x0002
# define DBG_COMP	0x0004
# define DBG_OUTER	0x0008
# define DBG_START	0x0010
# define DBG_STACKS	0x0020
# define DBG_CACHE	0x0040
#else
# define DPrint(f, x)
# define toscheck()
#endif

/*
 *[]------------------------------------------------------------[]
 * | get_cache -- during initialization when reading the .forth	|
 * | file the time to complete compiling everything we several	|
 * | seconds. Based on past experience I knew that a good 	|
 * | portion of that time was spent looking up words in the 	|
 * | dictionary. I added this caching code to speed up that op.	|
 * | Rick McNeal -- 06-Jun-1995					|
 *[]------------------------------------------------------------[]
 */
get_cache(char *s)
{
	cache_p c, cp;		/* ---- cache pointers */
	int len = strlen_util(s);/* ---- the length is used to find a hit */
	int idx = LOG2(len);	/* ---- find out which cache chain we're in */

	DPrint(DBG_CACHE, ("cache{%s}", s));
	if (!(c = hash[idx])) return FALSE;
	while (c) {
		DPrint(DBG_CACHE, ("{%s}", c->b->n));
		if ((len == c->len) && EQ(s, c->b->n)) {
			c->hits++;
			/*
			 * Bubble up the cache block based on the number
			 * of hits taken. Steps taken:
			 */
			if (c->prev && (c->prev->hits < c->hits)) {
				
				/* ---- unlink the cache block ---- */
				c->prev->next = c->next;
				if (c->next) c->next->prev = c->prev;

				/* ----	find out where it goes ---- */
				cp = c->prev;
				while (cp && (cp->hits < c->hits))
					cp = cp->prev;
				
				if (cp) {
					/* ---- insert into chain ---- */
					cp->next->prev = c;
					c->next = cp->next;
					cp->next = c;
					c->prev = cp;
				} else {
					/* ---- add to head of chain ---- */
					hash[idx]->prev = c;
					c->prev = 0;
					c->next = hash[idx];
					hash[idx] = c;
				}
			}
			DPrint(DBG_CACHE, ("hit "));
			break;
		}
		c = c->next;
	}
	if (c) {
		spush(c->b);
		spush(c->b->t & T_IMMED ? 1 : TRUE);
		return TRUE;
	} else {
		DPrint(DBG_CACHE, ("miss "));
		return FALSE;
	}
}

/*
 *[]------------------------------------------------------------[]
 * | alloc_node -- a given word wasn't found in the cache and	|
 * | we're going to add it by allocating a new node.		|
 * | Rick McNeal -- 06-Jun-1995					|
 *[]------------------------------------------------------------[]
 */
cache_p
alloc_node(builtin_p b, int len)
{
	cache_p c = (cache_p)malloc_util(sizeof(cache_t));
	
	/* --- quick way to clear links ---- */
	bzero_util((char *)c, sizeof(cache_t));

	c->b = b;
	c->len = len;
	return c;
}

/*
 *[]------------------------------------------------------------[]
 * | set_cache -- given a pointer to a forth word add it to the	|
 * | cache. First see if the given hash chain even has an entry	|
 * | if not, just add this node to the top. Otherwise we need	|
 * | to add this node to the correct chain. If we haven't	|
 * | exceeded an internal limit add the node by allocating space|
 * | else bump the last guy off of the chain.			|
 * | Rick McNeal -- 06-Jun-1995					|
 *[]------------------------------------------------------------[]
 */
set_cache(builtin_p b)
{
	int len = strlen_util(b->n);	/* ... save length for cache tag */
	int idx = LOG2(len);		/* ... find cache chain */
	int cs = 0;			/* ... limit cache size */
	cache_p c, cn;
	
	if (!hash[idx]) {
		hash[idx] = alloc_node(b, len);
		DPrint(DBG_CACHE, ("set(%d)", idx));
	} else {
		c = hash[idx];
		DPrint(DBG_CACHE, ("set(%d,%s[%s", idx, b->n, c->b->n));
		while (c->next) {
			DPrint(DBG_CACHE, (" %s", c->b->n));
			cs++; c = c->next;
		}
		if (cs < CACHE_NODE_SIZE) {
			/* ---- we still have room to add more ---- */
			cn = alloc_node(b, len);
			c->next = cn;
			cn->prev = c;
			DPrint(DBG_CACHE, (" added] "));
		} else {
			DPrint(DBG_CACHE, (" flush] "));
			c->b = b;
			c->len = len;
			c->hits = 0;
			c->flush++;
		}
	}
}

/*
 *[]------------------------------------------------------------[]
 * | Start of built in dictionary				|
 *[]------------------------------------------------------------[]
 */	
builtin_t root = {0, 0, "root", 0};
FWORD(leave, "leave", root, T_VAR, FALSE)
FWORD(base, "base", leave, T_VAR, 0x10)
FCODE(terminate, "term", base)
{
	rp = rpt;
	ip = (int *)&startingdict;
	inner = FALSE;
	DPrint(DBG_INNER, ("TERM'd\n"));
}
FCODE(Dup, "dup", terminate) { int v = *sp; spush(v); }
FCODE(add, "+", Dup) { binary(+);}
FCODE(sub, "-", add) {binary(-);}
FCODE(mul, "*", sub) {binary(*);}
/* ---- keyword 'div' has conflict with libc.so, so use 'Div' instead ---- */
FCODE(Div, "/", mul) {binary(/);}
FCODE(mod, "mod", Div) {binary(%);}
FCODE(not, "not", mod) {unary(~);}
FCODE(equal, "=", not) {relation(==);}
FCODE(greater, ">", equal) {relation(>);}
FCODE(less, "<", greater) {relation(<);}
FCODE(shl, "<<", less) {binary(<<);}
FCODE(shr, ">>", shl) {binary(>>);}
FCODE(store, "!", shr) { int *d = (int *)spop; *d = spop; }
FCODE(cstore, "c!", store) {char *c = (char *)spop; *c = (char)spop;}
FCODE(fetch, "@", cstore) { int *d = (int *)spop; spush(*d); }
FCODE(cfetch, "c@", fetch) {char *c = (char *)spop; spush(*c);}
FCODE(rstore, ">r", cfetch) {rpush(spop);}
FCODE(rfetch, "r>", rstore) {spush(rpop);}
FCODE(rpeek, "r@", rfetch) {spush(*rp);}
FCODE(pick, "pick", rpeek) {sp[0] = sp[sp[0] + 1];}
FWORD(state, "state", pick, T_VAR, FALSE)
FWORD(prevcomp, "last", state, T_VAR, FALSE)
FCODE(colon, ":", prevcomp)
{
	builtin_p	b;
	int		len;
	
	if (!getval(&state)) {
		setval(&state, TRUE);
		len = ioget(tib, ' ');
		DPrint(DBG_COMP, ("Compile %s\n", tib));
		bcopy_util(tib, (char *)dp, len + 1);
		b = (builtin_p)((int)dp + len + 1);
		b->p = (builtin_p)getval(&prevcomp);
		b->n = (char *)dp;
		b->t = T_COLON;
		/*
		 * move the directory pointer passed the ascii name plus 
		 * the null character, the size of the directory header 
		 * minus the size of an int. This way if you take the 
		 * address of builtin_t.code you'll have the first word 
		 * of this threaded entry
		 */
		dp = (int *)((int)dp + sizeof(builtin_t) +
		    len + 1 - sizeof(int));
		setval(&prevcomp, b);
	}
}
FCODE(parensemicolon, "(;)", colon)
{
	DPrint(DBG_INNER, ("Return to thread %x\n", *rp));
	jump(rpop);
}
FCODEI(semicolon, ";", parensemicolon)
{
	spush(&parensemicolon);
	BTcomma();
	setval(&state, FALSE);
}
FCODE(immed, "imm", semicolon)
{
	builtin_p b = (builtin_p)getval(&prevcomp);
	b->t |= T_IMMED;
}
FCODE(word, "word", immed)
{
	int sep = spop;
	spush(tib);
	spush(ioget(tib, sep));
}
FCODE(execute, "exec", word)
{
	builtin_p b = (builtin_p)spop;

	toscheck();
	DPrint(DBG_OUTER, ("%x_%s_%x\n", b, b->n, b->t));
	switch(gettype(b)) {
		case T_CODE:
			(*b->f)();
			break;
		
		case T_COLON:
			rpush(ip);
			jump(&b->f);
			DPrint(DBG_INNER,
			       ("IP Jumped from %x to %x\n", *rp, ip));
			break;
			
		case T_CONST:
			spush(b->f);
			break;
			
		case T_VAR:
			spush(&b->f);
			break;
	}
}
FCODE(emit, "emit", execute) {putc_util((char)spop);}
FCODE(number, "number", emit)
{
	int v, minus;
	char *s = (char *)spop, *p;
	
	if (*s == '-') {
		s++;
		minus = -1;
	}
	else
	  minus = 1;
	v = (int)strtol_util(s, &p, getval(&base));
	if (p && *p != '\0')
	  printf_util("%s ?\n", s);
	else
	  spush((v * minus));
}
FCODE(parenlit, "(lit)", number) {spush(*ip++);}
FCODEI(literal, "literal", parenlit)
{
	if (getval(&state) == TRUE) {
		spush(&parenlit);
		BTcomma();
		BTcomma();
	}
}
FCODE(comma, ",", literal) 
{
	DPrint(DBG_COMP, ("\t\t%x = %x\n", dp, *sp));
	*dp++ = spop;
}
FWORD(here, "(here)", comma, T_CONST, &dp)
FCODE(Open, "open", here)
{
	int f = spop;
	*sp = (int)open_dos((char *)*sp, f);
}
FCODE(Write, "write", Open)
{
	int cc = spop;
	char *b = (char *)spop;
	*sp = forthWrite(*sp, b, cc);
}
FCODE(qbranch, "(?branch)", Write)
{
	if (!spop) {
		DPrint(DBG_INNER, ("?branch jump %x\n", *ip));
		jump(*ip++);
	} else {
		DPrint(DBG_INNER, ("?branch doing...\n"));
		ip++;
	}
}
FCODE(Jump, "(jmp)", qbranch) { jump(*ip++); }
/*
 *[]------------------------------------------------------------[]
 * | XXX This routine should be compiled, not hard code.	|
 *[]------------------------------------------------------------[]
 */
FCODE(dots, ".s", Jump)
{
	int *d = sp;

	if (d == tos)
	  printf_util("(empty)");
	while (d < tos)
	  printf_util("%x ", *d++);
	printf_util("\n");
}
FWORD(DebugFunc, "debug", dots, T_CONST, &debugvar)
FWORD(InputFD, "inputfd", DebugFunc, T_CONST, &inputfd)
FWORD(LastFD, "lastfd", InputFD, T_CONST, &lastfd)
FCODE(swap, "swap", LastFD) { int v = *sp; *sp = sp[1]; sp[1] = v; }
FCODE(drop, "drop", swap) {sp++;}
FCODE(cache, "cache", drop)
{
	cache_p c;
	int i, flush = 0;
	
	for (i = LOG2(100); i--;) {
		c = hash[i];
		printf_util("Cache %d::  ", i);
		if (c) {
			while (c) {
				printf_util("%s,%d ", c->b->n, c->hits);
				flush += c->flush;
				c = c->next;
			}
			printf_util("\n  Flushes %d\n", flush);
		} else
			printf_util("EMPTY\n");
		flush = 0;
	}
}

/* ---- find must be the last entry, else code must change ---- */
FCODE(find, "find", cache)
{
	builtin_p b = &find;	/* ... reference last entry here */
	char *s = (char *)spop;

	if (get_cache(s) == TRUE)
		return;
	
	/* ---- see if name is amoung the built-in functions ---- */
	if (search_chain(b, s) == FALSE) {
		/* ---- nope, no search the compiled entries ---- */
		b = (builtin_p)getval(&prevcomp);
		if (search_chain(b, s) == FALSE) {
			spush(s);
			spush(FALSE);
			DPrint(DBG_SEARCH, ("\t%s not found\n", s));
			return;
		}
	}
}
/*
 *[]------------------------------------------------------------[]
 * | End of built in dictionary.
 *[]------------------------------------------------------------[]
 */

/*
 *[]------------------------------------------------------------[]
 * | search_chain -- this is a support routine which is 	|
 * | currently only used by "find".				|
 *[]------------------------------------------------------------[]
 */ 
search_chain(builtin_p b, char *s)
{
	while (b) {
		if ((*s == *b->n) && (strcmp_util(s, b->n) == 0)) {
			set_cache(b);
			spush(b);
			spush((b->t & T_IMMED) ? 1 : TRUE);
			DPrint(DBG_SEARCH, ("\tGot %x_%s_%x\n",
					    b, b->n, b->t));
			return TRUE;
		}
		b = b->p;
	}
	return FALSE;
}

/*
 *[]------------------------------------------------------------[]
 * | forth_init -- as the name implies, this routine does all	|
 * | initialization necessary before the main loop can be 	|
 * | called.							|
 *[]------------------------------------------------------------[]
 */
forth_init()
{
	dp = (int *)malloc_util(DICTSZ);
	sp = (int *)malloc_util(STKSZ);
	rp = (int *)malloc_util(RPSTKSZ);

#ifdef DEBUG
	if (debugvar & DBG_START)
		printf_util("%8s %8s %8s\n%8x %8x %8x\n", "dp", "sp", "rp",
			dp, sp, rp);
	hs[0] = sp;
	*sp = HOTSPOT;
	hs[1] = rp;
	*rp = HOTSPOT;
#endif
	tos = sp = sp + STKSZ/sizeof(int) - 1;
	rp = rpt = rp + RPSTKSZ/sizeof(int) - 1;
	tib = (char *)malloc_util(TIBSZ);
	startingdict = (int)&terminate;
	ip = (int *)&startingdict;
	inner = TRUE;
}

/*
 *[]------------------------------------------------------------[]
 * | forth_call -- here the main loop of the forth interpreter.	|
 * | Until "leave" is set to zero this routine continues.	|
 *[]------------------------------------------------------------[]
 */
forth_call()
{
	int code;

	DPrint(DBG_START, ("Debug code compiled\n"));
	while(getval(&leave) == FALSE) {
		toscheck();
		spush(' ');
		BTword();

		/* ... if count is zero */		
		if (spop == 0) {
			spop;	/* ... toss buffer */
			continue;
		}
		
		BTfind();
		code = spop;
		if (code) {
			if (getval(&state) == code)
				BTcomma();
			else {
				BTexecute();
				if (rp != rpt) {
					inner = TRUE;
					doinner();
				}
			}
		}
		else {
			BTnumber();
			BTliteral();
		}
	}
}

/*
 *[]------------------------------------------------------------[]
 * | doinner -- what forth_call is the to user doinner is to 	|
 * | the main interpreter "exec". ip begins by pointing at	|
 * | "terminate" and until the rp stack is poped and causes ip 	|
 * | to interpret "terminate" this routine will run.		|
 *[]------------------------------------------------------------[]
 */
doinner()
{
	while(inner == TRUE) {
		toscheck();
		DPrint(DBG_INNER, ("ip @ %x - ", ip));
		spush((builtin_p)*ip++);
		BTexecute();
		DPrint(DBG_STACKS, ("\t\t\t[sp:%x=%x rp:%x=%x]\n",
				    sp, *sp, rp, *rp));
	}
}

/*
 *[]------------------------------------------------------------[]
 * | ioprompt -- print out a prompt character if input is 	|
 * | comming from a keyboard. The character printed depends on	|
 * | the state of the interpreter.				|
 *[]------------------------------------------------------------[]
 */
ioprompt()
{
	if (!inputfd && iostate == TRUE) {
		iostate = FALSE;
		printf_util(getval(&state) == TRUE ? " ] " : "ok ");
	}
}

/*
 *[]--------------------------------------------------------------------[]
 * | ioget -- get a word which is defined as:				|
 * |   Input characters until the 'sep' character is seen in input 	|
 * |   stream. When 'sep' is a space its really a meta character and 	|
 * |   means to look for a space, tab, or newline.			|
 *[]--------------------------------------------------------------------[]
 */
ioget(char *b, int sep)
{	
	char	c;		/* ... the input character */
	char	*bo = b;	/* ... bo is used for backspace processing */
	int	cc = 0;		/* ... the number of characters received */

	ioprompt();
	do {
		c = iogetc();
		if (sep == ' ') {
			if ((c == ' ') || (c == '\t') ||
			    (c == '\n') || (c == '\r'))
				break;
		}
		else if (sep == c)
			break;

		if (c == '\b') {
			putc_util(' '); putc_util('\b');
			if (b > bo) {
				b--;
				cc--;
			}
			else {
				b = bo;
				c = 0;
			}
		}
		else {
			*b++ = c;
			cc++;
		}
	} while (1);
	*b++ = '\0';
	if ((c == '\n') || (c == '\r'))
	  iostate = TRUE;
	return cc;
}







