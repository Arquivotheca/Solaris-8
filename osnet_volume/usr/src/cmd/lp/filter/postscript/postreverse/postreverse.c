/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 *	All Rights Reserved
 */

#ident	"@(#)postreverse.c	1.10	99/04/22 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include "postreverse.h"

extern char *_sys_errlist[];

/**
 * This version of postreverse should parse any Adobe DSC conforming PostScript
 * file and most that are not conforming, but minimally have the page (%%Page:)
 * and trailer (%%Trailer) comments in them at the begining of the line.
 *
 * If a document cannot be parsed (no page and trailer comments), it is 
 * passed through untouched.  If you look through the code you will find that
 * it doesn't ever look for the PostScript magic (%!).  This is because it
 * assumes that PostScript is sent in.  If PostScript is in sent in, it will
 * still attempt to parse it based on DSC page and trailer comments as if it
 * were postscript.
 *
 * flow goes as follows:
 *   1)  get command line options (including parsing a page list if supplied)
 *   2)  if no filename is supplied in command line, copy stdin to temp file.
 *   3)  parse the document:
 *            start from begining looking for a DSC page comment (that is the
 *                     header)
 *            start from the end looking for a DSC trailer comment (that is the
 *                     trailer)
 *            start from the header until the trailer looking for DSC page
 *                     comments.  each one signifies a new page.
 *	      start from the header until the trailer looking for BSD global
 *		       comments.  Each one violates page independence and will
 *		       be stored so it can be printed after the header and
 *		       before any pages.
 *   4)  print the document:
 *            if there is no header, trailer, or pages, print it from start
 *                   to end unaltered
 *            if they all exist, print the header, pages, and trailer
 *                   the pages are compared against a page list before
 *                   being printed, and are reverse if the reverse flag
 *                   has been set.
 *	      if global definitions were found in the pages of a document, they
 *		     are printed after the header and before the pages.
 **/



/**
 * caddr_t strrstr(caddr_t as1, caddr_t as2)
 *      return the address of the beginning of the last occruence of as2
 *      in as1 or NULL if not found
 **/
caddr_t strrstr(caddr_t s1, caddr_t s2)
{
  char *t1,*t2;
  char c;
  
  
  t1 = s1 + strlen(s1) - 1;
  t2 = s2 + strlen(s2) - 1;
  
  if (t2 == NULL || *t2 == '\0')
    return((char *)t1);
	c = *t2;
  
  while (s1 <= t1)
    if (*t1-- == c) {
      while ((c = *--t2) == *t1-- && t2 > s2) ;
      if (t2 <= s2)
	return((char *)t1 + 1);
      t2 = s2 + strlen(s2) - 1;
      c = *t2;
    }
  return(NULL);
}

/**
 * Copy stdin to a temp file and return the name
 **/
char *StdinToFile()
{
  char *fileName = tmpnam(NULL);
  int fd;
  int count;
  char buf[BUFSIZ];

  if ((fd = open(fileName, O_RDWR|O_CREAT|O_EXCL, 0600)) < 0) {
    fprintf(stderr, "open(%s): %s\n", fileName, _sys_errlist[errno]);
    return(NULL);
  }

  while ((count = read(0, buf, sizeof(buf))) > 0)
    if (write(fd, buf, count) != count) {
      fprintf(stderr, "write(%d, 0x%x, %d): %s\n", fd, buf, count,
	      _sys_errlist[errno]);
      close(fd);
      unlink(fileName);
      return(NULL);
    }
  return(fileName);
}

/**
 * Usage(char *name) - program usage
 **/
void Usage(char *name)
{
  fprintf(stderr, "Usage: %s [ -o list ] [ -r ] [ filename ]\n", name);
  exit(1);
}


/**
 * int **ParsePageList(char *list)
 *    This will parse as string #,#,#-#,#... into an array of pointers
 *  to integers.  This array will contain all numbers in the list including
 *  those int the range #-#.  The list returned is NULL terminated.
 *  It uses 2 passes to build the list.  pass 1 counts the # of ints and
 *  allocates the space, and pass 2 fills in the list.
 **/
