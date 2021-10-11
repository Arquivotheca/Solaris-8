#include <sys/types.h>
#if defined(sun4m) || defined(sun4d)
#include <sys/mmu.h>
#include <sys/pte.h>
#include <vm/hat_srmmu.h>
#endif

ptbl
#if defined(sun4m) || defined(sun4d)
.>P
{SIZEOF}>E
(((<P-*ptbls)%<E)*4*0x40)+*ptes>X
<X=X
#endif
