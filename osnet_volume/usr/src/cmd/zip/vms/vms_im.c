/*************************************************************************
 *                                                                       *
 * VMS portions copyright (C) 1993 Igor Mandrichenko.                    *
 * Permission is granted to any individual or institution to use, copy,  *
 * or redistribute this software so long as all of the original files    *
 * are included, that it is not sold for profit, and that this copyright *
 * notice is retained.                                                   *
 *                                                                       *
 *************************************************************************/

/*
 *  vms_im.c (zip) by Igor Mandrichenko    Version 2.2-2
 *
 *  Revision history:
 *  ...
 *  2.1-1       16-feb-1993     I.Mandrichenko
 *      Get file size from XABFHC and check bytes rest in file before
 *      reading.
 *  2.1-2       2-mar-1993      I.Mandrichenko
 *      Make code more standard
 *  2.2         21-jun-1993     I.Mandrichenko
 *      Free all allocated space, use more static storage.
 *      Use memcompress() from bits.c (deflation) for block compression.
 *      To revert to old compression method #define OLD_COMPRESS
 *  2.2-2       28-sep-1995     C.Spieler
 *      Reorganized code for easier maintance of the two incompatible
 *      flavours (IM style and PK style) VMS attribute support.
 *      Generic functions (common to both flavours) are now collected
 *      in a `wrapper' source file that includes one of the VMS attribute
 *      handlers.
 */

#ifdef VMS                      /* For VMS only ! */

#define OLD_COMPRESS            /*To use old compression method define it.*/

#ifdef VMS_ZIP
#undef VMS_ZIP                  /* do NOT include PK style Zip definitions */
#endif
#include "vms.h"

#ifndef __LIB$ROUTINES_LOADED
#include <lib$routines.h>
#endif

#ifndef UTIL

#define RET_ERROR 1
#define RET_SUCCESS 0
#define RET_EOF 0

#define Kbyte 1024

typedef struct XAB *xabptr;

/*
 *   Block sizes
 */

#define EXTL0   ((FABL + EXTHL)+        \
                (XFHCL + EXTHL)+        \
                (XPROL + EXTHL)+        \
                (XDATL + EXTHL)+        \
                (XRDTL + EXTHL))

#ifdef OLD_COMPRESS
#define PAD     sizeof(uch)
#else
#define PAD     10*sizeof(ush)          /* Two extra bytes for compr. header */
#endif

#define PAD0    (5*PAD)                 /* Reserve space for the case when
                                         *  compression fails */
static int _compress(uch *from, uch *to, int size);
#ifdef DEBUG
static void dump_rms_block(uch *p);
#endif /* DEBUG */

/********************************
 *   Function set_extra_field   *
 ********************************/

static uch *_compress_block(register struct IZ_block *to,
                            uch *from, int size, char *sig);
static int get_vms_version(char *verbuf, int len);

int set_extra_field(z, z_utim)
  struct zlist far *z;
  iztimes *z_utim;