int **ParsePageList(char *list)
{
  int **pageList = NULL;
  int  pass = 0;

  if (list == NULL)
    return(NULL);

  while (pass++ < 2) {
    char *page;
    char *tmplist;
    int size = 0;

    tmplist = strdup(list);
    page = strtok(tmplist, ",");

    do {
      int start, end;
      char *s1 = page,
           *s2;
      
      if (s2 = strchr(page, '-')) {
	*s2++ = NULL;
	start = atoi(s1);
	end = atoi(s2);
	if (end < start) {
	  int tmp = end;
	  
	  end = start;
	  start = tmp;
	}
      } else
	start = end =  atoi(s1);
      
      while (start <= end)
	if (pass == 1) /* count the pages for allocation */
	  size++, start++;
        else {         /* fill in the page list */
	  int  *tmp = (int *)malloc(sizeof(int));
	  *tmp = start++;
	  pageList[size++] = tmp;
	}  
    } while (page = strtok(NULL, ","));
    free(tmplist);
    if (pass == 1)
      pageList = (int **)calloc(sizeof(int *), (size+1));
  }
  return(pageList);
}


/**
 * int PageIsListed(int page, int **pageList)
 *    returns 1 if the pagelist is empty or if the page is in the
 *  NULL terminated pageList.  returns 0 if the page is not listed
 **/
int PageIsListed(int page, int **pageList)
{
  int count = 0;

  if (!pageList)
    return(1);

  for (count = 0; pageList[count] != NULL ; count++)
    if (*pageList[count] == page)
      return(1);
  return(0);
}


/**
 * Writes the document Header to the fd
 **/
int WriteDocumentHeader(int fd, DOCUMENT *d)
{
  if (d) {
    HEADER *h = d->header;

    if (h) 
      return(write(fd, h->start, h->size));
  }
  errno = EINVAL;
  return(-1);
}

/**
 * Writes the document global block to the fd
 **/
int WriteGlobal(int fd, GLOBAL *g)
{
  if (g)
    return(write(fd, g->start, g->size));
  errno = EINVAL;
  return(-1);
}

/**
 * Writes the document Trailer to the fd
 **/
int WriteDocumentTrailer(int fd, DOCUMENT *d)
{
  if (d) {
    TRAILER *t = d->trailer;

    if (t) 
      return(write(fd, t->start, t->size));
  }
  errno = EINVAL;
  return(-1);
}

/**
 * Writes the document page to the fd
 **/
int WritePage(int fd, PAGE *p, int global)
{
  if (p) {
    caddr_t ptr1;

    if (((ptr1 = strstr(p->start, PS_BEGIN_GLOBAL)) != NULL) &&
	(ptr1 < p->start + p->size) &&
	(global != 0)) {
	    /* BeginGlobal/EndGlobal in the page... */
	    write(fd, p->start, ptr1 - p->start);
	    ptr1 = strstr(ptr1, PS_END_GLOBAL);
	    ptr1 += strlen(PS_END_GLOBAL);
	    return(write(fd, ptr1, (p->size - (ptr1 - p->start))));
    } else
      return(write(fd, p->start, p->size));
  }
  errno = EINVAL;
  return(-1);
}

/**
 * Writes out the document pages in pageList (or all if NULL) and reverse
 * the output if reverse == 1
 **/
void WriteDocument(DOCUMENT *document, int reverse, int **pageList)
{
  if (document->header && document->trailer && document->page) {
    int count = 0;
    WriteDocumentHeader(1, document);

    if (document->global != NULL) {
      int count = 0;

      while (document->global[count] != NULL) {
        GLOBAL *global = document->global[count++];

        if (global)
		WriteGlobal(1, global);
      }
    }

    if (reverse) {
      int count = document->pages;

      while (count >=0 ) {
	PAGE *page = document->page[count--];

	if (page && PageIsListed(page->number, pageList))
	  WritePage(1, page, document->global != NULL);
      }
    } else {
      int count = 0;

      while (count <= document->pages) {
	PAGE *page = document->page[count++];

	if (page && PageIsListed(page->number, pageList))
	  WritePage(1, page, document->global != NULL);
      }
    }      
    
    WriteDocumentTrailer(1, document);
  } else {
    write(1, document->start, document->size);
  }
}

