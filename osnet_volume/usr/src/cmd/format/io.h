
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef	_IO_H
#define	_IO_H

#pragma ident	"@(#)io.h	1.6	96/05/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Bounds structure for integer and disk input.
 */
struct bounds {
	daddr_t	lower;
	daddr_t	upper;
};

/*
 * List of strings with arbitrary matching values
 */
typedef struct slist {
	char	*str;
	char	*help;
	int	value;
} slist_t;


/*
 * Input parameter can be any one of these types.
 */
typedef union input_param {
	struct slist	*io_slist;
	char		**io_charlist;
	struct bounds	io_bounds;
} u_ioparam_t;

/*
 * These declarations define the legal input types.
 */
#define	FIO_BN		0		/* block number */
#define	FIO_INT		1		/* integer input */
#define	FIO_CSTR	2		/* closed string - exact match */
#define	FIO_MSTR	3		/* matched string, with abbreviations */
#define	FIO_OSTR	4		/* open string - anything's legal */
#define	FIO_BLNK	5		/* blank line */
#define	FIO_SLIST	6		/* one string out of a list, abbr. */
#define	FIO_CYL		7		/* nblocks, on cylinder boundary */
#define	FIO_OPINT	8		/* optional integer input */

/*
 * Miscellaneous definitions.
 */
#define	TOKEN_SIZE	36			/* max length of a token */
typedef	char TOKEN[TOKEN_SIZE+1];		/* token type */
#define	DATA_INPUT	0			/* 2 modes of input */
#define	CMD_INPUT	1
#define	WILD_STRING	"$"			/* wildcard character */
#define	COMMENT_CHAR	'#'			/* comment character */


/*
 *	Prototypes for ANSI C
 */
void	pushchar(int c);
int	checkeof(void);
char	*gettoken(char *inbuf);
void	clean_token(char *cleantoken, char *token);
void	flushline(void);
int	strcnt(char *s1, char *s2);
int	geti(char *str, int *iptr, int *wild);
int	getbn(char *str, daddr_t *iptr);
int	input(int, char *, int, u_ioparam_t *, int *, int);
void	print_input_choices(int type, u_ioparam_t *param);
int	find_value(slist_t *slist, char *match_str, int *match_value);
char	*find_string(slist_t *slist, int match_value);
int	slist_widest_str(slist_t *slist);
void	ljust_print(char *str, int width);
void	fmt_print(char *format, ...);
void	nolog_print(char *format, ...);
void	log_print(char *format, ...);
void	err_print(char *format, ...);
void	print_buf(char *buf, int nbytes);
void	pr_diskline(struct disk_info *disk, int num);
void	pr_dblock(void (*func)(char *, ...), daddr_t bn);
int	sup_inputchar(void);
void	sup_pushchar(int c);
int	sup_gettoken(char *buf);
void	sup_pushtoken(char *token_buf, int token_type);
void	get_inputline(char *, int);
int	istokenpresent(void);
int	execute_shell(char *);

/*
 * Most recent token type
 */
extern	int	last_token_type;

#ifdef	__cplusplus
}
#endif

#endif	/* _IO_H */
