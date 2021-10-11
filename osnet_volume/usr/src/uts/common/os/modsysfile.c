/*
 * Copyright (c) 1990-1994, 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)modsysfile.c	1.81	99/06/09 SMI"

#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/disp.h>
#include <sys/bootconf.h>
#include <sys/sysconf.h>
#include <sys/sunddi.h>
#include <sys/esunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/kmem.h>
#include <sys/vmem.h>
#include <sys/fs/ufs_fsdir.h>
#include <sys/hwconf.h>
#include <sys/modctl.h>
#include <sys/cmn_err.h>
#include <sys/kobj.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/autoconf.h>
#include <sys/callb.h>
#include <sys/sysmacros.h>
#include <sys/dacf.h>
#include <vm/seg_kmem.h>

struct hwc_class {
	struct hwc_class *class_next;
	char		*class_exporter;
	char		*class;
};

static struct hwc_class *hcl_head;	/* head of list of classes */

#define	DAFILE		"/etc/driver_aliases"
#define	CLASSFILE	"/etc/driver_classes"
#define	DACFFILE	"/etc/dacf.conf"

static char class_file[] = CLASSFILE;
static char dafile[] = DAFILE;
static char dacffile[] = DACFFILE;

char *systemfile = "etc/system";	/* name of ascii system file */

static struct sysparam *sysparam_hd;	/* head of parameters list */
static struct sysparam *sysparam_tl;	/* tail of parameters list */
static vmem_t *mod_sysfile_arena;	/* parser memory */

#if defined(__i386) || defined(__ia64)

struct psm_mach {
	struct psm_mach *m_next;
	char		*m_machname;
};

static struct psm_mach *pmach_head;	/* head of list of classes */

#define	MACHFILE	"/etc/mach"
static char mach_file[] = MACHFILE;

static char rtc_config_file[] = "/etc/rtc_config";

#endif /* __i386 || __ia64 */

static void sys_set_var(int, struct sysparam *, void *);

static void setparams(void);
static int get_string(u_longlong_t *, char *);
int getvalue(char *, u_longlong_t *);
static void impl_replicate_hwc_spec(struct par_list *, char *,
    struct hwc_spec **, struct hwc_spec **);


/*
 * driver.conf parse thread control structure
 */
struct hwc_parse_mt {
	ksema_t		sema;
	char		*name;		/* name of .conf files */
	struct hwc_spec	*rv;		/* return from hwc_parse_now */
};

static struct hwc_spec *hwc_parse_now(char *);
static void hwc_parse_thread(struct hwc_parse_mt *);
static struct hwc_parse_mt *hwc_parse_mtalloc(char *);
static void hwc_parse_mtfree(struct hwc_parse_mt *);

#ifdef DEBUG
static int parse_debug_on = 0;

/*VARARGS1*/
static void
parse_debug(struct _buf *file, char *fmt, ...)
{
	va_list adx;

	if (parse_debug_on) {
		va_start(adx, fmt);
		vprintf(fmt, adx);
		if (file)
			printf(" on line %d of %s\n", kobj_linenum(file),
				kobj_filename(file));
		va_end(adx);
	}
}
#endif /* DEBUG */

#define	FE_BUFLEN 256

/*PRINTFLIKE3*/
static void
file_err(int type,  struct _buf *file, char *fmt, ...)
{
	va_list ap;
	/*
	 * If we're in trouble, we might be short on stack... be paranoid
	 */
	char *buf = kmem_alloc(FE_BUFLEN, KM_SLEEP);
	char *trailer = kmem_alloc(FE_BUFLEN, KM_SLEEP);
	char *fmt_str = kmem_alloc(FE_BUFLEN, KM_SLEEP);
	char prefix = '\0';

	va_start(ap, fmt);
	if (strchr("^!?", fmt[0]) != NULL) {
		prefix = fmt[0];
		fmt++;
	}
	(void) vsnprintf(buf, FE_BUFLEN, fmt, ap);
	va_end(ap);
	(void) snprintf(trailer, FE_BUFLEN, " on line %d of %s",
	    kobj_linenum(file), kobj_filename(file));

	/*
	 * If prefixed with !^?, prepend that character
	 */
	if (prefix != '\0') {
		(void) snprintf(fmt_str, FE_BUFLEN, "%c%%s%%s", prefix);
	} else {
		(void) strncpy(fmt_str, "%s%s", FE_BUFLEN);
	}

	cmn_err(type, fmt_str, buf, trailer);
	kmem_free(buf, FE_BUFLEN);
	kmem_free(trailer, FE_BUFLEN);
	kmem_free(fmt_str, FE_BUFLEN);
}

#define	isunary(ch)	((ch) == '~' || (ch) == '-')

#define	iswhite(ch)	((ch) == ' ' || (ch) == '\t')

#define	isnewline(ch)	((ch) == '\n' || (ch) == '\r' || (ch) == '\f')

#define	isdigit(ch)	((ch) >= '0' && (ch) <= '9')

#define	isxdigit(ch)	(isdigit(ch) || ((ch) >= 'a' && (ch) <= 'f') || \
			((ch) >= 'A' && (ch) <= 'F'))

#define	isalpha(ch)	(((ch) >= 'a' && (ch) <= 'z') || \
			((ch) >= 'A' && (ch) <= 'Z'))

#define	isalphanum(ch)	(isalpha(ch) || isdigit(ch))

#define	isnamechar(ch)	(isalphanum(ch) || (ch) == '_' || (ch) == '-')

typedef enum {
	EQUALS,
	AMPERSAND,
	BIT_OR,
	STAR,
	POUND,
	COLON,
	SEMICOLON,
	COMMA,
	SLASH,
	WHITE_SPACE,
	NEWLINE,
	EOF,
	STRING,
	HEXVAL,
	DECVAL,
	NAME
} token_t;

#ifdef DEBUG
char *tokennames[] = {
	"EQUALS",
	"AMPERSAND",
	"BIT_OR",
	"STAR",
	"POUND",
	"COLON",
	"SEMICOLON",
	"COMMA",
	"SLASH",
	"WHITE_SPACE",
	"NEWLINE",
	"EOF",
	"STRING",
	"HEXVAL",
	"DECVAL",
	"NAME"
};
#endif /* DEBUG */

static token_t
lex(struct _buf *file, char *val)
{
	char	*cp;
	int	ch, oval, badquote;
	token_t token;

	cp = val;
	while ((ch = kobj_getc(file)) == ' ' || ch == '\t')
		;

	*cp++ = (char)ch;
	switch (ch) {
	case '=':
		token = EQUALS;
		break;
	case '&':
		token = AMPERSAND;
		break;
	case '|':
		token = BIT_OR;
		break;
	case '*':
		token = STAR;
		break;
	case '#':
		token = POUND;
		break;
	case ':':
		token = COLON;
		break;
	case ';':
		token = SEMICOLON;
		break;
	case ',':
		token = COMMA;
		break;
	case '/':
		token = SLASH;
		break;
	case ' ':
	case '\t':
	case '\f':
		while ((ch  = kobj_getc(file)) == ' ' ||
		    ch == '\t' || ch == '\f')
			*cp++ = (char)ch;
		(void) kobj_ungetc(file);
		token = WHITE_SPACE;
		break;
	case '\n':
	case '\r':
		token = NEWLINE;
		break;
	case '"':
		cp--;
		badquote = 0;
		while (!badquote && (ch  = kobj_getc(file)) != '"') {
			switch (ch) {
			case '\n':
			case -1:
				file_err(CE_WARN, file, "Missing \"");
				cp = val;
				*cp++ = '\n';
				badquote = 1;
				/* since we consumed the newline/EOF */
				(void) kobj_ungetc(file);
				break;

			case '\\':
				ch = (char)kobj_getc(file);
				if (!isdigit(ch)) {
					/* escape the character */
					*cp++ = (char)ch;
					break;
				}
				oval = 0;
				while (ch >= '0' && ch <= '7') {
					ch -= '0';
					oval = (oval << 3) + ch;
					ch = (char)kobj_getc(file);
				}
				(void) kobj_ungetc(file);
				/* check for character overflow? */
				if (oval > 127) {
					cmn_err(CE_WARN,
					    "Character "
					    "overflow detected.");
				}
				*cp++ = (char)oval;
				break;
			default:
				*cp++ = (char)ch;
				break;
			}
		}
		token = STRING;
		break;

	default:
		if (ch == -1) {
			token = EOF;
			break;
		}
		/*
		 * detect a lone '-' (including at the end of a line), and
		 * identify it as a 'name'
		 */
		if (ch == '-') {
			*cp++ = (char)(ch = kobj_getc(file));
			if (iswhite(ch) || (ch == '\n')) {
				(void) kobj_ungetc(file);
				cp--;
				token = NAME;
				break;
			}
		} else if (isunary(ch)) {
			*cp++ = (char)(ch = kobj_getc(file));
		}

		if (isdigit(ch)) {
			if (ch == '0') {
				if ((ch = kobj_getc(file)) == 'x') {
					*cp++ = (char)ch;
					ch = kobj_getc(file);
					while (isxdigit(ch)) {
						*cp++ = (char)ch;
						ch = kobj_getc(file);
					}
					(void) kobj_ungetc(file);
					token = HEXVAL;
				} else {
					goto digit;
				}
			} else {
				ch = kobj_getc(file);
digit:
				while (isdigit(ch)) {
					*cp++ = (char)ch;
					ch = kobj_getc(file);
				}
				(void) kobj_ungetc(file);
				token = DECVAL;
			}
		} else if (isalpha(ch)) {
			ch = kobj_getc(file);
			while (isnamechar(ch)) {
				*cp++ = (char)ch;
				ch = kobj_getc(file);
			}
			(void) kobj_ungetc(file);
			token = NAME;
		} else {
			return (-1);
		}
		break;
	}
	*cp = '\0';

#ifdef DEBUG
	parse_debug(NULL, "lex: token %s value '%s'\n", tokennames[token], val);
#endif
	return (token);
}

