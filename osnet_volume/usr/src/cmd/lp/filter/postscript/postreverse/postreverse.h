/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 *	All Rights Reserved
 */

#ident	"@(#)postreverse.h	1.5	94/05/20 SMI"

/**
 * Structures and definitions needed for PostScript page manipulation
 **/
#if !defined(_POSTREVERSE_H)
#define _POSTREVERSE_H

/* PS DSC comments of interest */
#define PS_PAGE		"\n%%Page:"
#define PS_TRAILER	"\n%%Trailer"
#define PS_BEGIN_GLOBAL	"\n%%BeginGlobal"
#define PS_END_GLOBAL	"\n%%EndGlobal"

struct _global {
  caddr_t start;
  size_t size;
};
typedef struct _global GLOBAL;

struct _page {
  unsigned int number;
  char *label;
  caddr_t start;
  size_t size;
};
typedef struct _page PAGE;

struct _header {
  char *label;
  caddr_t start;
  size_t size;
};
typedef struct _header HEADER;

struct _trailer {
  char *label;
  caddr_t start;
  size_t size;
};
typedef struct _trailer TRAILER;

struct _document {
  char *name;
  caddr_t start;
  size_t size;
  HEADER *header;
  PAGE **page;
  GLOBAL **global;
  long pages;
  TRAILER *trailer;
};
typedef struct _document DOCUMENT;

#endif /* _POSTREVERSE_H */
