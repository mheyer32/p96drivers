#ifndef RADEON_OVERLAY_H
#define RADEON_OVERLAY_H

/* Later P96 versions extend the memory-window tags with buffer selection. */
#ifndef FA_NumBuffers
#define FA_NumBuffers      (TAG_USER + 38)
#define FA_DisplayedBuffer (TAG_USER + 39)
#define FA_RenderBuffer    (TAG_USER + 40)
#endif
constexpr auto RADEON_OVERLAY_BUFFERS = 3u;
constexpr auto RADEON_WINDOW_ACTIVE   = 1u;
constexpr auto RADEON_WINDOW_ONBOARD  = 2u;
constexpr auto RADEON_WINDOW_VISIBLE  = (RADEON_WINDOW_ACTIVE | RADEON_WINDOW_ONBOARD);

struct RadeonWindow
{
    struct RadeonWindow *next;
    UWORD numBuffers;
    UBYTE displayedBuffer, renderBuffer;
    UWORD left, top, width, height;
    UWORD clipLeft, clipTop;
    RGBFTYPE format;
    UWORD panX, panY;
    ULONG flags, changed;
    struct BitMap *buffers[RADEON_OVERLAY_BUFFERS];
    struct ModeInfo *mode;
    RGBFTYPE modeFormat;
    ULONG color, pen;
    UWORD occlusion, reserved, sourceWidth, sourceHeight;
    struct TagItem allocationTags[12];
};

/* Keep the register image separate from MMIO until the scaler grants its
 * update lock. This prevents a frame from using half of an updated setup.
 */
struct RadeonOverlay
{
    ULONG start, end, scale, vInc, vAccum1, vAccum23, blank1, blank23;
    ULONG hInc, step, hAccum1, hAccum23, xEnd1, xEnd2, xEnd3;
    ULONG videoKeyLow, videoKeyHigh, graphicsKeyLow, graphicsKeyHigh, key;
    ULONG base[6], pitch0, pitch1, filter, coefficient[5];
};
void radeon_overlay_scale(struct RadeonOverlay *, const struct RadeonWindow *);
void radeon_install_overlay(struct BoardInfo *);
void radeon_refresh_overlay(struct BoardInfo *);
void radeon_free_windows(struct BoardInfo *);
#endif
