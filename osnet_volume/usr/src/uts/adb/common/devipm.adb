#pragma ident	"@(#)devipm.adb	1.1	99/04/01 SMI"

#include <sys/types.h>
#include <sys/dditypes.h>
#include <sys/ddidmareq.h>
#include <sys/ddi_impldefs.h>

dev_info
.>t
{*devi_name,<t}/"name:"8ts
{*devi_addr,<t}/"addr:"8ts
<t/"parent"16t"child"16t"sibling"n{devi_parent,X}{devi_child,X}{devi_sibling,X}
+/"pm_info"16t"components"16t"comp_size"n{devi_pm_info,X}{devi_components,X}{devi_comp_size,X}
+/"comp_flags"16t"num_comps"16t"ppm"n{devi_comp_flags,p}{devi_num_components,X}{devi_ppm,X}
+/"ppm_private"16tn{devi_pm_private,X}{END}