/*
 *      Get file VMS file attributes and store them into extent fields.
 *      Store VMS version also.
 *      On error leave z intact.
 */
{
    int status;
    uch *extra=(uch*)NULL, *scan;
    extent extra_l;
    static struct FAB fab;
    static struct XABSUM xabsum;
    static struct XABFHC xabfhc;
    static struct XABDAT xabdat;
    static struct XABPRO xabpro;
    static struct XABRDT xabrdt;
    xabptr x = (xabptr)NULL, xab_chain = (xabptr)NULL, last_xab = (xabptr)NULL;
    int nk, na;
    int i;
    int rc=RET_ERROR;
    char verbuf[80];
    int verlen = 0;

    if (!vms_native)
    {
#ifdef USE_EF_UT_TIME
       /*
        *  A `portable' zipfile entry is created. Create an "UT" extra block
        *  containing UNIX style modification time stamp in UTC, which helps
        *  maintaining the `real' "last modified" time when the archive is
        *  transfered across time zone boundaries.
        */
       if ((extra = (uch *)malloc(EB_HEADSIZE+EB_UT_LEN(1))) == NULL)
           return ZE_MEM;

       extra[0]  = 'U';
       extra[1]  = 'T';
       extra[2]  = EB_UT_LEN(1);          /* length of data part of e.f. */
       extra[3]  = 0;
       extra[4]  = EB_UT_FL_MTIME;
       extra[5]  = (uch)(z_utim->mtime);
       extra[6]  = (uch)(z_utim->mtime >> 8);
       extra[7]  = (uch)(z_utim->mtime >> 16);
       extra[8]  = (uch)(z_utim->mtime >> 24);

       z->cext = z->ext = (EB_HEADSIZE+EB_UT_LEN(1));
       z->cextra = z->extra = (char*)extra;
#endif /* USE_EF_UT_TIME */

       return RET_SUCCESS;
    }

    /*
     *  Initialize RMS control blocks and link them
     */

    fab =    cc$rms_fab;
    xabsum = cc$rms_xabsum;
    xabdat = cc$rms_xabdat;
    xabfhc = cc$rms_xabfhc;
    xabpro = cc$rms_xabpro;
    xabrdt = cc$rms_xabrdt;


    fab.fab$l_xab = (char*)&xabsum;
    /*
     *  Open the file and read summary information.
     */
    fab.fab$b_fns = strlen(z->name);
    fab.fab$l_fna = z->name;

    status = sys$open(&fab);
    if (ERR(status))
    {
#ifdef DEBUG
        printf("set_extra_field: sys$open for file %s:\n  error status = %d\n",
               z->name, status);
#endif
        goto err_exit;
    }

    nk = xabsum.xab$b_nok;
    na = xabsum.xab$b_noa;
#ifdef DEBUG
    printf("%d keys, %d alls\n", nk, na);
#endif

    /*
     *  Allocate XABKEY and XABALL blocks and link them
     */

    xabfhc.xab$l_nxt = (char*)&xabdat;
    xabdat.xab$l_nxt = (char*)&xabpro;
    xabpro.xab$l_nxt = (char*)&xabrdt;
    xabrdt.xab$l_nxt = NULL;

    xab_chain = (xabptr)(&xabfhc);
    last_xab  = (xabptr)(&xabrdt);

#define INIT(ptr,size,type,init)     \
        if ( (ptr = (type *)malloc(size)) == NULL )     \
        {                                               \
              printf( "set_extra_field: Insufficient memory.\n" );   \
                      goto err_exit;                    \
        }                                               \
        *(ptr) = (init);
    /*
     *  Allocate and initialize all needed XABKEYs and XABALLs
     */
    for (i = 0; i < nk; i++)
    {
        struct XABKEY *k;
        INIT(k, XKEYL, struct XABKEY, cc$rms_xabkey);
        k->xab$b_ref = i;
        if (last_xab != NULL)
            last_xab->xab$l_nxt = (char*)k;
        last_xab = (xabptr)k;
    }
    for (i = 0; i < na; i++)
    {
        struct XABALL *a;
        INIT(a, XALLL, struct XABALL, cc$rms_xaball);
        a->xab$b_aid = i;
        if (last_xab != NULL)
            last_xab->xab$l_nxt = (char*)a;
        last_xab = (xabptr)a;
    }

    fab.fab$l_xab = (char*)xab_chain;
#ifdef DEBUG
    printf("Dump of XAB chain before $DISPLAY:\n");
    for (x = xab_chain; x != NULL; x = x->xab$l_nxt)
        dump_rms_block((uch *)x);
#endif
    /*
     *  Get information on the file structure etc.
     */
    status = sys$display(&fab, 0, 0);
    if (ERR(status))
    {
#ifdef DEBUG
        printf("set_extra_field: sys$display for file %s:\n  error status = %d\n",
               z->name, status);
#endif
        goto err_exit;
    }

#ifdef DEBUG
    printf("\nDump of XAB chain after $DISPLAY:\n");
    for (x = xab_chain; x != NULL; x = x->xab$l_nxt)
        dump_rms_block((uch *)x);
#endif

    fab.fab$l_xab = NULL;  /* Keep XABs */
    status = sys$close(&fab);
    if (ERR(status))
    {
#ifdef DEBUG
        printf("set_extra_field: sys$close for file %s:\n  error status = %d\n",
               z->name, status);
#endif
        goto err_exit;
    }

    extra_l = EXTL0 + nk * (XKEYL + EXTHL) + na * (XALLL + EXTHL);
#ifndef OLD_COMPRESS
    extra_l += PAD0 + (nk+na) * PAD;
#endif

    if ( (verlen = get_vms_version(verbuf, sizeof(verbuf))) > 0 )
    {
        extra_l += verlen + EXTHL;
#ifndef OLD_COMPRESS
        extra_l += PAD;
#endif
    }

    if ((scan = extra = (uch *) malloc(extra_l)) == (uch*)NULL)
    {
#ifdef DEBUG
        printf("set_extra_field: Insufficient memory to allocate extra buffer\n");
#endif
        goto err_exit;
    }


    if (verlen > 0)
        scan = _compress_block((struct IZ_block *)scan, (uch *)verbuf,
                               verlen, VERSIG);

    /*
     *  Zero all unusable fields to improve compression
     */
    fab.fab$b_fns = fab.fab$b_shr = fab.fab$b_dns = fab.fab$b_fac = 0;
    fab.fab$w_ifi = 0;
    fab.fab$l_stv = fab.fab$l_sts = fab.fab$l_ctx = 0;
    fab.fab$l_fna = NULL;
    fab.fab$l_nam = NULL;
    fab.fab$l_xab = NULL;
    fab.fab$l_dna = NULL;

#ifdef DEBUG
    dump_rms_block( (uch *)&fab );
#endif
    scan = _compress_block((struct IZ_block *)scan, (uch *)&fab, FABL, FABSIG);
    for (x = xab_chain; x != NULL;)
    {
        int bln;
        char *sig;
        xabptr next;

        next = (xabptr)(x->xab$l_nxt);
        x->xab$l_nxt = 0;

        switch (x->xab$b_cod)
        {
            case XAB$C_ALL:
                bln = XALLL;
                sig = XALLSIG;
                break;
            case XAB$C_KEY:
                bln = XKEYL;
                sig = XKEYSIG;
                break;
            case XAB$C_PRO:
                bln = XPROL;
                sig = XPROSIG;
                break;
            case XAB$C_FHC:
                bln = XFHCL;
                sig = XFHCSIG;
                break;
            case XAB$C_DAT:
                bln = XDATL;
                sig = XDATSIG;
                break;
            case XAB$C_RDT:
                bln = XRDTL;
                sig = XRDTSIG;
                break;
            default:
                bln = 0;
                sig = 0L;
                break;
        }
        if (bln > 0)
            scan = _compress_block((struct IZ_block *)scan, (uch *)x,
                                   bln, sig);
        x = next;
    }

    z->ext = z->cext = scan-extra;
    z->extra = z->cextra = (char*)extra;
    rc = RET_SUCCESS;

err_exit:
    /*
     *  Give up all allocated blocks
     */
    for (x = (struct XAB *)xab_chain; x != NULL; )
    {
        struct XAB *next;
        next = (xabptr)(x->xab$l_nxt);
        if (x->xab$b_cod == XAB$C_ALL || x->xab$b_cod == XAB$C_KEY)
            free(x);
        x = next;
    }
    return rc;
}