/*
 * Leave NEWLINE as the next character.
 */

static void
find_eol(struct _buf *file)
{
	register int ch;

	while ((ch = kobj_getc(file)) != -1) {
		if (isnewline(ch)) {
			(void) kobj_ungetc(file);
			break;
		}
	}
}

/*
 * The ascii system file is read and processed.
 *
 * The syntax of commands is as follows:
 *
 * '*' in column 1 is a comment line.
 * <command> : <value>
 *
 * command is EXCLUDE, INCLUDE, FORCELOAD, ROOTDEV, ROOTFS,
 *	SWAPDEV, SWAPFS, MODDIR, SET
 *
 * value is an ascii string meaningful for the command.
 */

/*
 * Table of commands
 */
static struct modcmd modcmd[] = {
	{ "EXCLUDE",	MOD_EXCLUDE	},
	{ "exclude",	MOD_EXCLUDE	},
	{ "INCLUDE",	MOD_INCLUDE	},
	{ "include",	MOD_INCLUDE	},
	{ "FORCELOAD",	MOD_FORCELOAD	},
	{ "forceload",	MOD_FORCELOAD	},
	{ "ROOTDEV",	MOD_ROOTDEV	},
	{ "rootdev",	MOD_ROOTDEV	},
	{ "ROOTFS",	MOD_ROOTFS	},
	{ "rootfs",	MOD_ROOTFS	},
	{ "SWAPDEV",	MOD_SWAPDEV	},
	{ "swapdev",	MOD_SWAPDEV	},
	{ "SWAPFS",	MOD_SWAPFS	},
	{ "swapfs",	MOD_SWAPFS	},
	{ "MODDIR",	MOD_MODDIR	},
	{ "moddir",	MOD_MODDIR	},
	{ "SET",	MOD_SET		},
	{ "set",	MOD_SET		},
	{ "SET32",	MOD_SET32	},
	{ "set32",	MOD_SET32	},
	{ "SET64",	MOD_SET64	},
	{ "set64",	MOD_SET64	},
	{ NULL,		MOD_UNKNOWN	}
};


static char bad_op[] = "illegal operator '%s' used on a string";
static char colon_err[] = "A colon (:) must follow the '%s' command";
static char tok_err[] = "Unexpected token '%s'";
static char extra_err[] = "extraneous input ignored starting at '%s'";

static struct sysparam *
do_sysfile_cmd(struct _buf *file, char *cmd)
{
	register struct sysparam *sysp;
	register struct modcmd *mcp;
	register token_t token, op;
	register char *cp;
	register int ch;
	char tok1[MOD_MAXPATH + 1]; /* used to read the path set by 'moddir' */
	char tok2[64];

	for (mcp = modcmd; mcp->mc_cmdname != NULL; mcp++) {
		if (strcmp(mcp->mc_cmdname, cmd) == 0)
			break;
	}
	sysp = vmem_alloc(mod_sysfile_arena, sizeof (struct sysparam),
	    VM_SLEEP);
	bzero(sysp, sizeof (struct sysparam));
	sysp->sys_op = SETOP_NONE; /* set op to noop initially */

	switch (sysp->sys_type = mcp->mc_type) {
	case MOD_INCLUDE:
	case MOD_EXCLUDE:
	case MOD_FORCELOAD:
		/*
		 * Are followed by colon.
		 */
	case MOD_ROOTFS:
	case MOD_SWAPFS:
		if ((token = lex(file, tok1)) == COLON) {
			token = lex(file, tok1);
		} else {
			file_err(CE_WARN, file, colon_err, cmd);
		}
		if (token != NAME) {
			file_err(CE_WARN, file, "value expected");
			goto bad;
		}

		cp = tok1 + strlen(tok1);
		do {
			*cp++ = (char)(ch = kobj_getc(file));
		} while (!iswhite(ch) && !isnewline(ch) && (ch != -1));
		*(--cp) = '\0';
		if (ch != -1)
			(void) kobj_ungetc(file);
		if (sysp->sys_type == MOD_INCLUDE)
			return (NULL);
		sysp->sys_ptr = vmem_alloc(mod_sysfile_arena, strlen(tok1) + 1,
		    VM_SLEEP);
		(void) strcpy(sysp->sys_ptr, tok1);
		break;
	case MOD_SET:
	case MOD_SET64:
	case MOD_SET32:
	{
		char *var;

		if (lex(file, tok1) != NAME) {
			file_err(CE_WARN, file, "value expected");
			goto bad;
		}

		/*
		 * If the next token is a colon (:),
		 * we have the <modname>:<variable> construct.
		 */
		if ((token = lex(file, tok2)) == COLON) {
			if ((token = lex(file, tok2)) == NAME) {
				var = tok2;
				/*
				 * Save the module name.
				 */
				sysp->sys_modnam = vmem_alloc(mod_sysfile_arena,
				    strlen(tok1) + 1, VM_SLEEP);
				(void) strcpy(sysp->sys_modnam, tok1);
				op = lex(file, tok1);
			} else {
				file_err(CE_WARN, file, "value expected");
				goto bad;
			}
		} else {
			/* otherwise, it was the op */
			var = tok1;
			op = token;
		}
		/*
		 * kernel param - place variable name in sys_ptr.
		 */
		sysp->sys_ptr = vmem_alloc(mod_sysfile_arena, strlen(var) + 1,
		    VM_SLEEP);
		(void) strcpy(sysp->sys_ptr, var);
		/* set operation */
		switch (op) {
		case EQUALS:
			/* simple assignment */
			sysp->sys_op = SETOP_ASSIGN;
			break;
		case AMPERSAND:
			/* bitwise AND */
			sysp->sys_op = SETOP_AND;
			break;
		case BIT_OR:
			/* bitwise OR */
			sysp->sys_op = SETOP_OR;
			break;
		default:
			/* unsupported operation */
			file_err(CE_WARN, file,
				"unsupported operator %s", tok2);
			goto bad;
		} /* end switch */

		switch (lex(file, tok1)) {
		case STRING:
			/* string variable */
			if (sysp->sys_op != SETOP_ASSIGN) {
				file_err(CE_WARN, file, bad_op, tok1);
				goto bad;
			}
			if (get_string(&sysp->sys_info, tok1) == 0) {
				file_err(CE_WARN, file, "string garbled");
				goto bad;
			}
			break;
		case HEXVAL:
		case DECVAL:
			if (getvalue(tok1, &sysp->sys_info) == -1) {
				file_err(CE_WARN, file, "invalid number '%s'",
					tok1);
				goto bad;
			}
		} /* end switch */

		/*
		 * Now that we've parsed it to check the syntax, consider
		 * discarding it (because it -doesn't- apply to this flavour
		 * of the kernel)
		 */
#ifdef _LP64
		if (sysp->sys_type == MOD_SET32)
			return (NULL);
#else
		if (sysp->sys_type == MOD_SET64)
			return (NULL);
#endif
		sysp->sys_type = MOD_SET;
		break;
	}
	case MOD_MODDIR:
		if ((token = lex(file, tok1)) != COLON) {
			file_err(CE_WARN, file, colon_err, cmd);
			goto bad;
		}

		cp = tok1;
		while ((token = lex(file, cp)) != NEWLINE && token != EOF) {
			cp += strlen(cp);
			do {
				*cp++ = (char)(ch = kobj_getc(file));
			} while (!iswhite(ch) && !isnewline(ch) &&
			    ch != ':' && (ch != -1));
			*(cp - 1) = ':';
			if (isnewline(ch))
				(void) kobj_ungetc(file);
		}
		(void) kobj_ungetc(file);
		*(cp-1) = '\0';
		sysp->sys_ptr = vmem_alloc(mod_sysfile_arena, strlen(tok1) + 1,
		    VM_SLEEP);
		(void) strcpy(sysp->sys_ptr, tok1);
		break;

	case MOD_SWAPDEV:
	case MOD_ROOTDEV:
		if ((token = lex(file, tok1)) != COLON) {
			file_err(CE_WARN, file, colon_err, cmd);
			goto bad;
		}
		while ((ch = kobj_getc(file)) == ' ' || ch == '\t')
			;
		(void) kobj_ungetc(file);
		cp = tok1;
		do {
			*cp++ = (char)(ch = kobj_getc(file));
		} while (!iswhite(ch) && !isnewline(ch) && (ch != -1));
		if (ch != -1)
			(void) kobj_ungetc(file);
		*(cp-1) = '\0';
		sysp->sys_ptr = vmem_alloc(mod_sysfile_arena, strlen(tok1) + 1,
		    VM_SLEEP);
		(void) strcpy(sysp->sys_ptr, tok1);
		break;

	case MOD_UNKNOWN:
	default:
		file_err(CE_WARN, file, "unknown command '%s'", cmd);
		goto bad;
	}

	return (sysp);

bad:
	find_eol(file);
	return (NULL);
}

