#define __NOLIBBASE__

#include <exec/execbase.h>
#include <exec/libraries.h>
#include <proto/exec.h>

#include <SDI_compiler.h>

#include "boardinfo.h"

#ifdef DBG
#include <clib/debug_protos.h>
#endif

/*--- Functions prototypes -------------------------------------------------*/

struct ChipBase *LibOpen(__REGA6(struct ChipBase *cb));
LONG LibClose(__REGA6(struct ChipBase *cb));
APTR LibExpunge(__REGA6(struct ChipBase *cb));
LONG LibReserved(void);
struct ChipBase *LibInit(__REGD0(struct ChipBase *cb), __REGA0(APTR seglist), __REGA6(struct Library *sysb));

BOOL InitChip(__REGA0(struct BoardInfo *bi));

int USED _start(void)
{
    return -1;
}

/*--------------------------------------------------------------------------*/
extern const char LibName[];
extern const char LibIdString[];
extern const UWORD LibVersion;
extern const UWORD LibRevision;

USED_VAR void *FuncTable[] = {(APTR)LibOpen, (APTR)LibClose, (APTR)LibExpunge, (APTR)LibReserved, (APTR)InitChip, (APTR)-1};

struct InitTable /* do not change */
{
    ULONG LibBaseSize;
    APTR *FunctionTable;
    struct MyDataInit *DataTable;
    APTR InitLibTable;
} InitTab = {(ULONG)sizeof(struct ChipBase), (APTR *)&FuncTable[0], NULL, (APTR)LibInit};

/* ------------------- ROM Tag ------------------------ */

USED_VAR const struct Resident ROMTag = /* do not change */
    {RTC_MATCHWORD,   (APTR)&ROMTag, &ROMTag.rt_Init + 1, RTF_AUTOINIT, 0, NT_LIBRARY, 0, &LibName[0],
     &LibIdString[0], &InitTab};

/*-------------------------------------------------------------------------*/
/* INIT                                                                    */
/*-------------------------------------------------------------------------*/

struct ChipBase *LibInit(__REGD0(struct ChipBase *cb), __REGA0(APTR seglist), __REGA6(struct Library *sysb))
{
    struct ExecBase *SysBase = (struct ExecBase *)sysb;

    if (!(SysBase->AttnFlags & AFF_68020)) {
        FreeMem((APTR)((ULONG)cb - (ULONG)cb->LibBase.lib_NegSize),
                (LONG)cb->LibBase.lib_PosSize + (LONG)cb->LibBase.lib_NegSize);
        return FALSE;
    }

    /* set up header data */

    cb->LibBase.lib_Node.ln_Type = NT_LIBRARY;
    cb->LibBase.lib_Node.ln_Name = (char *)LibName;
    cb->LibBase.lib_Flags = LIBF_CHANGED | LIBF_SUMUSED;
    cb->LibBase.lib_Version = (UWORD)LibVersion;
    cb->LibBase.lib_Revision = (UWORD)LibRevision;
    cb->LibBase.lib_IdString = (char *)LibIdString;

    /* setup private data */
    cb->ExecBase = SysBase;
    cb->SegList = seglist;
    return cb;
}

/*-------------------------------------------------------------------------*/
/* OPEN                                                                    */
/*-------------------------------------------------------------------------*/

struct ChipBase *LibOpen(__REGA6(struct ChipBase *cb))
{
    BOOL open = TRUE;

          //    if (cb->LibBase.lib_OpenCnt == 0)
          //        open = init_resources(cb);

    if (open) {
        cb->LibBase.lib_OpenCnt++;
        cb->LibBase.lib_Flags &= ~LIBF_DELEXP;

        return cb;
    }
    //    free_resources(cb);
    return NULL;
}

/*-------------------------------------------------------------------------*/
/* CLOSE                                                                   */
/*-------------------------------------------------------------------------*/

LONG LibClose(__REGA6(struct ChipBase *cb))
{
    if (!(--cb->LibBase.lib_OpenCnt)) {
        if (cb->LibBase.lib_Flags & LIBF_DELEXP)
            return ((long)LibExpunge(cb));
    }
    return 0;
}

/*-------------------------------------------------------------------------*/
/* EXPUNGE                                                                 */
/*-------------------------------------------------------------------------*/

void *LibExpunge(__REGA6(struct ChipBase *cb))
{
    struct ExecBase *SysBase = cb->ExecBase;

    APTR seglist;

    if (cb->LibBase.lib_OpenCnt) {
        cb->LibBase.lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    Remove((struct Node *)cb);
    //    free_resources(cb);
    seglist = cb->SegList;
    FreeMem((APTR)((ULONG)cb - (ULONG)cb->LibBase.lib_NegSize),
            (LONG)cb->LibBase.lib_PosSize + (LONG)cb->LibBase.lib_NegSize);
    return seglist;
}

/*-------------------------------------------------------------------------*/
/* RESERVED                                                                */
/*-------------------------------------------------------------------------*/

LONG LibReserved(void)
{
    return 0;
}