static int get_vms_version(verbuf, len)
char *verbuf;
int len;
{
    int i = SYI$_VERSION;
    int verlen = 0;
    struct dsc$descriptor version;
    char *m;

    version.dsc$a_pointer = verbuf;
    version.dsc$w_length  = len - 1;
    version.dsc$b_dtype   = DSC$K_DTYPE_B;
    version.dsc$b_class   = DSC$K_CLASS_S;

    if (ERR(lib$getsyi(&i, 0, &version, &verlen, 0, 0)) || verlen == 0)
        return 0;

    /* Cut out trailing spaces "V5.4-3   " -> "V5.4-3" */
    for (m = verbuf + verlen, i = verlen - 1; i > 0 && verbuf[i] == ' '; --i)
        --m;
    *m = 0;

    /* Cut out release number "V5.4-3" -> "V5.4" */
    if ((m = strrchr(verbuf, '-')) != NULL)
        *m = 0;
    return strlen(verbuf) + 1;  /* Transmit ending 0 too */
}

#define CTXSIG ((ulg)('CtXx'))

typedef struct user_context
{
    ulg sig;
    struct FAB *fab;
    struct RAB *rab;
    ulg size,rest;
    int status;
} Ctx, *Ctxptr;

Ctx init_ctx =
{
        CTXSIG,
        NULL,
        NULL,
        0L,
        0L,
        0
};

#define CTXL    sizeof(Ctx)
#define CHECK_RAB(_r) ( (_r) != NULL &&                         \
                        (_r) -> rab$b_bid == RAB$C_BID &&       \
                        (_r) -> rab$b_bln == RAB$C_BLN &&       \
                        (_r) -> rab$l_ctx != 0         &&       \
                        (_r) -> rab$l_fab != NULL )

