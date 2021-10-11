/*
 * adb - source file access routines
 */

#ident "@(#)fio.c	1.17	97/09/04 SMI"

#include <stdio.h>
#include <strings.h>
#include "adb.h"
#include "fio.h"

char *
savestr(cp)
	char *cp;
{
	char *dp = (char *)malloc(strlen(cp) + 1);

	if (dp == NULL)
		outofmem();
	(void) strcpy(dp, cp);
	return (dp);
}
fenter(name)
	char *name;
{
	register struct sfile *fi;

	db_printf(7, "fenter: name='%s'", (name == NULL) ? "NULL" : name);
	if (fi = fget(name)) {
		db_printf(7, "fenter: returns %D",  fi - file + 1);
		return (fi - file + 1);
	}
	if (NFILE == 0) {
		NFILE = 10;
		file = (struct sfile *)malloc(NFILE * sizeof (struct sfile));
		if (file == NULL)
			outofmem();
		filenfile = file+nfile;
	}
	if (nfile == NFILE) {
		NFILE *= 2;
		file = (struct sfile *)
		    realloc((char *)file, NFILE*sizeof (struct sfile));
		if (file == NULL)
			outofmem();
		filenfile = file+nfile;
	}
	fi = &file[nfile++];
	filenfile++;
	fi->f_name = savestr(name);
	fi->f_flags = 0;
	fi->f_nlines = 0;
	fi->f_lines = 0;
	db_printf(7, "fenter: returns %D",  nfile);
	return (nfile);
}

struct sfile *
fget(name)
	char *name;
{
	register struct sfile *fi;

	db_printf(7, "fget: name='%s'", (name == NULL) ? "NULL" : name);
	for (fi = file; fi < filenfile; fi++)
		if (!strcmp(fi->f_name, name)) {
			db_printf(7, "fget: returns %D",  fi);
			return (fi);
		}
	
	db_printf(7, "fget: returns 0");
	return (0);
}

#ifdef notdef
static
findex(name)
	char *name;
{
	register struct sfile *fi;
	int index = 0;

	db_printf(8, "findex: name='%s'", (name == NULL) ? "NULL" : name);
	for (fi = file, index = 1; fi < filenfile; fi++, index++)
		if (!strcmp(fi->f_name, name))
			return (index);
	db_printf(8, "findex: returns 0");
	return (0);
}
#endif

struct sfile *
indexf(index)
	unsigned int index;
{
	db_printf(8, "indexf: index=%u", index);
	index--;
	if (index < nfile) {
		db_printf(8, "indexf: returns %X", &file[index]);
		return (&file[index]);
	}
	db_printf(8, "indexf: returns NULL");
	return (NULL);
}

void
sfinit(fi)
	struct sfile *fi;
{
#ifndef KADB
	char buf[BUFSIZ];
	register off_t *p;

	if (openfile == fi && OPENFILE)
		return;
	/* IT WOULD BE BETTER TO HAVE A COUPLE OF FILE DESCRIPTORS, LRU */
	if (OPENFILE) {
		(void) fclose(OPENFILE);
		OPENFILE = NULL;
	}
	if ((OPENFILE = fopen(fi->f_name, "r")) == NULL)
		error("can't open file");
	openfile = fi;
	setbuf(OPENFILE, filebuf);
	openfoff = -BUFSIZ;		/* putatively illegal */
	if (fi->f_flags & F_INDEXED)
		return;
	fi->f_flags |= F_INDEXED;
	p = fi->f_lines = (off_t *)malloc(101 * sizeof (off_t));
	if (p == NULL)
		outofmem();
	*p++ = 0;		/* line 1 starts at 0 ... */
	fi->f_nlines = 0;
	/* IT WOULD PROBABLY BE FASTER TO JUST USE GETC AND LOOK FOR \n */
	while (fgets(buf, sizeof buf, OPENFILE)) {
		if ((++fi->f_nlines % 100) == 0)
			openfile->f_lines =
			    (off_t *)realloc((char *)openfile->f_lines,
			     (openfile->f_nlines+101) * sizeof (off_t));
			if (openfile->f_lines == NULL)
				outofmem();
		p[0] = p[-1] + strlen(buf);
		p++;
	}
#else
	error("can't open file");
#endif KADB
}

void
printfiles()
{
	/* implement $f command */
	register struct sfile *f;
	register int i;

	for (f = file, i = 1; f < filenfile; f++, i++) {
		printf("%d	%s\n", i, f->f_name);
	}
}

void
getline(file, line)
	int file, line;
{
	register struct sfile *fi;
	register off_t *op;
	int o, n;

	db_printf(8, "getline: file=%D, line=%D", file, line);
	fi = indexf(file);
	sfinit(fi);
#ifndef KADB
	if (line == 0) {
		(void) sprintf(linebuf, "%s has %ld lines\n",
		    fi->f_name, fi->f_nlines);
		return;
	}
	if (line < 0 || line > fi->f_nlines)
		errflg = "line number out of range";
	op = &fi->f_lines[line-1];
	n = op[1] - op[0];
	linebuf[n] = 0;
	if (*op >= openfoff && *op < openfoff + BUFSIZ) {
case1:
		o = op[0] - openfoff;
		if (o + n > BUFSIZ) {
			strncpy(linebuf, filebuf+o, BUFSIZ-o);
			openfoff += BUFSIZ;
			(void) read(fileno(OPENFILE), filebuf, BUFSIZ);
			strncpy(linebuf+BUFSIZ-o, filebuf, n-(BUFSIZ-o));
		} else
			strncpy(linebuf, filebuf+o, n);
		return;
	}
	if (op[1]-1 >= openfoff && op[1] <= openfoff+BUFSIZ) {
		o = op[1] - openfoff;
		strncpy(linebuf+n-o, filebuf, o);
		(void) lseek(fileno(OPENFILE), (long)openfoff, 0);
		(void) read(fileno(OPENFILE), filebuf, BUFSIZ);
		strncpy(linebuf, filebuf+op[0]-openfoff, n-o);
		return;
	}
	openfoff = (op[0] / BUFSIZ) * BUFSIZ;
	(void) lseek(fileno(OPENFILE), (long)openfoff, 0);
	(void) read(fileno(OPENFILE), filebuf, BUFSIZ);
	goto case1;
#endif !KADB
}

