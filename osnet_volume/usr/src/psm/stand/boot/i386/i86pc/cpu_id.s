#include <sys/reg.h>

#ifdef	lint
int is486(void) { return(1); }
#else

/       Copyrighted as an unpublished work.
/       (c) Copyright 1991 Sun Microsystems, Inc.
/       All rights reserved.

/       RESTRICTED RIGHTS

/       These programs are supplied under a license.  They may be used,
/       disclosed, and/or copied only as permitted under such license
/       agreement.  Any copy must contain the above copyright notice and
/       this restricted rights notice.  Use, copying, and/or disclosure
/       of the programs is strictly prohibited unless otherwise provided
/       in the license agreement.


	.file   "cpu_id.s"

	.ident "@(#)cpu_id.s	1.8	98/02/11 SMI"
	.ident  "@(#) (c) Copyright Sun Microsystems, Inc. 1991"

	.text

/       We only support 486 or better. So just return 1.

	.globl  is486
is486:
	movl	$1, %eax
	ret

#endif	/* !lint */

#ifdef	lint
int has_cpuid(void) { return (1); }
#else
.set EFL_ID, 0x200000                   / ID bit in EFLAGS
	.text
	.globl	has_cpuid
has_cpuid:
	pushl	%esp
        pushfl                   
	popl	%eax
	movl	%eax, %ecx
        xorl    $EFL_ID, %eax
        pushl   %eax
        popfl
        pushfl
        popl    %eax		/ The above all lifted from locore.s
        cmpl    %eax, %ecx
        jne      has_it
	movl	$0, %eax
	popl	%esp
	ret
has_it:
	movl $1, %eax
	popl	%esp
	ret
#endif

#ifdef	lint
int largepage_supported(void) { return(1); }
#else
	.text
	.globl	largepage_supported
largepage_supported:
	pushl	%esp		/ save our stack value
	movl	$1, %eax	/ capability test. Mov 1 to eax for cpuid
	cpuid
	andl	$0x8, %edx 	/ do you have large pages?
	jne	yes
	movl	$0, %eax	/ no we don't have large pages
	popl	%esp
	ret
yes:	
	movl	$1, %eax	/ yes we do have large pages
	popl	%esp
	ret
#endif

#ifdef	lint
int enable_large_pages(void) { return(1); }
#else
	.text
	.globl	enable_large_pages
enable_large_pages:
	movl	%cr4, %eax
	orl	$CR4_PSE, %eax / since we have large pages enable them
	movl	%eax, %cr4
#endif

#ifdef	lint
int global_bit(void) { return(1); }
#else
.set EFL_ID, 0x200000                   / ID bit in EFLAGS

	.text
	.globl	global_bit
global_bit:
	pushl	%esp		/ save our stack value
	movl	$1, %eax	/ capability test. Mov 1 to eax for cpuid
	cpuid
	andl	$0x2000, %edx 	/ do you have global pdtes?
	jne	hasgbit
	movl	$0, %eax	/ no we don't have global pdtes
	popl	%esp
	ret
hasgbit:
	movl	$1, %eax	/ yes we do have global pdtes
	popl	%esp
	ret
#endif

#ifdef	lint
int enable_global_pages(void) { return(1); }
#else
	.text
	.globl	enable_global_pages
enable_global_pages:
	movl	%cr4, %eax
	orl	$CR4_PGE, %eax / since we have global pages enable them
	movl	%eax, %cr4
#endif

#ifdef	lint
int GenuineIntel(void) { return(1); }
#else
	.data
intel_cpu_id:	.string	"GenuineIntel"
amd_cpu_id:	.string	"AuthenticAMD"

	.text
	.globl	GenuineIntel
GenuineIntel:
	pushl	%esp		/ save our stack value
	pushl	%edi		/ save edi
	pushl	%esi		/ save esi
	movl	%esp, %esi
	subl 	$0xc, %esi
	movl	$0, %eax	/ capability test. Mov 0 to eax for cpuid
	cpuid

	movl	%ebx, (%esi)
	movl	%edx, 4(%esi)
	movl	%ecx, 8(%esi)

	leal    intel_cpu_id, %edi
        movl    $12, %ecx
        repz
        cmpsb
        jne     not_intel
	movl	$1, %eax
	jmp	out
not_intel:
	movl	$0, %eax
out:
	popl	%esi
	popl	%edi
	popl	%esp
	ret
#endif

#ifdef	lint
int AuthenticAMD(void) { return(1); }
#else
	.text
	.globl	AuthenticAMD
AuthenticAMD:
	pushl	%esp		/ save our stack value
	pushl	%edi		/ save edi
	pushl	%esi		/ save esi
	movl    %esp, %esi
	subl    $0xc, %esi
	movl	$0, %eax	/ capability test. Mov 0 to eax for cpuid
	cpuid

	movl	%ebx, (%esi)
	movl	%edx, 4(%esi)
	movl	%ecx, 8(%esi)

	leal    amd_cpu_id, %edi
        movl    $12, %ecx
        repz
        cmpsb
        jne     not_amd
	movl	$1, %eax
	jmp	out2
not_amd:
	movl	$0, %eax
out2:
	popl	%esi
	popl	%edi
	popl	%esp
	ret
#endif

#ifdef	lint
int pae_supported(void) { return (1); }
#else
.set PAE_AND_CXS, 0x140		/ PAE = 0x40 & CXS = 0x100

	.text
	.globl	pae_supported
pae_supported:
	pushl	%esp		/ save our stack value
	movl	$1, %eax	/ capability test. Mov 1 to eax for cpuid
	cpuid
	andl	$PAE_AND_CXS, %edx 	/ do you support pae and cmpxchg8b?
	cmpl	$PAE_AND_CXS, %edx
	jne	nopae
	movl	$1, %eax	/ yes we do support pae and cmpxchg8b
	popl	%esp
	ret
nopae:
	movl	$0, %eax	/ no we don't support pae and cmpxchg8b
	popl	%esp
	ret
#endif