void
mod_read_system_file(int ask)
{
	register struct sysparam *sp;
	register struct _buf *file;
	register token_t token, last_tok;
	char tokval[MAXLINESIZE];

	mod_sysfile_arena = vmem_create("mod_sysfile", NULL, 0, 8,
	    segkmem_alloc, segkmem_free, heap_arena, 0, VM_SLEEP);

	if (ask)
		mod_askparams();

	if (systemfile != NULL) {

		if ((file = kobj_open_file(systemfile)) ==
		    (struct _buf *)-1) {
			cmn_err(CE_WARN, "cannot open system file: %s",
			    systemfile);
		} else {
			/* XXX - Too tricky */
			sysparam_tl = (struct sysparam *)&sysparam_hd;

			last_tok = NEWLINE;
			while ((token = lex(file, tokval)) != EOF) {
				switch (token) {
				case STAR:
				case POUND:
					/*
					 * Skip comments.
					 */
					if (last_tok != NEWLINE)
						file_err(CE_WARN, file,
						    "line comment ignored");
					find_eol(file);
					break;
				case NEWLINE:
					kobj_newline(file);
					last_tok = NEWLINE;
					break;
				case NAME:
					if (last_tok != NEWLINE) {
						file_err(CE_WARN, file,
						    extra_err, tokval);
						find_eol(file);
					} else if ((sp = do_sysfile_cmd(file,
					    tokval)) != NULL) {
						sp->sys_next = NULL;
						sysparam_tl->sys_next = sp;
						sysparam_tl = sp;
					}
					last_tok = NAME;
					break;
				default:
					/* Error?? */
					file_err(CE_WARN,
					    file, tok_err, tokval);
					find_eol(file);
					break;
				}
			}
			kobj_close_file(file);
		}
	}

	(void) mod_sysctl(SYS_SET_KVAR, NULL);

	if (ask == 0)
		setparams();
}

/*
 * Process the system file commands.
 */
int
mod_sysctl(int fcn, void *p)
{
	static char wmesg[] = "forceload of %s failed";
	register struct sysparam *sysp;
	register char *name;
	struct modctl *modp;

	if (sysparam_hd == NULL)
		return (0);

	for (sysp = sysparam_hd; sysp != NULL; sysp = sysp->sys_next) {

		switch (fcn) {

		case SYS_FORCELOAD:
		if (sysp->sys_type == MOD_FORCELOAD) {
			name = sysp->sys_ptr;
			if (modload(NULL, name) == -1)
				cmn_err(CE_WARN, wmesg, name);
			/*
			 * The following works because it
			 * runs before autounloading is started!!
			 */
			modp = mod_find_by_filename(NULL, name);
			if (modp != NULL)
				modp->mod_loadflags |= MOD_NOAUTOUNLOAD;
			/*
			 * For drivers, attempt to install it.
			 */
			if (strncmp(sysp->sys_ptr, "drv", 3) == 0) {
				(void) ddi_install_driver(name + 4);
			}
		}
		break;

		case SYS_SET_KVAR:
		case SYS_SET_MVAR:
			if (sysp->sys_type == MOD_SET)
				sys_set_var(fcn, sysp, p);
			break;

		case SYS_CHECK_EXCLUDE:
			if (sysp->sys_type == MOD_EXCLUDE) {
				if (p == NULL || sysp->sys_ptr == NULL)
					return (0);
				if (strcmp((char *)p, sysp->sys_ptr) == 0)
					return (1);
			}
		}
	}
	param_check();

	return (0);
}

/*
 * Process the system file commands, by type.
 */
int
mod_sysctl_type(int type, int (*func)(struct sysparam *, void *), void *p)
{
	struct sysparam *sysp;
	int	err;

	for (sysp = sysparam_hd; sysp != NULL; sysp = sysp->sys_next)
		if (sysp->sys_type == type)
			if (err = (*(func))(sysp, p))
				return (err);
	return (0);
}


static char seterr[] = "Symbol %s has size of 0 in symbol table. %s";
static char assumption[] = "Assuming it is an 'int'";
static char defmsg[] = "Trying to set a variable that is of size %d";

static void set_int8_var(uintptr_t, struct sysparam *);
static void set_int16_var(uintptr_t, struct sysparam *);
static void set_int32_var(uintptr_t, struct sysparam *);
static void set_int64_var(uintptr_t, struct sysparam *);

static void
sys_set_var(int fcn, struct sysparam *sysp, void *p)
{
	uintptr_t symaddr;
	int size;

	if (fcn == SYS_SET_KVAR && sysp->sys_modnam == NULL) {
		symaddr = kobj_getelfsym(sysp->sys_ptr, NULL, &size);
	} else if (fcn == SYS_SET_MVAR) {
		if (sysp->sys_modnam == (char *)NULL ||
			strcmp(((struct modctl *)p)->mod_modname,
			    sysp->sys_modnam) != 0)
				return;
		symaddr = kobj_getelfsym(sysp->sys_ptr,
		    ((struct modctl *)p)->mod_mp, &size);
	} else
		return;

	if (symaddr != NULL) {
		switch (size) {
		case 1:
			set_int8_var(symaddr, sysp);
			break;
		case 2:
			set_int16_var(symaddr, sysp);
			break;
		case 0:
			cmn_err(CE_WARN, seterr, sysp->sys_ptr, assumption);
			/*FALLTHROUGH*/
		case 4:
			set_int32_var(symaddr, sysp);
			break;
		case 8:
			set_int64_var(symaddr, sysp);
			break;
		default:
			cmn_err(CE_WARN, defmsg, size);
			break;
		}
	} else {
		printf("sorry, variable '%s' is not defined in the '%s' ",
		    sysp->sys_ptr,
		    sysp->sys_modnam ? sysp->sys_modnam : "kernel");
		if (sysp->sys_modnam)
			printf("module");
		printf("\n");
	}
}

static void
set_int8_var(uintptr_t symaddr, struct sysparam *sysp)
{
	uint8_t uc = (uint8_t)sysp->sys_info;

	if (moddebug & MODDEBUG_LOADMSG)
		printf("OP: %x: param '%s' was '0x%" PRIx8
		    "' in module: '%s'.\n", sysp->sys_op, sysp->sys_ptr,
		    *(uint8_t *)symaddr, sysp->sys_modnam);

	switch (sysp->sys_op) {
	case SETOP_ASSIGN:
		*(uint8_t *)symaddr = uc;
		break;
	case SETOP_AND:
		*(uint8_t *)symaddr &= uc;
		break;
	case SETOP_OR:
		*(uint8_t *)symaddr |= uc;
		break;
	}

	if (moddebug & MODDEBUG_LOADMSG)
		printf("now it is set to '0x%" PRIx8 "'.\n",
		    *(uint8_t *)symaddr);
}

static void
set_int16_var(uintptr_t symaddr, struct sysparam *sysp)
{
	uint16_t us = (uint16_t)sysp->sys_info;

	if (moddebug & MODDEBUG_LOADMSG)
		printf("OP: %x: param '%s' was '0x%" PRIx16
		    "' in module: '%s'.\n", sysp->sys_op, sysp->sys_ptr,
		    *(uint16_t *)symaddr, sysp->sys_modnam);

	switch (sysp->sys_op) {
	case SETOP_ASSIGN:
		*(uint16_t *)symaddr = us;
		break;
	case SETOP_AND:
		*(uint16_t *)symaddr &= us;
		break;
	case SETOP_OR:
		*(uint16_t *)symaddr |= us;
		break;
	}

	if (moddebug & MODDEBUG_LOADMSG)
		printf("now it is set to '0x%" PRIx16 "'.\n",
		    *(uint16_t *)symaddr);
}

static void
set_int32_var(uintptr_t symaddr, struct sysparam *sysp)
{
	uint32_t ui = (uint32_t)sysp->sys_info;

	if (moddebug & MODDEBUG_LOADMSG)
		printf("OP: %x: param '%s' was '0x%" PRIx32
		    "' in module: '%s'.\n", sysp->sys_op, sysp->sys_ptr,
		    *(uint32_t *)symaddr, sysp->sys_modnam);

	switch (sysp->sys_op) {
	case SETOP_ASSIGN:
		*(uint32_t *)symaddr = ui;
		break;
	case SETOP_AND:
		*(uint32_t *)symaddr &= ui;
		break;
	case SETOP_OR:
		*(uint32_t *)symaddr |= ui;
		break;
	}

	if (moddebug & MODDEBUG_LOADMSG)
		printf("now it is set to '0x%" PRIx32 "'.\n",
		    *(uint32_t *)symaddr);
}

static void
set_int64_var(uintptr_t symaddr, struct sysparam *sysp)
{
	uint64_t ul = sysp->sys_info;

	if (moddebug & MODDEBUG_LOADMSG)
		printf("OP: %x: param '%s' was '0x%" PRIx64
		    "' in module: '%s'.\n", sysp->sys_op, sysp->sys_ptr,
		    *(uint64_t *)symaddr, sysp->sys_modnam);

	switch (sysp->sys_op) {
	case SETOP_ASSIGN:
		*(uint64_t *)symaddr = ul;
		break;
	case SETOP_AND:
		*(uint64_t *)symaddr &= ul;
		break;
	case SETOP_OR:
		*(uint64_t *)symaddr |= ul;
		break;
	}

	if (moddebug & MODDEBUG_LOADMSG)
		printf("now it is set to '0x%" PRIx64 "'.\n",
		    *(uint64_t *)symaddr);
}

