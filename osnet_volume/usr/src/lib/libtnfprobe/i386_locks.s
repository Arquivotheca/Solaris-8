#include <sys/asm_linkage.h>
	.file		"i386_locks.s"
	ENTRY(tnfw_b_get_lock)
	movl	4(%esp), %edx
	subl	%eax, %eax
	lock
	btsl	$0, (%edx)
	jnc	.L1
	incl	%eax
.L1:
	ret

	ENTRY(tnfw_b_clear_lock)
	movl	4(%esp), %eax
	movb	$0, (%eax)
	ret

	ENTRY(tnfw_b_atomic_swap)
	movl	4(%esp), %edx
	movl	8(%esp), %eax
	xchgl	%eax, (%edx)
	ret
