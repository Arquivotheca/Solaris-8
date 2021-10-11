/*
 * Copyright (c) 1990 Sun Microsystems, Inc.
 */

#ifndef _ELF_BOOT_H
#define	_ELF_BOOT_H

#ident	"@(#)elf_boot.h	1.2	92/07/14 SMI"

/*
 * Attribute/value structures used to bootstrap ELF-based dynamic linker.
 */

#ifndef	_ASM
typedef struct {
	Elf32_Sword eb_tag;		/* what this one is */
	union {				/* possible values */
		Elf32_Word eb_val;
		Elf32_Addr eb_ptr;
		Elf32_Off  eb_off;
	} eb_un;
} Elf32_Boot;
#endif	/* _ASM */

/*
 * Attributes
 */
#define	EB_NULL		0		/* (void) last entry */
#define	EB_DYNAMIC	1		/* (*) dynamic structure of subject */
#define	EB_LDSO_BASE	2		/* (caddr_t) base address of ld.so */
#define	EB_ARGV		3		/* (caddr_t) argument vector */
#define	EB_ENVP		4		/* (char **) environment strings */
#define	EB_AUXV		5		/* (auxv_t *) auxiliary vector */
#define	EB_DEVZERO	6		/* (int) fd for /dev/zero */
#define	EB_PAGESIZE	7		/* (int) page size */
#define	EB_MAX		8		/* number of "EBs" */

#endif	/* _ELF_BOOT_H */
