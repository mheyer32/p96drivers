#include "radeon_driver.hpp"

#include "radeon_render.h"
#include "radeon_resource.h"

using namespace RadeonReg;

constexpr auto R100_CONTEXT_WORDS          = 57u;
constexpr auto R200_CONTEXT_BASE_WORDS     = 90u;
constexpr auto R200_CONTEXT_COMBINER_WORDS = 4u;
constexpr auto R200_CONTEXT_WORDS          = (R200_CONTEXT_BASE_WORDS + 1 + R200_CONTEXT_COMBINER_WORDS);
constexpr auto R200_CONTEXT_CAPACITY =
    (R200_CONTEXT_BASE_WORDS + 1 + RADEON_RENDER_STAGES * R200_CONTEXT_COMBINER_WORDS);
constexpr auto RADEON_CONTEXT_DEFAULT_WIDTH  = 1280u;
constexpr auto RADEON_CONTEXT_DEFAULT_HEIGHT = 1024u;
constexpr auto RADEON_CONTEXT_DEFAULT_SIZE =
    ((RADEON_CONTEXT_DEFAULT_HEIGHT << RADEON_TEXTURE_DIMENSION_SHIFT) | RADEON_CONTEXT_DEFAULT_WIDTH);
constexpr auto RADEON_CONTEXT_DEFAULT_PITCH = (RADEON_CONTEXT_DEFAULT_WIDTH | RADEON_RENDER_COLOR_SWAP);
constexpr auto RADEON_RESOURCE_NAME_BYTES   = 16u;
constexpr auto RADEON_PCI_COMPONENT_DIGITS  = 3u;
constexpr auto RADEON_DECIMAL_BASE          = 10u;

struct RadeonResourceBoard
{
    struct Node node;
    struct BoardInfo *bi;
    ULONG opens;
    struct SignalSemaphore semaphore;
    char name[RADEON_RESOURCE_NAME_BYTES];
};

static struct List resourceBoards;
static struct ExecBase *resourceExecBase;
static BOOL resourcePublished;

/* Register packets keep the context layout stable while giving each state
 * group a hardware name. Clients can update values and submit the context
 * without depending on the driver's internal rendering-state structure.
 */
/* clang-format off: keep each register packet grouped with its values. */
static const ULONG r100Context[] = {radeon_packet0(RADEON_RE_SOLID_COLOR, 2),
                                    ~0UL,
                                    RADEON_BLEND_SOURCE_ALPHA | RADEON_BLEND_DEST_INV_SOURCE_ALPHA,
                                    radeon_packet0(RADEON_RB3D_STENCILREFMASK, 2),
                                    0,
                                    0,
                                    radeon_packet0(RADEON_PP_CNTL, 7),
                                    R200_RENDER_PIXEL_CONTROL,
                                    RADEON_COLOR_ARGB8888,
                                    0,
                                    RADEON_CONTEXT_DEFAULT_SIZE,
                                    RADEON_CONTEXT_DEFAULT_PITCH,
                                    RADEON_RENDER_SETUP_DEFAULT,
                                    R100_RENDER_COORDINATES_DEFAULT,
                                    radeon_packet0(RADEON_PP_TXFILTER_0, 6),
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    radeon_packet0(RADEON_PP_TXFILTER_0 + R100_TEXTURE_REGISTER_STRIDE, 6),
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    radeon_packet0(RADEON_RB3D_PLANEMASK, 1),
                                    ~0UL,
                                    radeon_packet0(RADEON_RE_TOP_LEFT, 1),
                                    0,
                                    radeon_packet0(RADEON_PP_TEX_SIZE_0, 2),
                                    0,
                                    0,
                                    radeon_packet0(RADEON_PP_TEX_SIZE_0 + R100_TEXTURE_SIZE_STRIDE, 2),
                                    0,
                                    0,
                                    radeon_packet0(RADEON_PP_TXFILTER_0 + 2 * R100_TEXTURE_REGISTER_STRIDE, 6),
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    radeon_packet0(RADEON_PP_MISC, 1),
                                    RADEON_RENDER_MISC_DEFAULT,
                                    radeon_packet0(RADEON_RB3D_DEPTHOFFSET, 3),
                                    0,
                                    0,
                                    RADEON_RENDER_ZSTENCIL_DEFAULT,
                                    radeon_packet0(RADEON_PP_FOG_COLOR, 1),
                                    RADEON_RENDER_FOG_MASK,
                                    radeon_packet0(RADEON_RE_LINE_WIDTH, 1),
                                    0,
                                    radeon_packet0(RADEON_RE_AUX_SCISSOR_CNTL, 1),
                                    0};

