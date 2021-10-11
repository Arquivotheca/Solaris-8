#include <sys/types.h>
#if defined(sun4m) || defined(sun4d)
#include <vm/hat.h>
#include <vm/hat_srmmu.h>
#include <sys/mmu.h>
#endif

ptbl
#if defined(sun4m) || defined(sun4d)
.>P
{SIZEOF}>E
$<<hme.sizeof
(((<P-*ptbls)%<E)*<U*0x40)+*hments>X
<X=X
#endif