/**
 * get a document header from document and return a pointer to a HEADER
 * structure.
 **/
HEADER *DocumentHeader(DOCUMENT *document)
{
  HEADER *header;
  caddr_t start;

  header = (HEADER *)malloc(sizeof(*header));
  memset(header, 0, sizeof(*header));
  if (start = strstr(document->start, PS_PAGE)) {
    header->label = "Document Header";
    header->start = document->start;
    header->size = (start - document->start + 1);
  } else {
    free(header);
    header = NULL;
  }
  return(header);
}
   

/**
 * get a document trailer from document and return a pointer to a trailer
 * structure.
 **/
TRAILER *DocumentTrailer(DOCUMENT *document)
{
  TRAILER *trailer;

  trailer = (TRAILER *)malloc(sizeof(*trailer));
  memset(trailer, 0, sizeof(trailer));
  if (trailer->start = strrstr(document->start, PS_TRAILER)) {
    trailer->label = "Document Trailer";
    trailer->start += 1;
    trailer->size = strlen(trailer->start);
  } else {
    free(trailer);
    trailer = NULL;
  }
  return(trailer);
}

GLOBAL **DocumentGlobals(DOCUMENT *document)
{
  GLOBAL **globals;
  caddr_t start, ptr1, ptr2;
  long size = 0;
  int count = 0;

  start = strstr(document->start, PS_PAGE);
  for (ptr1 = strstr(start, PS_BEGIN_GLOBAL) ; ptr1 != NULL ;
       ptr1 = strstr(++ptr1, PS_BEGIN_GLOBAL))
    size++;

  if (!size)
    return(NULL);

  globals = (GLOBAL **)calloc(sizeof(GLOBAL *), size+1);
  
  for (ptr1 = strstr(start, PS_BEGIN_GLOBAL) ; ptr1 != NULL ;
       ptr1 = strstr(++ptr1, PS_BEGIN_GLOBAL)) {
    GLOBAL *global = (GLOBAL *)malloc(sizeof(*global));
    caddr_t global_end;

    if ((global_end = strstr(++ptr1, PS_END_GLOBAL)) == NULL) {
       fprintf(stderr,
	       "DSC violation: %%%%BeginGlobal with no %%%%EndGlobal\n");
       exit(-1);
    }
    memset(global, 0, sizeof(*global));
    global->start = ptr1;
    global->size = strchr(++global_end, '\n') - ptr1 + 1;
    globals[count++] = global;
    ptr1 = global->start + global->size;
  }
  return(globals);
}


/**
 * get the pages from a document and return a pointer a list of PAGE
 * structures.
 **/
PAGE **DocumentPages(DOCUMENT *document)
{
  PAGE **pages;
  caddr_t ptr1, ptr2;
  long size = 0;

  for (ptr1 = strstr(document->start, PS_PAGE) ; ptr1 != NULL ;
       ptr1 = strstr(++ptr1, PS_PAGE))
    size++;

  if (!size)
    return(NULL);

  pages = (PAGE **)calloc(sizeof(PAGE *), size+1);

  for (ptr1 = strstr(document->start, PS_PAGE) ; ptr1 != NULL ;
       ptr1 = strstr(++ptr1, PS_PAGE)) {
    PAGE *page = (PAGE *)malloc(sizeof(*page));
    caddr_t page_end;
    char *label = NULL,
         *tmp;
    long  number = -1;

    /* page start & end */
    if ((page_end = strstr(++ptr1, PS_PAGE)) == NULL)
      if (document->trailer) 
	page_end = document->trailer->start - 1;
      else
	page_end = document->start + document->size;

    /* page label & number */
    if (tmp = strchr(ptr1, ' ')) {
      char *tmp_end;

      if (tmp_end = strchr(++tmp, ' ')) {
	label = (char *)malloc((tmp_end - tmp) + 1);
	memset(label, 0, (tmp_end - tmp) + 1);
	strncpy(label, tmp, (tmp_end - tmp));
	number = atol(++tmp_end);
      }
    }

    memset(page, 0, sizeof(*page));
    page->label = label;
    page->number = number;
    page->start = ptr1;
    page->size = page_end - ptr1 + 1;

    pages[document->pages++] = page;
  }
  return(pages);
}