/*
 * The next item on the line is a string value. Allocate memory for
 * it and copy the string. Return 1, and set arg ptr to newly allocated
 * and initialized buffer, or NULL if an error occurs.
 */
static int
get_string(u_longlong_t *llptr, char *tchar)
{
	register char *cp;
	register char *start = (char *)0;
	register int len = 0;

	len = strlen(tchar);
	start = tchar;
	/* copy string */
	cp = vmem_alloc(mod_sysfile_arena, len + 1, VM_SLEEP);
	bzero(cp, len + 1);
	*llptr = (u_longlong_t)cp;
	for (; len > 0; len--) {
		/* convert some common escape sequences */
		if (*start == '\\') {
			switch (*(start + 1)) {
			case 't':
				/* tab */
				*cp++ = '\t';
				len--;
				start += 2;
				break;
			case 'n':
				/* new line */
				*cp++ = '\n';
				len--;
				start += 2;
				break;
			case 'b':
				/* back space */
				*cp++ = '\b';
				len--;
				start += 2;
				break;
			default:
				/* simply copy it */
				*cp++ = *start++;
				break;
			}
		} else
			*cp++ = *start++;
	}
	*cp = '\0';
	return (1);
}

/*
 * get a decimal octal or hex number. Handle '~' for one's complement.
 */
int
getvalue(char *token, u_longlong_t *valuep)
{
	register int radix;
	register u_longlong_t retval = 0;
	register int onescompl = 0;
	register int negate = 0;
	register char c;

	if (*token == '~') {
		onescompl++; /* perform one's complement on result */
		token++;
	} else if (*token == '-') {
		negate++;
		token++;
	}
	if (*token == '0') {
		token++;
		c = *token;

		if (c == '\0') {
			*valuep = 0;	/* value is 0 */
			return (0);
		}

		if (c == 'x' || c == 'X') {
			radix = 16;
			token++;
		} else
			radix = 8;
	} else
		radix = 10;

	while ((c = *token++)) {
		switch (radix) {
		case 8:
			if (c >= '0' && c <= '7')
				c -= '0';
			else
				return (-1);	/* invalid number */
			retval = (retval << 3) + c;
			break;
		case 10:
			if (c >= '0' && c <= '9')
				c -= '0';
			else
				return (-1);	/* invalid number */
			retval = (retval * 10) + c;
			break;
		case 16:
			if (c >= 'a' && c <= 'f')
				c = c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				c = c - 'A' + 10;
			else if (c >= '0' && c <= '9')
				c -= '0';
			else
				return (-1);	/* invalid number */
			retval = (retval << 4) + c;
			break;
		}
	}
	if (onescompl)
		retval = ~retval;
	if (negate)
		retval = -retval;
	*valuep = retval;
	return (0);
}

/*
 * set parameters that can be set early during initialization.
 */
static void
setparams()
{
	struct sysparam *sysp;
	struct bootobj *bootobjp;

	for (sysp = sysparam_hd; sysp != NULL; sysp = sysp->sys_next) {

		if (sysp->sys_type == MOD_MODDIR) {
			default_path = sysp->sys_ptr;
			continue;
		}

		if (sysp->sys_type == MOD_ROOTDEV ||
		    sysp->sys_type == MOD_ROOTFS)
			bootobjp = &rootfs;
		else
			bootobjp = &swapfile;

		switch (sysp->sys_type) {
		case MOD_ROOTDEV:
		case MOD_SWAPDEV:
			bootobjp->bo_flags |= BO_VALID;
			(void) copystr(sysp->sys_ptr, bootobjp->bo_name,
			    BO_MAXOBJNAME, NULL);
			break;
		case MOD_ROOTFS:
		case MOD_SWAPFS:
			bootobjp->bo_flags |= BO_VALID;
			(void) copystr(sysp->sys_ptr, bootobjp->bo_fstype,
			    BO_MAXOBJNAME, NULL);
			break;

		default:
			break;
		}
	}
}

/*
 * clean up after an error.
 */
void
hwc_free(struct hwc_spec *hwcp)
{
	register char *name;
	extern void e_ddi_prop_list_delete(ddi_prop_t *);

	if ((name = hwcp->hwc_parent_name) != NULL)
		kmem_free(name, strlen(name) + 1);
	if ((name = hwcp->hwc_class_name) != NULL)
		kmem_free(name, strlen(name) + 1);
	if ((name = hwcp->hwc_proto->proto_devi_name) != NULL)
		kmem_free(name, strlen(name) + 1);
	e_ddi_prop_list_delete(hwcp->hwc_proto->proto_devi_sys_prop_ptr);
	kmem_free(hwcp->hwc_proto, sizeof (proto_dev_info_t));
	kmem_free(hwcp, sizeof (struct hwc_spec));
}

struct val_list {
	struct val_list *val_next;
	int		val_type;
	int		val_size;
	union {
		char *string;
		int integer;
	} val;
};

static void
add_val(struct val_list **val_listp, int val_type, caddr_t val)
{
	register struct val_list *new_val, *listp = *val_listp;

	new_val = kmem_alloc(sizeof (struct val_list), KM_SLEEP);
	new_val->val_next = NULL;
	if ((new_val->val_type = val_type) == 0) {
		new_val->val_size = strlen((char *)val) + 1;
		new_val->val.string = (char *)val;
	} else {
		new_val->val_size = sizeof (int);
		new_val->val.integer = (int)val;
	}

	if (listp) {
		while (listp->val_next) {
			listp = listp->val_next;
		}
		listp->val_next = new_val;
	} else {
		*val_listp = new_val;
	}
}

static void
make_prop(struct _buf *file, dev_info_t *devi, char *name, struct val_list *val)
{
	register int propcnt = 0, val_type;
	register struct val_list *vl, *tvl;
	caddr_t valbuf = NULL;
	register char **valsp;
	register int *valip;

	if (name == NULL)
		return;

#ifdef DEBUG
	parse_debug(NULL, "%s", name);
#endif
	if (val) {
		for (vl = val, val_type = vl->val_type; vl; vl = vl->val_next) {
			if (val_type != vl->val_type) {
				cmn_err(CE_WARN, "Mixed types in value list");
				return;
			}
			propcnt++;
		}

		vl = val;

		if (val_type == 1) {
			valip = (int *)kmem_alloc(
			    (propcnt * sizeof (int)), KM_SLEEP);
			valbuf = (caddr_t)valip;
			while (vl) {
				tvl = vl;
				vl = vl->val_next;
#ifdef DEBUG
				parse_debug(NULL, " %x",  tvl->val.integer);
#endif
				*valip = tvl->val.integer;
				valip++;
				kmem_free(tvl, sizeof (struct val_list));
			}
			/* restore valip */
			valip = (int *)valbuf;

			/* create the property */
			if (e_ddi_prop_update_int_array(DDI_DEV_T_NONE, devi,
			    name, valip, propcnt) != DDI_PROP_SUCCESS) {
				file_err(CE_WARN, file,
				    "cannot create property %s", name);
			}
			/* cleanup */
			kmem_free(valip, (propcnt * sizeof (int)));
		} else if (val_type == 0) {
			valsp = (char **)kmem_alloc(
			    ((propcnt + 1) * sizeof (char *)), KM_SLEEP);
			valbuf = (caddr_t)valsp;
			while (vl) {
				tvl = vl;
				vl = vl->val_next;
#ifdef DEBUG
				parse_debug(NULL, " %s", tvl->val.string);
#endif
				*valsp = tvl->val.string;
				valsp++;
			}
			/* terminate array with NULL */
			*valsp = NULL;

			/* restore valsp */
			valsp = (char **)valbuf;

			/* create the property */
			if (e_ddi_prop_update_string_array(DDI_DEV_T_NONE,
			    devi, name, valsp, propcnt)
			    != DDI_PROP_SUCCESS) {
				file_err(CE_WARN, file,
				    "cannot create property %s", name);
			}
			/* Clean up */
			vl = val;
			while (vl) {
				tvl = vl;
				vl = vl->val_next;
				kmem_free(tvl->val.string, tvl->val_size);
				kmem_free(tvl, sizeof (struct val_list));
			}
			kmem_free(valsp, ((propcnt + 1) * sizeof (char *)));
		} else {
			cmn_err(CE_WARN, "Invalid property type");
			return;
		}
	} else {
		/*
		 * No value was passed in with property so we will assume
		 * it is a "boolean" property and create an integer
		 * property with 0 value.
		 */
#ifdef DEBUG
		parse_debug(NULL, "\n");
#endif
		if (e_ddi_prop_update_int(DDI_DEV_T_NONE, devi, name, 0)
		    != DDI_PROP_SUCCESS) {
			file_err(CE_WARN, file,
			    "cannot create property %s", name);
		}
	}
	kmem_free(name, strlen(name) + 1);
}

