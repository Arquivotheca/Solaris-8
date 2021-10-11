#if !(defined lint)
/       Copyrighted as an unpublished work.
/       (c) Copyright 1992-96, by Sun Microsystems, Inc.
/       All rights reserved.

/       RESTRICTED RIGHTS

/       These programs are supplied under a license.  They may be used,
/       disclosed, and/or copied only as permitted under such license
/       agreement.  Any copy must contain the above copyright notice and
/       this restricted rights notice.  Use, copying, and/or disclosure
/       of the programs is strictly prohibited unless otherwise provided
/       in the license agreement.


	.file   "dpt_send.s"

	.ident "@(#)dpt_send.s	1.5	96/08/29 SMI"

	.text

/       Send a dpt command to the dpt hba adaptor
#endif

#if defined(lint)

/* ARGSUSED */
void
_dpt_send_cmd()
{
}

#else	/* lint */
/	--------------------------------------------
/	void dpt_send_cmd(port, addr, command): 
/	offset	       	   4    8    12
	.globl	dpt_send_cmd
dpt_send_cmd:
	movl	4(%esp), %edx	/ port + 0 = 1F0 ( bootom of ports: 1f0 - 1f7
	addb	$2,%dl     	/ = 1f2

	movl	8(%esp), %eax	/ address of physical memory (LSB) 1f2
	outb	(%dx)

	incw	%dx		/ next port 1f3
	shrl    $8, %eax	/ next byte in addr
	outb	(%dx)

	incw	%dx		/ next port 1f4
	shrl    $8, %eax	/ next byte in addr
	outb	(%dx)

	incw	%dx		/ next port 1f5
	shrl    $8, %eax	/ MSB
	outb	(%dx)

	addb	$2, %dl		/ 1f7 - command byte
	movl	12(%esp), %eax
	outb	(%dx)
	ret

#endif /* lint */
