/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _PARSER_H
#define	_PARSER_H

#pragma ident	"@(#)parser.h	1.1	99/01/25 SMI"

#include <sys/param.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct metainfo {
	char mi_filename[MAXPATHLEN];
	int mi_line_number;
	int mi_nlines;
	int mi_ext_cnt;
	int mi_flags;
	int mi_extended;
} Meta_info;

typedef struct translator_info {
	char	*ti_liblist;
	char	*ti_dash_I;
	char	*ti_output_file;
	int	ti_nfiles;
	int	ti_verbosity;
	int	ti_flags;
	char    *ti_versfile;
	char	*ti_arch;
	int	ti_archtoken;
	int	ti_libtype;	/* set to FILTERLIB if processing filter lib */
} Translator_info;

typedef struct {
	char *key;
	int  token;
} xlator_keyword_t;

/* Return Codes from xlator_* functions */
#define	XLATOR_FATAL	-2
#define	XLATOR_NONFATAL	-1
#define	XLATOR_SUCCESS	0
#define	XLATOR_SKIP	1

/* Misc Return Codes from Utility Functions */
enum {
	XLATOR_KW_NOTFOUND,
	XLATOR_KW_FUNC,
	XLATOR_KW_DATA,
	XLATOR_KW_END
};

/* Library Type */
#define	NORMALLIB 0
#define	FILTERLIB 1

/* Maxmimum levels of extends */
#define	MAX_EXTENDS 16

/* Architecture Bitmap */
#define	XLATOR_SPARC	0x01
#define	XLATOR_SPARCV9	0x02
#define	XLATOR_I386	0x04
#define	XLATOR_IA64	0x08
#define	XLATOR_ALLARCH	0xFF

extern xlator_keyword_t *keywordlist;
extern char **filelist;
extern int verbosity;

extern int frontend(const Translator_info *);
extern int do_extends(const Meta_info, const Translator_info *, char *);
extern void split(const char *, char *, char *);
extern void remcomment(char const *);
extern void getlinecont(char *, char *, int, FILE *, Meta_info *);
extern char *line_to_buf(char *, const char *);
extern int non_empty(const char *);
extern int check4extends(const char *, const char *, int, FILE *);
extern int interesting_keyword(xlator_keyword_t *, const char *);
extern int arch_strtoi(const char *);

#ifndef NOREADLINE
extern int readline(char **, FILE *);
#endif
extern int arch_match(FILE *, int);

/* xlator_ functions */
extern xlator_keyword_t *xlator_init(const Translator_info *);
extern int xlator_startlib(char const *libname);
extern int xlator_startfile(char const *filename);
extern int xlator_start_if(const Meta_info meta_info, const int token,
    char *value);
extern int xlator_take_kvpair(const Meta_info, const int token, char *value);
extern int xlator_end_if(const Meta_info, const char *value);
extern int xlator_endfile(void);
extern int xlator_endlib(void);
extern int xlator_end(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _PARSER_H */