static char omit_err[] = "(the ';' may have been omitted on previous spec!)";
static char prnt_err[] = "'parent' property already specified";
static char nm_err[] = "'name' property already specified";
static char class_err[] = "'class' property already specified";

typedef enum {
	hwc_begin, parent, drvname, drvclass, prop,
	parent_equals, name_equals, drvclass_equals,
	parent_equals_string, name_equals_string,
	drvclass_equals_string,
	prop_equals, prop_equals_string, prop_equals_integer,
	prop_equals_string_comma, prop_equals_integer_comma
} hwc_state_t;

static struct hwc_spec *
get_hwc_spec(struct _buf *file, char *tokbuf)
{
	register char *prop_name, *string, *class_string;
	register token_t token;
	register struct hwc_spec *hwcp;
	register struct dev_info *devi;
	struct val_list *val_list;
	hwc_state_t state;
	u_longlong_t ival;

	hwcp = kmem_zalloc(sizeof (*hwcp), KM_SLEEP);
	devi = kmem_zalloc(sizeof (*devi), KM_SLEEP);
	hwcp->hwc_proto = kmem_zalloc(sizeof (proto_dev_info_t), KM_SLEEP);

	state = hwc_begin;
	token = NAME;
	prop_name = NULL;
	val_list = NULL;
	string = NULL;
	do {
		switch (token) {
		case NAME:
			switch (state) {
			case prop:
			case prop_equals_string:
			case prop_equals_integer:
				make_prop(file, (dev_info_t *)devi,
				    prop_name, val_list);
				prop_name = NULL;
				val_list = NULL;
				/*FALLTHROUGH*/
			case hwc_begin:
				if (strcmp(tokbuf, "PARENT") == 0 ||
				    strcmp(tokbuf, "parent") == 0) {
					state = parent;
				} else if (strcmp(tokbuf, "NAME") == 0 ||
				    strcmp(tokbuf, "name") == 0) {
					state = drvname;
				} else if (strcmp(tokbuf, "CLASS") == 0 ||
				    strcmp(tokbuf, "class") == 0) {
					state = drvclass;
					prop_name = kmem_alloc(strlen(tokbuf) +
						1, KM_SLEEP);
					(void) strcpy(prop_name, tokbuf);
				} else {
					state = prop;
					prop_name = kmem_alloc(strlen(tokbuf) +
						1, KM_SLEEP);
					(void) strcpy(prop_name, tokbuf);
				}
				break;
			default:
				file_err(CE_WARN, file, tok_err, tokbuf);
			}
			break;
		case EQUALS:
			switch (state) {
			case drvname:
				state = name_equals;
				break;
			case parent:
				state = parent_equals;
				break;
			case drvclass:
				state = drvclass_equals;
				break;
			case prop:
				state = prop_equals;
				break;
			default:
				file_err(CE_WARN, file, tok_err, tokbuf);
			}
			break;
		case STRING:
			string = kmem_alloc(strlen(tokbuf) + 1, KM_SLEEP);
			(void) strcpy(string, tokbuf);
			switch (state) {
			case name_equals:
				if (ddi_get_name((dev_info_t *)devi)) {
					file_err(CE_WARN, file, "%s %s",
						nm_err, omit_err);
					goto bad;
				}
				devi->devi_name = string;
				state = hwc_begin;
				break;
			case parent_equals:
				if (hwcp->hwc_parent_name) {
					file_err(CE_WARN, file, "%s %s",
						prnt_err, omit_err);
					goto bad;
				}
				hwcp->hwc_parent_name = string;
				state = hwc_begin;
				break;
			case drvclass_equals:
				if (hwcp->hwc_class_name) {
					file_err(CE_WARN, file, class_err);
					goto bad;
				}
				class_string = kmem_alloc(strlen(string) + 1,
					KM_SLEEP);
				(void) strcpy(class_string, string);
				hwcp->hwc_class_name = class_string;
				/*FALLTHROUGH*/
			case prop_equals:
			case prop_equals_string_comma:
				add_val(&val_list, 0, string);
				state = prop_equals_string;
				break;
			default:
				file_err(CE_WARN, file, tok_err, tokbuf);
			}
			break;
		case HEXVAL:
		case DECVAL:
			switch (state) {
			case prop_equals:
			case prop_equals_integer_comma:
				(void) getvalue(tokbuf, &ival);
				add_val(&val_list, 1, (caddr_t)ival);
				state = prop_equals_integer;
				break;
			default:
				file_err(CE_WARN, file, tok_err, tokbuf);
			}
			break;
		case COMMA:
			switch (state) {
			case prop_equals_string:
				state = prop_equals_string_comma;
				break;
			case prop_equals_integer:
				state = prop_equals_integer_comma;
				break;
			default:
				file_err(CE_WARN, file, tok_err, tokbuf);
			}
			break;
		case NEWLINE:
			kobj_newline(file);
			break;
		case POUND:
			find_eol(file);
			break;
		case EOF:
			file_err(CE_WARN, file, "Unexpected EOF");
			goto bad;
		default:
			file_err(CE_WARN, file, tok_err, tokbuf);
			goto bad;
		}
	} while ((token = lex(file, tokbuf)) != SEMICOLON);

	switch (state) {
	case prop:
	case prop_equals_string:
	case prop_equals_integer:
		make_prop(file, (dev_info_t *)devi,
			prop_name, val_list);
		break;

	case hwc_begin:
		break;
	default:
		file_err(CE_WARN, file, "Unexpected end of line");
		break;
	}

	/* copy 2 relevant members of devi to hwc_proto */
	hwcp->hwc_proto->proto_devi_sys_prop_ptr = devi->devi_sys_prop_ptr;
	hwcp->hwc_proto->proto_devi_name = devi->devi_name;

	kmem_free(devi, sizeof (struct dev_info));

	return (hwcp);

bad:
	if (string) {
		kmem_free(string, strlen(string) + 1);
	}
	if (hwcp) {
		hwc_free(hwcp);
	}
	if (devi) {
		kmem_free(devi, sizeof (struct dev_info));
	}
	return (NULL);
}

/*
 * This is the primary kernel interface to parse driver .conf
 * files.
 *
 * Yet another bigstk thread handoff due to deep kernel stacks when booting
 * cache-only-clients.
 */
struct hwc_spec *
hwc_parse(register char *fname)
{
	struct hwc_parse_mt *pltp = hwc_parse_mtalloc(fname);
	struct hwc_spec *hwcp;

	if (curthread != &t0 && thread_create(NULL, DEFAULTSTKSZ * 2,
	    hwc_parse_thread, (caddr_t)pltp, 0, &p0, TS_RUN,
	    MAXCLSYSPRI) != NULL)
		sema_p(&pltp->sema);
	else
		pltp->rv = hwc_parse_now(fname);
	hwcp = pltp->rv;
	hwc_parse_mtfree(pltp);
	return (hwcp);
}

/*
 * Calls to hwc_parse() are handled off to this routine in a separate
 * thread.
 */
static void
hwc_parse_thread(struct hwc_parse_mt *pltp)
{
	kmutex_t	cpr_lk;
	callb_cpr_t	cpr_i;

	mutex_init(&cpr_lk, NULL, MUTEX_DEFAULT, NULL);
	CALLB_CPR_INIT(&cpr_i, &cpr_lk, callb_generic_cpr, "hwc_parse");

	/*
	 * load and parse the .conf file
	 * return the hwc_spec list (if any) to the creator of this thread
	 */
	pltp->rv = hwc_parse_now(pltp->name);
	sema_v(&pltp->sema);
	mutex_enter(&cpr_lk);
	CALLB_CPR_EXIT(&cpr_i);
	mutex_destroy(&cpr_lk);
	thread_exit();
}

/*
 * allocate and initialize a hwc_parse thread control structure
 */
static struct hwc_parse_mt *
hwc_parse_mtalloc(char *name)
{
	struct hwc_parse_mt *pltp = kmem_zalloc(sizeof (*pltp), KM_SLEEP);

	ASSERT(name != NULL);

	pltp->name = kmem_alloc(strlen(name) + 1, KM_SLEEP);
	bcopy(name, pltp->name, strlen(name) + 1);

	sema_init(&pltp->sema, 0, NULL, SEMA_DEFAULT, NULL);
	return (pltp);
}

/*
 * free a hwc_parse thread control structure
 */
static void
hwc_parse_mtfree(struct hwc_parse_mt *pltp)
{
	sema_destroy(&pltp->sema);

	kmem_free(pltp->name, strlen(pltp->name) + 1);
	kmem_free(pltp, sizeof (*pltp));
}

/*
 * hwc_parse -- parse an hwconf file.  Ignore error lines and parse
 * as much as possible.
 */