/**
 * parse a document and return a pointer to a DOCUMENT structure
 **/
DOCUMENT *DocumentParse(char *name)
{
  DOCUMENT *document = NULL;
  int fd;
  struct stat st;

  if (stat(name, &st) < 0) {
    fprintf(stderr, "stat(%s): %s\n", name, _sys_errlist[errno]);
    return(NULL);
  }

  if (st.st_size == 0) {
    fprintf(stderr, "%s: empty file\n", name);
    return(NULL);
  }

  if ((fd=open(name, O_RDONLY)) < 0) {
    fprintf(stderr, "open(%s, O_RDONLY): %s\n", name, _sys_errlist[errno]);
    return(NULL);
  }

  document = (DOCUMENT *)malloc(sizeof(*document));
  memset(document, 0, sizeof(*document));
  if ((document->start = mmap((caddr_t)0, (size_t)st.st_size, PROT_READ,
			      (MAP_PRIVATE | MAP_NORESERVE),
			      fd, (off_t)0)) == MAP_FAILED) {
    fprintf(stderr,
	    "mmap(0, %, PROT_READ, (MAP_PRIVATE|MAP_NORESERVE), %d, 0): %s\n",
	    st.st_size, fd);
    free(document);
    document = NULL;
  } else {
    /* order in important */
    document->name = strdup(name);
    document->size = strlen(document->start);
    document->header = DocumentHeader(document);
    document->trailer = DocumentTrailer(document);
    document->page = DocumentPages(document);
    document->global = DocumentGlobals(document);
  }
  close(fd);
  return(document);
}


#if defined(DEBUG)
/**
 * Print out the contents of the document structure
 **/
void PrintDocumentInfo(DOCUMENT *d)
{
  if (d) {
    printf("Document:\n\tname:  %s\n\tstart: 0x%x\n\tsize:  %ld\n",
	   d->name, d->start, d->size);
    if (d->header) {
      HEADER *h = d->header;

      printf("\tHeader: %s (0x%x, %ld)\n", h->label, h->start, h->size);
    }

    if (d->global) {
      int count = 0;

      while (d->global[count++] != NULL) ;
      printf("\tDSC violating BeginGlobals: %d\n", count);
    }

    if (d->page) {
      PAGE *p;
      int count = 0;
      
      printf("\tPages: (%d)\n", d->pages);
      for (p = d->page[0]; p != NULL ; p = d->page[++count])
	printf("\t\t %4d (%s) - (0x%x, %ld)\n", p->number,
	       (p->label ? p->label : "Page"), p->start, p->size);
    }
    if (d->trailer) {
      TRAILER *t = d->trailer;

      printf("\tTrailer: %s (0x%x, %ld)\n", t->label, t->start, t->size);
    }
  }
}
#endif /* DEBUG */


main( int ac, char *av[])
{
  DOCUMENT *document;
  char *fileName = NULL;
  char *programName = NULL;
  char *unlinkFile = NULL;
  int  reversePages = 1;
  int  **pageList = NULL;
  int option;

  if (programName = strrchr(av[0], '/'))
    programName++;
  else
    programName = av[0];

  while ((option = getopt(ac, av, "o:r")) != EOF)
    switch(option) {
    case 'o':                  
      pageList = ParsePageList(optarg);
      break;
    case 'r':                  
      reversePages = 0;
      break;
    case '?':                  
      Usage(programName);
      break;
    default:             
      fprintf(stderr, "missing case for option %c\n", option);
      Usage(programName);
      break;
    }
  
  ac -= optind;
  av += optind;

  switch (ac) {
    case 0:
    unlinkFile = fileName = StdinToFile();
    break;
  case 1:
    fileName = av[0];
    break;
  default:
    Usage(programName);
  }

  if ((document = DocumentParse(fileName)) == NULL) {
    fprintf(stderr, "Unable to parse document (%s)\n", fileName);
    exit(0);
  }

#if defined(DEBUG) && defined(NOTDEF)
  PrintDocumentInfo(document);
#endif /* DEBUG */

  WriteDocument(document, reversePages, pageList); 
     
  if (unlinkFile)
    unlink(unlinkFile);

  exit(0);
}
