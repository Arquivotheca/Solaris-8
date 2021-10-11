/*
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)elf_notes.h	1.2	97/09/13 SMI"

#define	TRUE	1
#define	FALSE	0

/*
 * Start of an ELF Note.
 */
typedef struct {
	Nhdr		nhdr;
	char		name[8];
} Note;

extern void elfnote(int dfd, int type, char *ptr, int size);

int setup_note_header(Phdr *, int nlwp, char *pdir, pid_t pid);
int write_elfnotes(int nlwp, int dfd);
void cancel_notes(void);

extern int setup_old_note_header(Phdr *, int nlwp, char *pdir, pid_t pid);
extern int write_old_elfnotes(int nlwp, int dfd);
extern void cancel_old_notes(void);