static struct hwc_spec *
hwc_parse_now(register char *fname)
{
	register struct _buf *file;
	register struct hwc_spec *hwcp, *hwcp1;
	register struct hwc_spec *hwcp_head, *hwcp_tail;
	register char *tokval;
	register token_t token;

	/*
	 * Don't use kobj_open_path's use_moddir_suffix option, we only
	 * expect to find conf files in the base module directory, not
	 * an ISA-specific subdirectory.
	 */
	if ((file = kobj_open_path(fname, 1, 0)) == (struct _buf *)-1) {
		if (moddebug & MODDEBUG_ERRMSG)
			cmn_err(CE_WARN, "Cannot open %s", fname);
		return (NULL);
	}

	/*
	 * Initialize variables
	 */
	tokval = kmem_alloc(MAX_HWC_LINESIZE, KM_SLEEP);
	hwcp_head = NULL;
	hwcp_tail = NULL;

	while ((token = lex(file, tokval)) != EOF) {
		switch (token) {
		case POUND:
			/*
			 * Skip comments.
			 */
			find_eol(file);
			break;
		case NAME:
			if (hwcp = get_hwc_spec(file, tokval)) {
				/*
				 * No name, class, and parent indicates
				 * that this really a global property
				 * entry, which will be processed later.
				 * However, it still an error to specify
				 * a name and no parent/class, or a
				 * parent/class and no name.
				 * class specs are added to hwc list
				 * with major number -1 (by add_spec())
				 * and replicated on demand.
				 */
				if ((hwcp->hwc_parent_name == NULL &&
				    hwcp->hwc_class_name == NULL) &&
				    (hwcp->hwc_proto->proto_devi_name !=
				    NULL)) {
					file_err(CE_WARN, file,
					    "missing parent or class "
					    "attribute");
					hwc_free(hwcp);
					continue;
				} else if ((hwcp->hwc_parent_name != NULL ||
				    hwcp->hwc_class_name != NULL) &&
				    (hwcp->hwc_proto->proto_devi_name ==
				    NULL)) {
					file_err(CE_WARN, file,
					    "missing name attribute");
					hwc_free(hwcp);
					continue;
				}
				/*
				 * link this hwcp into the list of the hwc_specs
				 */
				if (hwcp_head == NULL)
					hwcp_head = hwcp;
				else
					hwcp_tail->hwc_next = hwcp;

				for (hwcp1 = hwcp; hwcp1->hwc_next != NULL;
				    hwcp1 = hwcp1->hwc_next)
					;
				hwcp_tail = hwcp1;
			}
			break;
		case NEWLINE:
			kobj_newline(file);
			break;
		default:
			/* Error? */
			break;
		}
	}
	/*
	 * XXX - Check for clean termination.
	 */
	kmem_free(tokval, MAX_HWC_LINESIZE);
	kobj_close_file(file);
	return (hwcp_head);
}

void
make_aliases(struct bind **bhash)
{
	enum {
		AL_NEW, AL_DRVNAME, AL_DRVNAME_COMMA, AL_ALIAS, AL_ALIAS_COMMA
	} state;

	struct _buf *file;
	char tokbuf[MAXNAMELEN];
	char drvbuf[MAXNAMELEN];
	token_t token;
	major_t major;
	static char dupwarn[] = "!Driver alias \"%s\" conflicts with "
	    "an existing driver name or alias.";

	if ((file = kobj_open_file(dafile)) == (struct _buf *)-1)
		return;

	state = AL_NEW;
	major = (major_t)-1;
	while ((token = lex(file, tokbuf)) != EOF) {
		switch (token) {
		case POUND:
			state = AL_NEW;
			find_eol(file);
			break;
		case NAME:
		case STRING:
			switch (state) {
			case AL_NEW:
				(void) strcpy(drvbuf, tokbuf);
				state = AL_DRVNAME;
				break;
			case AL_DRVNAME_COMMA:
				(void) strcat(drvbuf, tokbuf);
				state = AL_DRVNAME;
				break;
			case AL_ALIAS_COMMA:
				(void) strcat(drvbuf, tokbuf);
				state = AL_ALIAS;
				break;
			case AL_DRVNAME:
				major = mod_name_to_major(drvbuf);
				if (major == (major_t)-1) {
					find_eol(file);
					state = AL_NEW;
				} else {
					(void) strcpy(drvbuf, tokbuf);
					state = AL_ALIAS;
				}
				break;
			case AL_ALIAS:
				if (make_mbind(drvbuf, major, NULL, bhash)
				    != 0) {
					cmn_err(CE_WARN, dupwarn, drvbuf);
				}
				break;
			}
			break;
		case COMMA:
			(void) strcat(drvbuf, tokbuf);
			switch (state) {
			case AL_DRVNAME:
				state = AL_DRVNAME_COMMA;
				break;
			case AL_ALIAS:
				state = AL_ALIAS_COMMA;
				break;
			default:
				file_err(CE_WARN, file, tok_err, tokbuf);
			}
			break;
		case NEWLINE:
			if (state == AL_ALIAS) {
				if (make_mbind(drvbuf, major, NULL, bhash)
				    != 0) {
					cmn_err(CE_WARN, dupwarn, drvbuf);
				}
			} else if (state != AL_NEW) {
				file_err(CE_WARN, file, "Missing alias for %s",
				    drvbuf);
			}

			kobj_newline(file);
			state = AL_NEW;
			major = (major_t)-1;
			break;
		default:
			file_err(CE_WARN, file, tok_err, tokbuf);
		}
	}
	if ((state == AL_ALIAS) &&
	    (make_mbind(drvbuf, major, NULL, bhash) != 0)) {
		cmn_err(CE_WARN, dupwarn, drvbuf);
	}

	kobj_close_file(file);
}


int
read_binding_file(char *bindfile, struct bind **bhash)
{
	enum {
		B_NEW, B_NAME, B_VAL, B_BIND_NAME
	} state;
	struct _buf *file;
	char tokbuf[MAXNAMELEN];
	token_t token;
	int maxnum = 0;
	char *bind_name = NULL, *name = NULL, *bn = NULL;
	u_longlong_t val;

	static char num_err[] = "Missing number on preceding line?";
	static char dupwarn[] = "!The binding file entry \"%s %u\" conflicts "
	    "with a previous entry";

	clear_binding_hash(bhash);

	if ((file = kobj_open_file(bindfile)) == (struct _buf *)-1)
		cmn_err(CE_PANIC, "read_binding_file: %s file not found",
		    bindfile);

	state = B_NEW;

	while ((token = lex(file, tokbuf)) != EOF) {
		switch (token) {
		case POUND:
			state = B_NEW;
			find_eol(file);
			break;
		case NAME:
		case STRING:
			switch (state) {
			case B_NEW:
				/*
				 * This case is for the first name and
				 * possibly only name in an entry.
				 */
				ASSERT(name == NULL);
				name = kmem_alloc(strlen(tokbuf) + 1, KM_SLEEP);
				(void) strcpy(name, tokbuf);
				state = B_NAME;
				break;
			case B_VAL:
				/*
				 * This case is for a second name, which
				 * would be the binding name if the first
				 * name was actually a generic name.
				 */
				ASSERT(bind_name == NULL);
				bind_name = kmem_alloc(strlen(tokbuf) + 1,
				    KM_SLEEP);
				(void) strcpy(bind_name, tokbuf);
				state = B_BIND_NAME;
				break;
			default:
				file_err(CE_WARN, file, num_err);
			}
			break;
		case HEXVAL:
		case DECVAL:
			if (state != B_NAME) {
				file_err(CE_WARN, file, "Missing name?");
				state = B_NEW;
				continue;
			}
			(void) getvalue(tokbuf, &val);
			if (val > (u_longlong_t)INT_MAX) {
				file_err(CE_WARN, file,
				    "value %llu too large", val);
				state = B_NEW;
				continue;
			}
			state = B_VAL;
			break;
		case NEWLINE:
			if ((state == B_BIND_NAME) || (state == B_VAL)) {
				if (state == B_BIND_NAME)
					bn = bind_name;
				else
					bn = NULL;

				if (make_mbind(name, (int)val, bn, bhash) == 0)
					maxnum = MAX((int)val, maxnum);
				else
					file_err(CE_WARN, file, dupwarn,
					    name, (uint_t)val);

			} else if (state != B_NEW)
				file_err(CE_WARN, file, "Syntax error?");

			if (name) {
				kmem_free(name, strlen(name) + 1);
				name = NULL;
			}
			if (bind_name) {
				kmem_free(bind_name, strlen(bind_name) + 1);
				bind_name = NULL;
			}
			state = B_NEW;
			kobj_newline(file);
			break;
		default:
			file_err(CE_WARN, file, "Missing name/number?");
			break;
		}
	}

	ASSERT(name == NULL);		/* any leaks? */
	ASSERT(bind_name == NULL);

	kobj_close_file(file);
	return (maxnum);
}

/*
 * read_dacf_binding_file()
 * 	Read the /etc/dacf.conf file and build the dacf_rule_t database from it.
 *
 * The syntax of a line in the dacf.conf file is:
 *   dev-spec 	[module:]op-set	operation options 	[config-args];
 *
 * Where:
 *   	1. dev-spec is of the format: name="data"
 *   	2. operation is the operation that this rule matches. (i.e. pre-detach)
 *   	3. options is a comma delimited list of options (i.e. debug,foobar)
 *   	4. config-data is a whitespace delimited list of the format: name="data"
 */