static const ULONG r200Context[R200_CONTEXT_CAPACITY] = {
    radeon_packet0(RADEON_PP_MISC, 4),
    RADEON_RENDER_MISC_DEFAULT,
    RADEON_RENDER_FOG_MASK,
    ~0UL,
    RADEON_BLEND_SOURCE_ALPHA | RADEON_BLEND_DEST_INV_SOURCE_ALPHA,
    radeon_packet0(RADEON_RB3D_ZSTENCILCNTL, 1),
    RADEON_RENDER_ZSTENCIL_DEFAULT,
    radeon_packet0(RADEON_RB3D_CNTL, 6),
    RADEON_COLOR_ARGB8888,
    0,
    RADEON_CONTEXT_DEFAULT_SIZE,
    RADEON_CONTEXT_DEFAULT_PITCH,
    RADEON_RENDER_SETUP_DEFAULT,
    R200_RENDER_COORDINATES_DEFAULT,
    radeon_packet0(RADEON_RB3D_PLANEMASK, 1),
    ~0UL,
    radeon_packet0(R200_SE_VTX_FMT_0, 2),
    0,
    3,
    radeon_packet0(RADEON_RE_TOP_LEFT, 1),
    0,
    radeon_packet0(R200_PP_TXFILTER_0, 3),
    0,
    0,
    0,
    radeon_packet0(R200_PP_TXFILTER_0 + R200_TEXTURE_REGISTER_STRIDE, 3),
    0,
    0,
    0,
    radeon_packet0(R200_PP_TEXTURE_CONTROL, 1),
    R200_RENDER_PIXEL_CONTROL,
    radeon_packet0(R200_PP_TXOFFSET_0, 1),
    0,
    radeon_packet0(R200_PP_TXOFFSET_0 + R200_TEXTURE_OFFSET_STRIDE, 1),
    0,
    radeon_packet0(R200_PP_TXSIZE_0, 2),
    0,
    0,
    radeon_packet0(R200_PP_TXSIZE_0 + R200_TEXTURE_REGISTER_STRIDE, 2),
    0,
    0,
    radeon_packet0(R200_SE_VTE_CNTL, 1),
    R200_RENDER_VERTEX_CONTROL,
    radeon_packet0(R200_RB3D_ABLENDCNTL, 1),
    0,
    radeon_packet0(RADEON_RB3D_DEPTHOFFSET, 2),
    0,
    0,
    radeon_packet0(RADEON_RE_STIPPLE_ADDR, 1),
    0,
    radeon_packet0(RADEON_RE_LINE_PATTERN, 1),
    RADEON_RENDER_LINE_PATTERN,
    radeon_packet0(RADEON_RB3D_STENCILREFMASK, 2),
    RADEON_RENDER_STENCIL_MASK,
    0,
    radeon_packet0(RADEON_RE_LINE_WIDTH, 1),
    0,
    radeon_packet0(R200_PP_TXFILTER_0 + 3 * R200_TEXTURE_REGISTER_STRIDE, 3),
    0,
    0,
    0,
    radeon_packet0(R200_PP_TXFILTER_0 + 4 * R200_TEXTURE_REGISTER_STRIDE, 3),
    0,
    0,
    0,
    radeon_packet0(R200_PP_TXFILTER_0 + 5 * R200_TEXTURE_REGISTER_STRIDE, 3),
    0,
    0,
    0,
    radeon_packet0(R200_PP_TXMULTI_CTL_0, 1),
    0,
    radeon_packet0(R200_PP_TXOFFSET_0 + 3 * R200_TEXTURE_OFFSET_STRIDE, 1),
    0,
    radeon_packet0(R200_PP_TXOFFSET_0 + 4 * R200_TEXTURE_OFFSET_STRIDE, 1),
    0,
    radeon_packet0(R200_PP_TXOFFSET_0 + 5 * R200_TEXTURE_OFFSET_STRIDE, 1),
    0,
    radeon_packet0(R200_PP_TFACTOR_0, 6),
    0,
    0,
    0,
    0,
    0,
    0,
    radeon_packet0(R200_PP_TXFILTER_0 + 2 * R200_TEXTURE_REGISTER_STRIDE, 3),
    0,
    0,
    0,
    radeon_packet0(R200_PP_TXOFFSET_0 + 2 * R200_TEXTURE_OFFSET_STRIDE, 1),
    0,
    radeon_packet0(R200_PP_TXCBLEND_0, R200_CONTEXT_COMBINER_WORDS),
    R200_RENDER_ALPHA_TEXTURE,
    R200_RENDER_COMBINER_CLAMP,
    R200_RENDER_ALPHA_TEXTURE,
    R200_RENDER_COMBINER_CLAMP};

