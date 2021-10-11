/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _SOURCE_H
#define	_SOURCE_H

#pragma ident	"@(#)source.h	1.5	94/08/24 SMI"

/*
 * Declarations
 */

#ifdef __cplusplus
extern "C" {
#endif

void			source_init(void);
void			source_file(char *path);
int			 source_input(void);
void			source_unput(int c);
void			source_output(int c);
void			source_nl(void);

void			yyerror(char *s);
void			prompt(void);
void
semantic_err(char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* _SOURCE_H */