/**************************
 *   Function vms_open    *
 **************************/
struct RAB *vms_open(name)
    char *name;
{
    struct RAB *rab;
    struct FAB *fab;
    struct XABFHC *fhc;
    Ctxptr ctx;

    if ((fab = (struct FAB *) malloc(FABL)) == (struct FAB *)NULL)
        return NULL;
    if ((rab = (struct RAB *) malloc(RABL)) == (struct RAB *)NULL)
    {
        free(fab);
        return (struct RAB *)NULL;
    }
    if ((fhc = (struct XABFHC *) malloc(XFHCL)) == (struct XABFHC *)NULL)
    {
        free(rab);
        free(fab);
        return (struct RAB *)NULL;
    }
    if ((ctx = (Ctxptr) malloc(CTXL)) == (Ctxptr)NULL)
    {
        free(fhc);
        free(fab);
        free(rab);
        return (struct RAB *)NULL;
    }
    *fab = cc$rms_fab;
    *rab = cc$rms_rab;
    *fhc = cc$rms_xabfhc;

    fab->fab$l_fna = name;
    fab->fab$b_fns = strlen(name);
    fab->fab$b_fac = FAB$M_GET | FAB$M_BIO;
    fab->fab$l_xab = (char*)fhc;

    if (ERR(sys$open(fab)))
    {
        sys$close(fab);
        free(fhc);
        free(fab);
        free(rab);
        free(ctx);
        return (struct RAB *)NULL;
    }

    rab->rab$l_fab = fab;
    rab->rab$l_rop = RAB$M_BIO;

    if (ERR(sys$connect(rab)))
    {
        sys$close(fab);
        free(fab);
        free(rab);
        free(ctx);
        return (struct RAB *)NULL;
    }

    *ctx = init_ctx;
    ctx->rab = rab;
    ctx->fab = fab;

    if (fhc->xab$l_ebk > 0)
        ctx->size = ctx->rest = ( fhc->xab$l_ebk-1 ) * 512 + fhc->xab$w_ffb;
    else if ( fab->fab$b_org == FAB$C_IDX
             || fab->fab$b_org == FAB$C_REL
             || fab->fab$b_org == FAB$C_HSH )
                /* Special case, when ebk=0: save entire allocated space */
        ctx->size = ctx->rest = fhc->xab$l_hbk * 512;
    else
        ctx->size = ctx->rest = fhc->xab$w_ffb;

    free(fhc);
    fab->fab$l_xab = NULL;
    rab->rab$l_ctx = (unsigned) ctx;
    return rab;
}

/**************************
 *   Function vms_close   *
 **************************/
int vms_close(rab)
    struct RAB *rab;
{
    struct FAB *fab;
    Ctxptr ctx;

    if (!CHECK_RAB(rab))
        return RET_ERROR;
    fab = (ctx = (Ctxptr)(rab->rab$l_ctx))->fab;
    sys$close(fab);

    free(fab);
    free(rab);
    free(ctx);

    return RET_SUCCESS;
}

/**************************
 *   Function vms_rewind  *
 **************************/
int vms_rewind(rab)
    struct RAB *rab;
{
    Ctxptr ctx;

    int status;
    if (!CHECK_RAB(rab))
        return RET_ERROR;

    ctx = (Ctxptr) (rab->rab$l_ctx);
    if (ERR(status = sys$rewind(rab)))
    {
        ctx->status = status;
        return RET_ERROR;
    }

    ctx->status = 0;
    ctx->rest = ctx->size;

    return RET_SUCCESS;
}

/**************************
 *   Function vms_read    *
 **************************/
int vms_read(rab, buf, size)
    struct RAB *rab;
char *buf;
int size;
/*
 *      size must be greater or equal to 512 !
 */
{
    int status;
    Ctxptr ctx;

    ctx = (Ctxptr)rab->rab$l_ctx;

    if (!CHECK_RAB(rab))
        return 0;

    if (ctx -> rest <= 0)
        return 0;               /* Eof */

    if (size > 16*Kbyte)        /* RMS can not read too much */
        size = 16*Kbyte;
    else
        size &= ~511L;

    rab->rab$l_ubf = buf;
    rab->rab$w_usz = size;
    status = sys$read(rab);
    if (!ERR(status) && rab->rab$w_rsz > 0)
    {
        ctx -> status = 0;
        ctx -> rest -= rab->rab$w_rsz;
        return rab->rab$w_rsz;
    }
    else
    {
        ctx->status = (status==RMS$_EOF ? 0:status);
        if (status == RMS$_EOF)
                ctx -> rest = 0L;
        return 0;
    }
}

