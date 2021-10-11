#include <sys/types.h>
#include <vm/hat.h>
#if defined(sun4m) || defined(sun4d)
#include <vm/hat_srmmu.h>
#endif

ptbl
#if defined(sun4m) || defined(sun4d)
.>P
{SIZEOF}>E
$<<hme.sizeof
(((<P-*hments)%(<U*0x40))*<E)+*ptbls$<ptbl
#endif

