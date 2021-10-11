! @(#)goforth.S 1.4 91/04/17 SMI
!

#include <sys/asm_linkage.h>

#if	defined(lint)
void
goforth(struct sunromvec *romp, caddr_t start, caddr_t end)
{ return; }
#endif

	.text
!
! goforth(struct sunromvec *romp,
!	char *start, char *end)
!
	ENTRY(goforth)
	save	%sp, -SA(MINFRAME), %sp
	ld	[%i0 + 0x7c], %l2	! Address of romp->v_interpret
	set	byteload, %i1
	sethi	%hi(forthblock), %i2
	or	%i2, %lo(forthblock), %i2
v2:
	!
	! op_interpret(cmd, 1, forthblock);
	!
	mov	%i1, %o0
	mov	%i2, %o2

	call	%l2
	mov	1, %o1
/*NOTREACHED*/

byteload:
	.asciz	"byte-load"
	.align	4