/**************************
 *   Function vms_error   *
 **************************/
int vms_error(rab)
    struct RAB *rab;
{
    if (!CHECK_RAB(rab))
        return RET_ERROR;
    return ((Ctxptr) (rab->rab$l_ctx))->status;
}


#ifdef DEBUG
static void dump_rms_block(p)
    uch *p;
{
    uch bid, len;
    int err;
    char *type;
    char buf[132];
    int i;

    err = 0;
    bid = p[0];
    len = p[1];
    switch (bid)
    {
        case FAB$C_BID:
            type = "FAB";
            break;
        case XAB$C_ALL:
            type = "xabALL";
            break;
        case XAB$C_KEY:
            type = "xabKEY";
            break;
        case XAB$C_DAT:
            type = "xabDAT";
            break;
        case XAB$C_RDT:
            type = "xabRDT";
            break;
        case XAB$C_FHC:
            type = "xabFHC";
            break;
        case XAB$C_PRO:
            type = "xabPRO";
            break;
        default:
            type = "Unknown";
            err = 1;
            break;
    }
    printf("Block @%08X of type %s (%d).", p, type, bid);
    if (err)
    {
        printf("\n");
        return;
    }
    printf(" Size = %d\n", len);
    printf(" Offset - Hex - Dec\n");
    for (i = 0; i < len; i += 8)
    {
        int j;

        printf("%3d - ", i);
        for (j = 0; j < 8; j++)
            if (i + j < len)
                printf("%02X ", p[i + j]);
            else
                printf("   ");
        printf(" - ");
        for (j = 0; j < 8; j++)
            if (i + j < len)
                printf("%03d ", p[i + j]);
            else
                printf("    ");
        printf("\n");
    }
}
#endif /* DEBUG */

#ifdef OLD_COMPRESS
# define BC_METHOD      BC_00
# define        COMP_BLK(to,tos,from,froms) _compress( from,to,froms )
#else
# define BC_METHOD      BC_DEFL
# define        COMP_BLK(to,tos,from,froms) memcompress(to,tos,from,froms)
#endif

static uch *_compress_block(to,from,size,sig)
register struct IZ_block *to;
uch *from;
int size;
char *sig;
{
        ulg cl;
        to -> sig =  *(ush*)IZ_SIGNATURE;
        to -> bid =       *(ulg*)(sig);
        to -> flags =           BC_METHOD;
        to -> length =  size;
#ifdef DEBUG
        printf("\nmemcompr(%d,%d,%d,%d)\n",&(to->body[0]),size+PAD,from,size);
#endif
        cl = COMP_BLK( &(to->body[0]), size+PAD, from, size );
#ifdef DEBUG
        printf("Compressed to %d\n",cl);
#endif
        if (cl >= size)
        {
                memcpy(&(to->body[0]), from, size);
                to->flags = BC_STORED;
                cl = size;
#ifdef DEBUG
                printf("Storing block...\n");
#endif
        }
        return (uch*)(to) + (to->size = cl + EXTBSL + RESL) + EB_HEADSIZE;
}

#define NBITS 32

static int _compress(from,to,size)
uch *from,*to;
int size;
{
    int off=0;
    ulg bitbuf=0;
    int bitcnt=0;
    int i;

#define _BIT(val,len)   {                       \
        if (bitcnt + (len) > NBITS)             \
            while(bitcnt >= 8)                  \
            {                                   \
                to[off++] = (uch)bitbuf;        \
                bitbuf >>= 8;                   \
                bitcnt -= 8;                    \
            }                                   \
        bitbuf |= ((ulg)(val))<<bitcnt;         \
        bitcnt += len;                          \
    }

#define _FLUSH  {                               \
            while(bitcnt>0)                     \
            {                                   \
                to[off++] = (uch)bitbuf;        \
                bitbuf >>= 8;                   \
                bitcnt -= 8;                    \
            }                                   \
        }

    for (i=0; i<size; i++)
    {
        if (from[i])
        {
                _BIT(1,1);
                _BIT(from[i],8);
        }
        else
            _BIT(0,1);
    }
    _FLUSH;
    return off;
}

#endif /* !UTIL */
#endif /* VMS */
