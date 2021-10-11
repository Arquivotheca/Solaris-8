#include "_rtld.h"

rt_map
.$<<link_map{OFFSETOK}
+/"rt_alias.head"8t"rt_alias.tail"n{rt_alias.head,X}{rt_alias.tail,X}
+/"rt_init"16t"rt_fini"16t"rt_runpath"8t"rt_runlist"8tn{rt_init,X}{rt_fini,X}{rt_runpath,X}{rt_runlist,X}
+/"rt_count"8t"rt_depends.head"8t"rt_depends.tail"8t"rt_dlp"n{rt_count,D}{rt_depends.head,X}{rt_depends.tail,X}{rt_dlp,X}
+/"rt_count"8t"rt_parents.head"8t"rt_parents.tail"8t"rt_dlp"n{rt_count,D}{rt_parents.head,X}{rt_parents.tail,X}{rt_dlp,X}
+/"rt_permit"8t"rt_msize"8t"rt_etext"n{rt_permit,X}{rt_msize,X}{rt_etext,X}
+/"rt_padstart"8t"rt_padimlen"8t"rt_fct"n{rt_padstart,X}{rt_padimlen,X}{rt_fct,X}
+/"rt_filtees"8t"rt_lastdep"8t"rt_symintp"8t"rt_priv"n{rt_filtees,X}{rt_lastdep,X}{rt_symintp,X}{rt_priv,X}
+/"rt_list"16t"rt_flags"8t"rt_stdev"8t"rt_stino"n{rt_list,X}{rt_flags,X}{rt_stdev,D}{rt_stino,D}
+/"rt_refrom.head"8t"rt_refrom.tail"n{rt_refrom.head,X}{rt_refrom.tail,X}
+/"rt_pathname"8t"rt_dirsz"n{rt_pathname,X}{rt_dirsz,X}