int
read_dacf_binding_file(char *filename)
{
	enum {
		DACF_BEGIN,
		/* minor_nodetype="ddi_mouse:serial" */
		DACF_NT_SPEC, DACF_NT_EQUALS, DACF_NT_DATA,
		/* consconfig:mouseconfig */
		DACF_MN_MODNAME, DACF_MN_COLON, DACF_MN_OPSET,
		/* op */
		DACF_OP_NAME,
		/* [ option1, option2, option3... | - ] */
		DACF_OPT_OPTION, DACF_OPT_COMMA, DACF_OPT_END,
		/* argname1="argval1" argname2="argval2" ... */
		DACF_OPARG_SPEC, DACF_OPARG_EQUALS, DACF_OPARG_DATA,
		DACF_ERR, DACF_ERR_NEWLINE, DACF_COMMENT
	} state = DACF_BEGIN;

	struct _buf *file;
	char *fname;
	token_t token;

	char tokbuf[MAXNAMELEN];
	char mn_modname_buf[MAXNAMELEN], *mn_modnamep = NULL;
	char mn_opset_buf[MAXNAMELEN], *mn_opsetp = NULL;
	char nt_data_buf[MAXNAMELEN], *nt_datap = NULL;
	char arg_spec_buf[MAXNAMELEN];

	uint_t opts = 0;
	dacf_devspec_t nt_spec_type = DACF_DS_ERROR;

	dacf_arg_t *arg_list = NULL;
	dacf_opid_t opid = DACF_OPID_ERROR;
	int done = 0;

	static char w_syntax[] = "'%s' unexpected";
	static char w_equals[] = "'=' is illegal in the current context";
	static char w_baddevspec[] = "device specification '%s' unrecognized";
	static char w_badop[] = "operation '%s' unrecognized";
	static char w_badopt[] = "option '%s' unrecognized, ignoring";
	static char w_newline[] = "rule is incomplete";
	static char w_insert[] = "failed to register rule";
	static char w_comment[] = "'#' not allowed except at start of line";
	static char w_dupargs[] =
	    "argument '%s' duplicates a previous argument, skipping";
	static char w_nt_empty[] = "empty device specification not allowed";

	if (filename == NULL) {
		fname = dacffile;	/* default binding file */
	} else {
		fname = filename;	/* user specified */
	}

	if ((file = kobj_open_file(fname)) == (struct _buf *)-1) {
		return (ENOENT);
	}

	if (dacfdebug & DACF_DBG_MSGS) {
		printf("dacf debug: clearing rules database\n");
	}

	mutex_enter(&dacf_lock);
	dacf_clear_rules();

	if (dacfdebug & DACF_DBG_MSGS) {
		printf("dacf debug: parsing %s\n", fname);
	}

	while (!done) {
		token = lex(file, tokbuf);

		switch (token) {
		case POUND:	/* comment line */
			if (state != DACF_BEGIN) {
				file_err(CE_WARN, file, w_comment);
				state = DACF_ERR;
				break;
			}
			state = DACF_COMMENT;
			find_eol(file);
			break;

		case EQUALS:
			switch (state) {
			case DACF_NT_SPEC:
				state = DACF_NT_EQUALS;
				break;
			case DACF_OPARG_SPEC:
				state = DACF_OPARG_EQUALS;
				break;
			default:
				file_err(CE_WARN, file, w_equals);
				state = DACF_ERR;
			}
			break;

		case NAME:
			switch (state) {
			case DACF_BEGIN:
				nt_spec_type = dacf_get_devspec(tokbuf);
				if (nt_spec_type == DACF_DS_ERROR) {
					file_err(CE_WARN, file, w_baddevspec,
					    tokbuf);
					state = DACF_ERR;
					break;
				}
				state = DACF_NT_SPEC;
				break;
			case DACF_NT_DATA:
				(void) strncpy(mn_modname_buf, tokbuf,
				    sizeof (mn_modname_buf));
				mn_modnamep = mn_modname_buf;
				state = DACF_MN_MODNAME;
				break;
			case DACF_MN_MODNAME:
				/*
				 * This handles the 'optional' modname.
				 * What we thought was the modname is really
				 * the op-set.  So it is copied over.
				 */
				ASSERT(mn_modnamep);
				(void) strncpy(mn_opset_buf, mn_modnamep,
				    sizeof (mn_opset_buf));
				mn_opsetp = mn_opset_buf;
				mn_modnamep = NULL;
				/*
				 * Now, the token we just read is the opset,
				 * so look that up and fill in opid
				 */
				if ((opid = dacf_get_op(tokbuf)) ==
				    DACF_OPID_ERROR) {
					file_err(CE_WARN, file, w_badop,
					    tokbuf);
					state = DACF_ERR;
					break;
				}
				state = DACF_OP_NAME;
				break;
			case DACF_MN_COLON:
				(void) strncpy(mn_opset_buf, tokbuf,
				    sizeof (mn_opset_buf));
				mn_opsetp = mn_opset_buf;
				state = DACF_MN_OPSET;
				break;
			case DACF_MN_OPSET:
				if ((opid = dacf_get_op(tokbuf)) ==
				    DACF_OPID_ERROR) {
					file_err(CE_WARN, file, w_badop,
					    tokbuf);
					state = DACF_ERR;
					break;
				}
				state = DACF_OP_NAME;
				break;
			case DACF_OP_NAME:
				/*
				 * This case is just like DACF_OPT_COMMA below,
				 * but we check for the sole '-' argument
				 */
				if (strcmp(tokbuf, "-") == 0) {
					state = DACF_OPT_END;
					break;
				}
				/*FALLTHROUGH*/
			case DACF_OPT_COMMA:
				/*
				 * figure out what option was given, but don't
				 * make a federal case if invalid, just skip it
				 */
				if (dacf_getopt(tokbuf, &opts) != 0) {
					file_err(CE_WARN, file, w_badopt,
					    tokbuf);
				}
				state = DACF_OPT_OPTION;
				break;
			case DACF_OPT_END:
			case DACF_OPT_OPTION:
			case DACF_OPARG_DATA:
				(void) strncpy(arg_spec_buf, tokbuf,
				    sizeof (arg_spec_buf));
				state = DACF_OPARG_SPEC;
				break;
			case DACF_OPARG_EQUALS:
				/*
				 * Add the arg.  Warn if it's a duplicate
				 */
				if (dacf_arg_insert(&arg_list, arg_spec_buf,
				    tokbuf) != 0) {
					file_err(CE_WARN, file, w_dupargs,
					    arg_spec_buf);
				}
				state = DACF_OPARG_DATA;
				break;
			default:
				file_err(CE_WARN, file, w_syntax, tokbuf);
				state = DACF_ERR;
				break;
			}
			break;

		case STRING:
			/*
			 * We need to check to see if the string has a \n in it.
			 * If so, we had an unmatched " mark error, and lex has
			 * already emitted an error for us, so we need to enter
			 * the error state.  Stupid lex.
			 */
			if (strchr(tokbuf, '\n')) {
				state = DACF_ERR;
				break;
			}
			switch (state) {
			case DACF_NT_EQUALS:
				if (strlen(tokbuf) == 0) {
					file_err(CE_WARN, file, w_nt_empty);
					state = DACF_ERR;
					break;
				}
				state = DACF_NT_DATA;
				nt_datap = nt_data_buf;
				(void) strncpy(nt_datap, tokbuf,
				    sizeof (nt_data_buf));
				break;
			case DACF_OPARG_EQUALS:
				/*
				 * Add the arg.  Warn if it's a duplicate
				 */
				if (dacf_arg_insert(&arg_list, arg_spec_buf,
				    tokbuf) != 0) {
					file_err(CE_WARN, file, w_dupargs,
					    arg_spec_buf);
				}
				state = DACF_OPARG_DATA;
				break;
			default:
				file_err(CE_WARN, file, w_syntax, tokbuf);
				state = DACF_ERR;
				break;
			}
			break;

		case COMMA:
			switch (state) {
			case DACF_OPT_OPTION:
				state = DACF_OPT_COMMA;
				break;
			default:
				file_err(CE_WARN, file, w_syntax, ",");
				state = DACF_ERR;
				break;
			}
			break;

		case COLON:
			if (state == DACF_MN_MODNAME)
				state = DACF_MN_COLON;
			else {
				file_err(CE_WARN, file, w_syntax, ":");
				state = DACF_ERR;
			}
			break;

		case EOF:
			done = 1;
			/*FALLTHROUGH*/
		case NEWLINE:
			if (state == DACF_COMMENT || state == DACF_BEGIN) {
				state = DACF_BEGIN;
				kobj_newline(file);
				break;
			}
			if ((state != DACF_OPT_OPTION) &&
			    (state != DACF_OPARG_DATA) &&
			    (state != DACF_OPT_END)) {
				file_err(CE_WARN, file, w_newline);
				/*
				 * We can't just do DACF_ERR here, since we'll
				 * wind up eating the _next_ newline if so.
				 */
				state = DACF_ERR_NEWLINE;
				kobj_newline(file);
				break;
			}

			/*
			 * insert the rule.
			 */
			if (dacf_rule_insert(nt_spec_type, nt_datap,
			    mn_modnamep, mn_opsetp, opid, opts, arg_list) < 0) {
				/*
				 * We can't just do DACF_ERR here, since we'll
				 * wind up eating the _next_ newline if so.
				 */
				file_err(CE_WARN, file, w_insert);
				state = DACF_ERR_NEWLINE;
				kobj_newline(file);
				break;
			}

			state = DACF_BEGIN;
			kobj_newline(file);
			break;

		default:
			file_err(CE_WARN, file, w_syntax, tokbuf);
			break;
		} /* switch */

		/*
		 * Clean up after ourselves, either after a line has terminated
		 * successfully or because of a syntax error; or when we reach
		 * EOF (remember, we may reach EOF without being 'done' with
		 * handling a particular line).
		 */
		if (state == DACF_ERR) {
			find_eol(file);
		}
		if ((state == DACF_BEGIN) || (state == DACF_ERR) ||
		    (state == DACF_ERR_NEWLINE) || done) {
			nt_datap = NULL;
			mn_modnamep = mn_opsetp = NULL;
			opts = 0;
			opid = DACF_OPID_ERROR;
			nt_spec_type = DACF_DS_ERROR;
			dacf_arglist_delete(&arg_list);
			state = DACF_BEGIN;
		}
	} /* while */

	if (dacfdebug & DACF_DBG_MSGS) {
		printf("\ndacf debug: done!\n");
	}

	mutex_exit(&dacf_lock);

	kobj_close_file(file);
	return (0);
}

