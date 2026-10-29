#ifndef RADEON_RESOURCE_H
#define RADEON_RESOURCE_H

#include <exec/nodes.h>
#include <exec/types.h>

constexpr auto RADEON_CP_RESOURCE_NAME    = "radeoncp.resource";
constexpr auto RADEON_CP_RESOURCE_VERSION = 2u;

struct BoardInfo;
struct RadeonResourceBoard;

/* Context commands are native-endian packet words. R200 adds a stage count
 * between this header and its command storage; commands points past it.
 */
struct RadeonResourceContext
{
    ULONG wordCount;
    ULONG baseWordCount;
    ULONG *commands;
    ULONG dirty;
};

/* Resource methods use stack arguments, including when called by clients
 * whose normal Amiga library interfaces use register arguments.
 */
struct RadeonCPResource
{
    struct Node node;
    ULONG version;
    struct RadeonResourceBoard *(__stdargs *Open)(CONST_STRPTR name);
    void(__stdargs *Close)(struct RadeonResourceBoard *board);
    ULONG(__stdargs *Wait)(struct RadeonResourceBoard *board);
    ULONG(__stdargs *Lock)(struct RadeonResourceBoard *board);
    void(__stdargs *Unlock)(struct RadeonResourceBoard *board);
    ULONG(__stdargs *Submit)(struct RadeonResourceBoard *board, const ULONG *words, ULONG count);
    ULONG(__stdargs *GetEngine)(struct RadeonResourceBoard *board);
    ULONG(__stdargs *GetFamily)(struct RadeonResourceBoard *board);
    struct RadeonResourceContext *(__stdargs *AllocContext)(struct RadeonResourceBoard *board);
    void(__stdargs *FreeContext)(struct RadeonResourceBoard *board, struct RadeonResourceContext *context);
    void(__stdargs *SetContext)(struct RadeonResourceBoard *board, struct RadeonResourceContext *context);
    ULONG(__stdargs *SubmitRaw)(struct RadeonResourceBoard *board, const ULONG *words, ULONG count);
    ULONG reserved;
};

BOOL radeon_resource_attach(struct BoardInfo *bi);
void radeon_resource_detach(struct BoardInfo *bi);
void radeon_resource_lock(struct BoardInfo *bi);
void radeon_resource_unlock(struct BoardInfo *bi);
#endif