/* clang-format on */

STATIC_ASSERT(ARRAY_SIZE(r100Context) == R100_CONTEXT_WORDS, radeon_r100_context_size);
STATIC_ASSERT(offsetof(struct RadeonCPResource, Open) == sizeof(struct Node) + sizeof(ULONG),
              radeon_resource_method_offset);
STATIC_ASSERT(sizeof(struct RadeonCPResource) == sizeof(struct Node) + 14 * sizeof(ULONG),
              radeon_resource_interface_size);

static char *append_decimal(char *out, UBYTE value)
{
    char digits[RADEON_PCI_COMPONENT_DIGITS];
    ULONG count = 0;
    do {
        digits[count++] = '0' + value % RADEON_DECIMAL_BASE;
        value /= RADEON_DECIMAL_BASE;
    } while (value);
    while (count)
        *out++ = digits[--count];
    return out;
}

static BOOL same_name(CONST_STRPTR a, CONST_STRPTR b)
{
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

static struct RadeonResourceBoard *__stdargs resource_open(CONST_STRPTR name)
{
    struct ExecBase *SysBase          = resourceExecBase;
    struct RadeonResourceBoard *found = NULL;
    Forbid();
    for (struct Node *node = resourceBoards.lh_Head; node->ln_Succ; node = node->ln_Succ) {
        struct RadeonResourceBoard *board = (struct RadeonResourceBoard *)node;
        if ((!name || same_name(name, node->ln_Name) || same_name(name, board->bi->BoardName)) &&
            getCardData(board->bi)->state->cp.active) {
            ++board->opens;
            found = board;
            break;
        }
    }
    Permit();
    return found;
}

static void __stdargs resource_close(struct RadeonResourceBoard *board)
{
    struct BoardInfo *bi = board->bi;
    LOCAL_SYSBASE();
    Forbid();
    if (board->opens)
        --board->opens;
    Permit();
}

static ULONG __stdargs resource_wait(struct RadeonResourceBoard *board)
{
    return radeon_wait_idle(board->bi) ? 0 : 1;
}

static ULONG __stdargs resource_lock(struct RadeonResourceBoard *board)
{
    struct BoardInfo *bi = board->bi;
    LOCAL_SYSBASE();
    ObtainSemaphore(&board->semaphore);
    return 0;
}

static void __stdargs resource_unlock(struct RadeonResourceBoard *board)
{
    struct BoardInfo *bi = board->bi;
    LOCAL_SYSBASE();
    ReleaseSemaphore(&board->semaphore);
}

static ULONG __stdargs resource_submit(struct RadeonResourceBoard *board, const ULONG *words, ULONG count)
{
    resource_lock(board);
    ULONG result = radeon_cp_submit(board->bi, words, count) ? 0 : 1;
    resource_unlock(board);
    return result;
}

static ULONG __stdargs resource_engine(struct RadeonResourceBoard *board)
{
    return getCardData(board->bi)->state->device->engine;
}

static ULONG __stdargs resource_family(struct RadeonResourceBoard *board)
{
    return getCardData(board->bi)->state->device->family;
}

static struct RadeonResourceContext *__stdargs resource_alloc_context(struct RadeonResourceBoard *board)
{
    struct BoardInfo *bi = board->bi;
    LOCAL_SYSBASE();
    BOOL r100                             = resource_engine(board) == RADEON_ENGINE_R100;
    ULONG capacity                        = r100 ? R100_CONTEXT_WORDS : R200_CONTEXT_CAPACITY;
    ULONG extra                           = r100 ? 0 : sizeof(ULONG);
    struct RadeonResourceContext *context = static_cast<RadeonResourceContext *>(
        AllocVec(sizeof(*context) + extra + capacity * sizeof(ULONG), MEMF_PUBLIC | MEMF_CLEAR));
    if (!context)
        return NULL;
    context->wordCount     = r100 ? R100_CONTEXT_WORDS : R200_CONTEXT_WORDS;
    context->baseWordCount = r100 ? R100_CONTEXT_WORDS : R200_CONTEXT_BASE_WORDS;
    context->commands      = (ULONG *)((UBYTE *)(context + 1) + extra);
    context->dirty         = ~0UL;
    if (!r100)
        *(ULONG *)(context + 1) = 1;
    const ULONG *defaults = r100 ? r100Context : r200Context;
    for (ULONG i = 0; i < capacity; ++i)
        context->commands[i] = defaults[i];
    return context;
}

static void __stdargs resource_free_context(struct RadeonResourceBoard *board, struct RadeonResourceContext *context)
{
    struct BoardInfo *bi = board->bi;
    LOCAL_SYSBASE();
    FreeVec(context);
}

static void __stdargs resource_set_context(struct RadeonResourceBoard *board, struct RadeonResourceContext *context)
{
    ULONG capacity = resource_engine(board) == RADEON_ENGINE_R100 ? R100_CONTEXT_WORDS : R200_CONTEXT_CAPACITY;
    if (!context || context->wordCount > capacity)
        return;
    /* A full restore also covers changes made by P96 or another resource
     * client since this context was last submitted.
     */
    if (!resource_submit(board, context->commands, context->wordCount))
        context->dirty = 0;
}

static struct RadeonCPResource resource = {{NULL, NULL, NT_RESOURCE, 0, const_cast<char *>(RADEON_CP_RESOURCE_NAME)},
                                           RADEON_CP_RESOURCE_VERSION,
                                           resource_open,
                                           resource_close,
                                           resource_wait,
                                           resource_lock,
                                           resource_unlock,
                                           resource_submit,
                                           resource_engine,
                                           resource_family,
                                           resource_alloc_context,
                                           resource_free_context,
                                           resource_set_context,
                                           resource_submit,
                                           0};

BOOL radeon_resource_attach(struct BoardInfo *bi)
{
    struct RadeonState *state = getCardData(bi)->state;
    LOCAL_SYSBASE();
    if (state->resourceBoard)
        return TRUE;
    if (!state->cp.active)
        return FALSE;
    struct RadeonResourceBoard *board =
        static_cast<RadeonResourceBoard *>(AllocVec(sizeof(*board), MEMF_PUBLIC | MEMF_CLEAR));
    if (!board)
        return FALSE;
    char *name          = board->name;
    *name++             = 'P';
    *name++             = 'C';
    *name++             = 'I';
    *name++             = ':';
    name                = append_decimal(name, state->pciBus);
    *name++             = '.';
    name                = append_decimal(name, state->pciSlot);
    *name++             = '.';
    name                = append_decimal(name, state->pciFunction);
    *name               = '\0';
    board->node.ln_Name = board->name;
    board->bi           = bi;
    InitSemaphore(&board->semaphore);
    Forbid();
    if (!resourcePublished) {
        if (OpenResource(RADEON_CP_RESOURCE_NAME)) {
            Permit();
            FreeVec(board);
            return FALSE;
        }
        resourceBoards.lh_Head     = (struct Node *)&resourceBoards.lh_Tail;
        resourceBoards.lh_Tail     = NULL;
        resourceBoards.lh_TailPred = (struct Node *)&resourceBoards.lh_Head;
        resourceExecBase           = SysBase;
        AddResource(&resource);
        resourcePublished = TRUE;
    }
    AddTail(&resourceBoards, &board->node);
    state->resourceBoard = board;
    Permit();
    return TRUE;
}

void radeon_resource_detach(struct BoardInfo *bi)
{
    struct RadeonState *state         = getCardData(bi)->state;
    struct RadeonResourceBoard *board = state->resourceBoard;
    LOCAL_SYSBASE();
    if (!board)
        return;
    Forbid();
    Remove(&board->node);
    state->resourceBoard = NULL;
    if (!resourceBoards.lh_Head->ln_Succ) {
        RemResource(&resource);
        resourcePublished = FALSE;
    }
    Permit();
    FreeVec(board);
}

void radeon_resource_lock(struct BoardInfo *bi)
{
    struct RadeonResourceBoard *board = getCardData(bi)->state->resourceBoard;
    if (board)
        resource_lock(board);
}

void radeon_resource_unlock(struct BoardInfo *bi)
{
    struct RadeonResourceBoard *board = getCardData(bi)->state->resourceBoard;
    if (board)
        resource_unlock(board);
}