void
add_class(char *exporter, char *class)
{
	struct hwc_class *hcl;

	hcl = kmem_zalloc(sizeof (struct hwc_class), KM_SLEEP);
	hcl->class_exporter = kmem_alloc(strlen(exporter) + 1, KM_SLEEP);
	hcl->class = kmem_alloc(strlen(class) + 1, KM_SLEEP);
	(void) strcpy(hcl->class_exporter, exporter);
	(void) strcpy(hcl->class, class);
	hcl->class_next = hcl_head;
	hcl_head = hcl;
}

typedef enum {
	class_begin, new_class, exporter_
} class_state_t;

void
read_class_file(void)
{
	struct _buf *file;
	struct hwc_class *hcl, *hcl1;
	char tokbuf[MAXNAMELEN];
	register class_state_t state;
	register token_t token;
	register char *exporter = NULL, *class = NULL, *name = NULL;

	if (hcl_head != NULL) {
		hcl = hcl_head;
		while (hcl != NULL) {
			kmem_free(hcl->class_exporter,
			    strlen(hcl->class_exporter) + 1);
			hcl1 = hcl;
			hcl = hcl->class_next;
			kmem_free(hcl1, sizeof (struct hwc_class));
		}
		hcl_head = NULL;
	}

	if ((file = kobj_open_file(class_file)) == (struct _buf *)-1)
		return;

	state = class_begin;
	while ((token = lex(file, tokbuf)) != EOF) {
		switch (token) {
		case POUND:
			find_eol(file);
			break;
		case NAME:
		case STRING:
			name = kmem_alloc(strlen(tokbuf) + 1, KM_SLEEP);
			(void) strcpy(name, tokbuf);
			switch (state) {
			case class_begin:
				exporter = name;
				name = NULL;
				state = exporter_;
				break;
			case exporter_:
				class = name;
				name = NULL;
				add_class(exporter, class);
				kmem_free(exporter, strlen(exporter) + 1);
				exporter = NULL;
				kmem_free(class, strlen(class) + 1);
				class = NULL;
				break;
			} /* End Switch */
			break;
		case NEWLINE:
			kobj_newline(file);
			state = class_begin;
			if (name)
				kmem_free(name, strlen(name) + 1);
			if (exporter)
				kmem_free(exporter, strlen(exporter) + 1);
			if (class)
				kmem_free(class, strlen(class) + 1);
			break;
		default:
			/* Error */
			break;
		}
	}
	kobj_close_file(file);
}

/*
 * given a class spec, replicate the hwc specs, sort and return
 * a parent list. the actual replication is done by impl_replicate_hwc_spec()
 */
struct par_list *
impl_replicate_class_spec(struct par_list *class_pl)
{
	struct hwc_spec *hwcp = class_pl->par_specs;
	struct hwc_class *hcl;
	struct hwc_spec *hwc_head = NULL;
	struct hwc_spec *hwc_tail = NULL;

	if (hwcp == NULL) {
		return (NULL);
	}

	ASSERT(class_pl->par_specs->hwc_class_name);
	ASSERT(class_pl->par_specs->hwc_parent_name == NULL);

	for (hcl = hcl_head; hcl != NULL; hcl = hcl->class_next) {
		if (strcmp(hwcp->hwc_class_name, hcl->class) != 0) {
			continue;
		}
		impl_replicate_hwc_spec(class_pl, hcl->class_exporter,
						&hwc_head, &hwc_tail);
	}
	if (hwc_head) {
		return (sort_hwc(hwc_head));
	} else {
		return (NULL);
	}
}

/*
 * replicate a hwc spec
 */
static void
impl_replicate_hwc_spec(struct par_list *class_pl, char *parent,
    struct hwc_spec **hwc_head, struct hwc_spec **hwc_tail)
{
	struct hwc_spec *hwcp;
	struct hwc_spec *hs1;

	for (hwcp = class_pl->par_specs; hwcp != NULL;
	    hwcp = hwcp->hwc_next) {
		/*
		 * create new spec
		 */
		hs1 = kmem_zalloc(sizeof (struct hwc_spec),
		    KM_SLEEP);

		hs1->hwc_proto = kmem_zalloc(
		    sizeof (proto_dev_info_t), KM_SLEEP);

		hs1->hwc_proto->proto_devi_name = kmem_alloc(
		    strlen(hwcp->hwc_proto->proto_devi_name)
		    + 1, KM_SLEEP);
		(void) strcpy(hs1->hwc_proto->proto_devi_name,
		    hwcp->hwc_proto->proto_devi_name);

		hs1->hwc_parent_name =
		    kmem_alloc(strlen(parent) + 1, KM_SLEEP);
		(void) strcpy(hs1->hwc_parent_name, parent);

		copy_prop(hwcp->hwc_proto->proto_devi_sys_prop_ptr,
		    &(hs1->hwc_proto->proto_devi_sys_prop_ptr));

		/* if list is empty, add to tail, otherwise start new list */
		if (*hwc_head) {
			(*hwc_tail)->hwc_next = hs1;
			*hwc_tail = hs1;
		} else {
			*hwc_head = *hwc_tail = hs1;
		}
	}
}

/*
 * delete a parent list and all its hwc specs
 */
void
impl_delete_par_list(struct par_list *pl)
{
	struct par_list *saved_pl;
	struct hwc_spec *hp, *hp1;

	while (pl) {
		hp = pl->par_specs;
		while (hp) {
			hp1 = hp;
			hp = hp->hwc_next;
			hwc_free(hp1);
		}
		saved_pl = pl;
		pl = pl->par_next;
		kmem_free(saved_pl, sizeof (*saved_pl));
	}
} 

#if defined(__i386) || defined(__ia64)
void
open_mach_list(void)
{
	struct _buf *file;
	char tokbuf[MAXNAMELEN];
	register token_t token;
	struct psm_mach *machp;

	if ((file = kobj_open_file(mach_file)) == (struct _buf *)-1)
		return;

	while ((token = lex(file, tokbuf)) != EOF) {
		switch (token) {
		case POUND:
			find_eol(file);
			break;
		case NAME:
		case STRING:
			machp = kmem_alloc((sizeof (struct psm_mach) +
				strlen(tokbuf) + 1), KM_SLEEP);
			machp->m_next = pmach_head;
			machp->m_machname = (char *)(machp + 1);
			(void) strcpy(machp->m_machname, tokbuf);
			pmach_head = machp;
			break;
		case NEWLINE:
			kobj_newline(file);
			break;
		default:
			/* Error */
			break;
		}
	}
	kobj_close_file(file);
}

void *
get_next_mach(void *handle, char *buf)
{
	struct psm_mach *machp;

	machp = (struct psm_mach *)handle;
	if (machp)
		machp = machp->m_next;
	else
		machp = pmach_head;
	if (machp)
		(void) strcpy(buf, machp->m_machname);
	return (machp);
}

void
close_mach_list(void)
{
	struct psm_mach *machp;

	while (pmach_head) {
		machp = pmach_head;
		pmach_head = machp->m_next;
		kmem_free((caddr_t)machp, (sizeof (struct psm_mach) +
			strlen(machp->m_machname) + 1));
	}
}

/*
 * Read in the 'zone_lag' value from the rtc configuration file,
 * and return the value to the caller.
 */
long
process_rtc_config_file(void)
{
	struct _buf *file;
	char tokbuf[MAXNAMELEN];
	register token_t token;
	long zone_lag = 0;
	int state = 0;
	u_longlong_t tmp;

	if ((file = kobj_open_file(rtc_config_file)) == (struct _buf *)-1)
		return (0);

	while ((token = lex(file, tokbuf)) != EOF) {
		switch (token) {
		case POUND:
			find_eol(file);
			break;
		case NAME:
		case STRING:
			if (strcmp(tokbuf, "zone_lag") == 0)
				state = 1;	/* look for '=' */
			else {
				find_eol(file);
				state = 0;
			}
			break;
		case EQUALS:
			if (state == 1)
				state = 2;	/* look for zone_lag */
			else {
				find_eol(file);
				state = 0;
			}
			break;
		case DECVAL:
			if (state == 2) {
				(void) getvalue(tokbuf, &tmp);
				zone_lag = (long)tmp;
			} else {
				find_eol(file);
				state = 0;
			}
			break;
		case NEWLINE:
			kobj_newline(file);
			break;
		default:
			/* Error */
			break;
		}
	}
	kobj_close_file(file);
	return (zone_lag);
}
#endif /* __i386 || __ia64 */
