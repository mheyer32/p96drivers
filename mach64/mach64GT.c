#include "mach64GT.h"
#include "chip_mach64.h"
#include "mach64_common.h"

static const UWORD defaultRegs_GT[] = {0x00a2, 0x7b33,  //
                                       0x00a0, 0x0000,  //
                                       0x0018, 0x0000,  //
                                       0x001c, 0x0200,  //
                                       0x001e, 0x0400,  //
                                       0x00e4, 0x0020,  //
                                       0x00b0, 0x1a73,  //
                                       0x00b2, 0x1075,  //
                                       0x00ac, 0x0c01,  //
                                       0x00ae, 0x2513,  //
                                       0x00d0, 0x0000,  //
                                       0x001e, 0x0000,  //
                                       0x0080, 0x0000,  //
                                       0x0082, 0x0000,  //
                                       0x0084, 0x0000,  //
                                       0x0086, 0x0000,  //
                                       0x00c4, 0x0002,  //
                                       0x00c6, 0x8000,  //
                                       0x007a, 0x0000,  //
                                       0x00d0, 0x0100,  //
                                       0x00ca, 0x0000,  //
                                       0x00d4, 0x0171,  //
                                       0x00d6, 0x003c,  //
                                       0x0028, 0x0000,  //
                                       0x002a, 0x0000,  //
                                       0x002c, 0x2848,  //
                                       0x002e, 0x0038,  //
                                       0x00ec, 0x0000,  //
                                       0x00ee, 0x0000,  //
                                       0x00fc, 0x0000,  //
                                       0x00fe, 0x0000,  //
                                       0x004c, 0x07f2,  //
                                       0x004e, 0x0560,  //
                                       0x0050, 0x05b5,  //
                                       0x0052, 0x03f8,  //
                                       0x007c, 0x3800,  //
                                       0x007e, 0x0084};

const UBYTE g_VCLKPllMultiplier[] = {1, 2, 3, 4, 5, 6, 8, 12};

const UBYTE g_VCLKPllMultiplierCode[] = {
    // *1,    *2,    *3,    *4,    *5,    *6,    *8,   *12
    0b000, 0b001, 0b100, 0b010, 0b101, 0b110, 0b011, 0b111};

static const UBYTE g_MCLKPllMultiplier[] = {1, 2, 3, 4, 8};

static const UBYTE g_MCLKMultiplierCode[] = {
    // *1,    *2,    *3,    *4,    *8
    0b000, 0b001, 0b100, 0b010, 0b011};

static const UBYTE g_SPllMultiplier[] = {1, 2, 4, 8};

static const UBYTE g_SPllMultiplierCode[] = {
    // *1,    *2,    *4,    *8
    0b000, 0b001, 0b010, 0b011};

ULONG computeFrequencyKhz10(UWORD RefFreq, UWORD FBDiv, UWORD RefDiv, UBYTE PostDiv)
{
    return ((ULONG)2 * RefFreq * FBDiv) / (RefDiv * PostDiv);
}

ULONG computeFrequencyKhz10FromPllValue(const BoardInfo_t *bi, const PLLValue_t *pllValues, const UBYTE *multipliers)
{
    const ChipData_t *cd = getConstChipData(bi);
    return computeFrequencyKhz10(cd->referenceFrequency, pllValues->N, cd->referenceDivider,
                                 multipliers[pllValues->Pidx]);
}

static BOOL inline isGoodVCOFrequency(ULONG freqKhz10)
{
    return freqKhz10 >= 11800 && freqKhz10 <= 23500;
}

ULONG computePLLValues(const BoardInfo_t *bi, ULONG freqKhz10, const UBYTE *multipliers, BYTE numMultipliers,
                       PLLValue_t *pllValues)
{
    DFUNC(CHATTY, "targetFrequency: %ld0 KHz\n", freqKhz10);

    UBYTE postDivIdx = 0xff;
    if (freqKhz10 < 1475) {
        freqKhz10  = 1475;
        postDivIdx = 7;
    }

    if (freqKhz10 > 23500) {
        freqKhz10 = 23500;
    }

    if (postDivIdx == 0xff) {
        for (BYTE i = numMultipliers - 1; i >= 0; --i) {
            if (isGoodVCOFrequency(freqKhz10 * multipliers[i])) {
                postDivIdx = i;
                break;
            }
        }
        if (postDivIdx == 0xff) {
            postDivIdx = 0;
            DFUNC(WARN, "Failed to find a suitable post divider for freq %ld0 KHz\n", freqKhz10);
        }
    }
    D(CHATTY, "chose postDivIdx %ld\n", postDivIdx);

    UWORD M = getConstChipData(bi)->referenceDivider;
    UWORD R = getConstChipData(bi)->referenceFrequency;

    // T = 2 * R * N / (M * P)
    // N = T * M * P / (2 * R)

    UWORD Ntimes2 = (freqKhz10 * M * multipliers[postDivIdx] + R - 1) / R;

    pllValues->N    = Ntimes2 / 2;
    pllValues->Pidx = postDivIdx;

    ULONG outputFreq = computeFrequencyKhz10(R, pllValues->N, M, multipliers[pllValues->Pidx]);

    D(CHATTY, "target: %ld0 KHz, Output: %ld0 KHz, R: %ld0 KHz, M: %ld, P: %ld, N: %ld\n", (ULONG)freqKhz10,
      (ULONG)outputFreq, (ULONG)R, (ULONG)M, (ULONG)multipliers[postDivIdx], (ULONG)pllValues->N);

    return outputFreq;
}

ULONG computeSPllValues(const BoardInfo_t *bi, ULONG freqKhz10, PLLValue_t *pllValues)
{
    return computePLLValues(bi, freqKhz10, g_SPllMultiplier, ARRAY_SIZE(g_SPllMultiplier), pllValues);
}

static void setGUIEngineClockToSecondaryPLL(BoardInfo_t *bi, UWORD freqKhz10)
{
    DFUNC(VERBOSE, "Setting GUI Engine Clock to %ld0 KHz\n", freqKhz10);

    if (freqKhz10 < 0x1475) {
        freqKhz10 = 0x1475;
    }
    if (freqKhz10 > 8300) {
        freqKhz10 = 8300;
    }
    PLLValue_t pllValues;
    if (!computeSPllValues(bi, freqKhz10, &pllValues)) {
        DFUNC(ERROR, "Failed to compute SPLL values\n");
        return;
    }

    static const UBYTE selectCPUSrc = 0x50;

    // SCLK to CPUCLK
    WRITE_PLL(PLL_SPLL_CNTL2, selectCPUSrc);
    // Reset + Sleep secondary PLL
    WRITE_PLL(PLL_SPLL_CNTL2, selectCPUSrc | 3);

    // Secondary PLL serves as another clock source for the GUI Engine, in addition to
    // MPLL. Using this PLL allows the GUI clock to run at any frequency (within the
    // operating range) regardless of what frequency the memory clock (XCLK) is running
    // at (default = 53h)
    UBYTE postDivCode = g_SPllMultiplierCode[pllValues.Pidx];
    postDivCode <<= 4;

    WRITE_PLL(PLL_SCLK_FB_DIV, pllValues.N);
    delayMilliSeconds(5);
    // Take PLL out of sleep and Reset, write post divider, select SPLL as clock source
    WRITE_PLL(PLL_SPLL_CNTL2, postDivCode);
}

void initSDRAM_PLL(BoardInfo_t *bi)
{
    // EXT_MEM_CNTL

    // Changes in settings to this register will not take affect until MEM_SDRAM_RESET
    // is pulsed (from 0 –>1).
    // The sequence should be as follows:
    // 1. Write EXT_MEM_CNTL with the desired settings, setting
    // MEM_SDRAM_RESET = 0 (use read/modify/write).
    // 2. Rewrite EXT_MEM_CNTL, setting MEM_SDRAM_RESET = 1.
    // 3. Rewrite EXT_MEM_CNTL, setting MEM_SDRAM_RESET = 0 (clear reset bit).

    // In order to reset SDRAM, the following steps must be performed:
    //  1. Set MEM_SDRAM_RESET to ‘1’
    //  2. Set MEM_CYC_TEST to ‘10’
    //  3. Set MEM_CYC_TEST to ‘11’. Wait at least 3ns.
    //  4. Set MEM_CYC_TEST to ‘00’
    //  5. Set MEM_SDRAM_RESET to ‘0’

    DFUNC(VERBOSE, "1\n");

    REGBASE();

    // DLL_PWDN = 0 , power UP DLL
    WRITE_PLL_MASK(PLL_GEN_CNTL, 0x80, 0);
    // FIXME: 0x06 is not really legal for a GT?!
    // DLL_REF_SRC = !YCLK; DLL_FB_CLK set to XCLK; ; DLL_RESET = 0; HCLK output enabled
    // DLL resets on rising edge of this signal if DLL_REF_CLK is running
    WRITE_PLL(PLL_DLL_CNTL, 0xa6);
    // DLL_RESET = 1
    WRITE_PLL(PLL_DLL_CNTL, 0xe6);

    delayMilliSeconds(5);

    WRITE_PLL(PLL_DLL_CNTL, 0xa6);

    // Take Memory Controller out of Reset
    W_BLKIO_MASK_L(GEN_TEST_CNTL, GEN_SOFT_RESET_MASK, 0);

    ULONG ext_mem_cntl = R_BLKIO_L(EXT_MEM_CNTL) & 0xfffffff1;
    // Start reset, test cycle 0
    W_BLKIO_L(EXT_MEM_CNTL, ext_mem_cntl);
    // Start reset, initiate test cycle
    W_BLKIO_L(EXT_MEM_CNTL, ext_mem_cntl | MEM_CYC_TEST(0b10) | MEM_SDRAM_RESET);
    // Test mode sequence run
    W_BLKIO_L(EXT_MEM_CNTL, ext_mem_cntl | MEM_CYC_TEST(0b11) | MEM_SDRAM_RESET);
    delayMilliSeconds(5);
    // Take out of reset and test cycle
    W_BLKIO_L(EXT_MEM_CNTL, ext_mem_cntl);

    // 111 : XCLK = DLL_CLK; doesn't work :-(
    // WRITE_PLL_MASK(PLL_EXT_CNTL, XCLK_SRC_SEL_MASK, XCLK_SRC_SEL(0b111));

    DFUNC(VERBOSE, "4\n");
}

void setMemoryClock(BoardInfo_t *bi, UWORD freqKhz10)
{
    DFUNC(VERBOSE, "Setting Memory Clock to %ld0 KHz\n", (ULONG)freqKhz10);

    if (freqKhz10 < 0x1475) {
        freqKhz10 = 0x1475;
    }
    // SGRAM is rated up to 100Mhz, Core engine up to 83Mhz
    if (freqKhz10 > 10000) {
        freqKhz10 = 10000;
    }
    PLLValue_t pllValues;
    if (!computePLLValues(bi, freqKhz10, g_MCLKPllMultiplier, ARRAY_SIZE(g_MCLKPllMultiplier), &pllValues)) {
        DFUNC(ERROR, "Failed to compute PLL values\n");
        return;
    }

    BYTE current = READ_PLL(PLL_MCLK_FB_DIV);
    /* Same frequency already set? */
    if (current == pllValues.N) {
        return;
    }
    ResetEngine(bi);

    REGBASE();

    // Reset Memory Controller
    W_BLKIO_MASK_L(GEN_TEST_CNTL, GEN_SOFT_RESET_MASK, GEN_SOFT_RESET);

    WRITE_PLL(PLL_MCLK_FB_DIV, pllValues.N);
    WRITE_PLL_MASK(PLL_GEN_CNTL, MCLK_SRC_SEL_MASK, MCLK_SRC_SEL(g_MCLKMultiplierCode[pllValues.Pidx]));

    delayMilliSeconds(16);

    initSDRAM_PLL(bi);
    return;
}

void initPLL(struct BoardInfo *bi)
{
    DFUNC(VERBOSE, "\n");

    REGBASE();

    WRITE_PLL(PLL_MPLL_CNTL, 0xAD);
    WRITE_PLL(PLL_MACRO_CNTL, 0xD5);
    WRITE_PLL(PLL_VCLK_CNTL, 0x00);
    WRITE_PLL(PLL_VFC_CNTL, 0x1B);
    WRITE_PLL(PLL_REF_DIV, 0x1F);
    WRITE_PLL(PLL_EXT_CNTL, 0x01);
    WRITE_PLL(PLL_SPLL_CNTL2, 0x10);
    setGUIEngineClockToSecondaryPLL(bi, 7500);

    WRITE_PLL(PLL_GEN_CNTL, 0x54);

    setMemoryClock(bi, 10000);

    // This would switch MCLK back to CPU Clock? Maybe thats ok when GUI runs on SCLK as setup above
    WRITE_PLL(PLL_GEN_CNTL, 0x64);
    WRITE_PLL_MASK(PLL_VCLK_CNTL, 0x7, 0x3);
}

static BOOL probeMemorySize(BoardInfo_t *bi)
{
    DFUNC(VERBOSE, "\n");

    REGBASE();
    LOCAL_SYSBASE();

    static const ULONG memorySizes[] = {0x800000, 0x600000, 0x400000, 0x200000, 0x100000};
    static const ULONG memoryCodes[] = {11, 9, 7, 3, 1};
    static const ULONG addrCfg[]     = {(0b10 << 8) | 2, (0b10 << 8) | 2, (0b01 << 8) | 1, (0b01 << 8) | 1,
                                        (0b00 << 8 | 0)};

    volatile ULONG *framebuffer = (volatile ULONG *)bi->MemoryBase;
    framebuffer[0]              = 0;

    for (int i = 0; i < ARRAY_SIZE(memorySizes); i++) {
        bi->MemorySize = memorySizes[i];
        D(VERBOSE, "\nProbing memory size %ld\n", bi->MemorySize);

        W_BLKIO_L(MEM_ADDR_CONFIG, addrCfg[i]);
        W_BLKIO_MASK_L(MEM_CNTL, 0xF, memoryCodes[i]);

        flushWrites();

        CacheClearU();

        // Probe the last and the first longword for the current segment,
        // as well as offset 0 to check for wrap arounds
        volatile ULONG *highOffset = framebuffer + (bi->MemorySize >> 2) - 512 - 1;
        volatile ULONG *lowOffset  = framebuffer + (bi->MemorySize >> 3);
        // Probe  memory
        *framebuffer = 0;
        *highOffset  = (ULONG)highOffset;
        *lowOffset   = (ULONG)lowOffset;

        CacheClearU();

        ULONG readbackHigh = *highOffset;
        ULONG readbackLow  = *lowOffset;
        ULONG readbackZero = *framebuffer;

        D(VERBOSE, "Probing memory at 0x%lx ?= 0x%lx; 0x%lx ?= 0x%lx, 0x0 ?= 0x%lx\n", highOffset, readbackHigh,
          lowOffset, readbackLow, readbackZero);

        if (readbackHigh == (ULONG)highOffset && readbackLow == (ULONG)lowOffset && readbackZero == 0) {
            D(VERBOSE, "Memory size sucessfully probed.\n\n");
            return TRUE;
        }
    }
    D(VERBOSE, "Memory size probe failed.\n\n");
    return FALSE;
}

typedef struct
{
    ULONG reserved2 : 2;
    ULONG mem_page_size : 2;
    ULONG uppwer_aper_endian : 2;
    ULONG lower_aper_endian : 2;
    ULONG reserved : 1;
    ULONG mem_refresh_rate : 3;
    ULONG mem_refresh_dis : 1;
    ULONG mem_tras : 3;
    ULONG mem_oe_pullback : 1;
    ULONG mem_cas_phase : 1;
    ULONG mem_tr2w : 1;
    ULONG mem_tcrd : 1;
    ULONG mem_trcd : 2;
    ULONG mem_trp : 2;
    ULONG mem_latch : 2;
    ULONG mem_latency : 2;
    ULONG mem_size : 4;
} MEM_CNTL_t;

void print_MEM_CNTL(const MEM_CNTL_t mem_cntl)
{
    D(ALWAYS, "MEM_CNTL---------------------\n");
    D(ALWAYS, "MEM_SIZE: %u\n", mem_cntl.mem_size);
    D(ALWAYS, "MEM_LATENCY: %u\n", mem_cntl.mem_latency);
    D(ALWAYS, "MEM_LATCH: %u\n", mem_cntl.mem_latch);
    D(ALWAYS, "MEM_TRP: %u\n", mem_cntl.mem_trp);
    D(ALWAYS, "MEM_TRCD: %u\n", mem_cntl.mem_trcd);
    D(ALWAYS, "MEM_TCRD: %u\n", mem_cntl.mem_tcrd);
    D(ALWAYS, "MEM_TR2W: %u\n", mem_cntl.mem_tr2w);
    D(ALWAYS, "MEM_CAS_PHASE: %u\n", mem_cntl.mem_cas_phase);
    D(ALWAYS, "MEM_OE_PULLBACK: %u\n", mem_cntl.mem_oe_pullback);
    D(ALWAYS, "MEM_TRAS: %u\n", mem_cntl.mem_tras);
    D(ALWAYS, "MEM_REFRESH_DIS: %u\n", mem_cntl.mem_refresh_dis);
    D(ALWAYS, "MEM_REFRESH_RATE: %u\n", mem_cntl.mem_refresh_rate);
    // D(ALWAYS, "reserved: %u\n", mem_cntl.reserved);
    D(ALWAYS, "LOWER_APER_ENDIAN: %u\n", mem_cntl.lower_aper_endian);
    D(ALWAYS, "UPPER_APER_ENDIAN: %u\n", mem_cntl.uppwer_aper_endian);
    D(ALWAYS, "MEM_PAGE_SIZE: %u\n", mem_cntl.mem_page_size);
    D(ALWAYS, "------------------------------\n");
}

typedef struct
{
    ULONG mem_group_fault_en : 1;  // Bit 31: Page fault control
    ULONG mem_all_page_dis : 1;    // Bit 30: All page memory cycles
    ULONG sdram_mem_cfg : 1;       // Bit 29: RAS, CAS, and CS config
    ULONG reserved2 : 1;           // Bit 28
    ULONG mem_gcmrs : 4;           // Bits 24-27: Graphics Controller Mode
    ULONG reserved : 2;            // Bits 22-23
    ULONG mem_ma_delay : 1;        // Bit 21: Delay MA pins
    ULONG mem_ma_drive : 1;        // Bit 20: Drive strength MA pins
    ULONG mem_mdo_delay : 1;       // Bit 19: Delay odd MD pins
    ULONG mem_mde_delay : 1;       // Bit 18: Delay even MD pins
    ULONG mem_mdb_drive : 1;       // Bit 17: Drive strength MD pins
    ULONG mem_mda_drive : 1;       // Bit 16: Drive strength MD pins
    ULONG mem_tile_boundary : 4;   // Bits 12-15: Memory tile boundary
    ULONG mem_cas_latency : 2;     // Bits 10-11: SGRAM CAS latency
    ULONG mem_clk_select : 2;      // Bits 8-9: HCLK pin Clock selection
    ULONG mem_tile_select : 4;     // Bits 4-7: SDRAM Memory tile boundary
    ULONG mem_cyc_test : 2;        // Bits 2-3: Memory cycle test
    ULONG mem_sdram_reset : 1;     // Bit 1: Reset SDRAM
    ULONG mem_cs : 1;              // Bit 0: SDRAM Command behavior
} EXT_MEM_CNTL_t;

void print_EXT_MEM_CNTL(EXT_MEM_CNTL_t ext_mem_cntl)
{
    D(ALWAYS, "EXT_MEM_CNTL-------------------\n");
    D(ALWAYS, "MEM_CS: %lx\n", ext_mem_cntl.mem_cs);
    D(ALWAYS, "MEM_SDRAM_RESET: %lx\n", ext_mem_cntl.mem_sdram_reset);
    D(ALWAYS, "MEM_CYC_TEST: %lx\n", ext_mem_cntl.mem_cyc_test);
    D(ALWAYS, "MEM_TILE_SELECT: %lx\n", ext_mem_cntl.mem_tile_select);
    D(ALWAYS, "MEM_CLK_SELECT: %lx\n", ext_mem_cntl.mem_clk_select);
    D(ALWAYS, "MEM_CAS_LATENCY: %lx\n", ext_mem_cntl.mem_cas_latency);
    D(ALWAYS, "MEM_TILE_BOUNDARY: %lx\n", ext_mem_cntl.mem_tile_boundary);
    D(ALWAYS, "MEM_MDA_DRIVE: %lx\n", ext_mem_cntl.mem_mda_drive);
    D(ALWAYS, "MEM_MDB_DRIVE: %lx\n", ext_mem_cntl.mem_mdb_drive);
    D(ALWAYS, "MEM_MDE_DELAY: %lx\n", ext_mem_cntl.mem_mde_delay);
    D(ALWAYS, "MEM_MDO_DELAY: %lx\n", ext_mem_cntl.mem_mdo_delay);
    D(ALWAYS, "MEM_MA_DRIVE: %lx\n", ext_mem_cntl.mem_ma_drive);
    D(ALWAYS, "MEM_MA_DELAY: %lx\n", ext_mem_cntl.mem_ma_delay);
    D(ALWAYS, "MEM_GCMRS: %lx\n", ext_mem_cntl.mem_gcmrs);
    D(ALWAYS, "SDRAM_MEM_CFG: %lx\n", ext_mem_cntl.sdram_mem_cfg);
    D(ALWAYS, "MEM_ALL_PAGE_DIS: %lx\n", ext_mem_cntl.mem_all_page_dis);
    D(ALWAYS, "MEM_GROUP_FAULT_EN: %lx\n", ext_mem_cntl.mem_group_fault_en);
    D(ALWAYS, "------------------------------\n");
}

BOOL InitMach64GT(struct BoardInfo *bi)
{
    DFUNC(VERBOSE, "\n");

    REGBASE();

    ULONG cfgStat0 = R_BLKIO_L(CONFIG_STAT0);

    WriteDefaultRegList(bi, defaultRegs_GT, ARRAY_SIZE(defaultRegs_GT));

    // Set to SGRAM
    W_BLKIO_L(CONFIG_STAT0, (cfgStat0 & ~CFG_MEM_TYPE_MASK) | CFG_MEM_TYPE(CFG_MEM_TYPE_SGRAM) | CFG_CLOCK_EN);

    W_BLKIO_MASK_L(EXT_MEM_CNTL, MEM_TILE_SELECT_MASK, MEM_TILE_SELECT(0));
    W_BLKIO_MASK_L(EXT_MEM_CNTL, MEM_ALL_PAGE_DIS_MASK, MEM_ALL_PAGE_DIS);

    initPLL(bi);
    initSDRAM_PLL(bi);
    setMemoryClock(bi, 10000);  // 100Mhz
    if (!probeMemorySize(bi)) {
        return FALSE;
    }

    InitVClockPLLTable(bi, g_VCLKPllMultiplier, ARRAY_SIZE(g_VCLKPllMultiplier));

    ULONG mem_cntl = R_BLKIO_L(MEM_CNTL);
    print_MEM_CNTL(*(MEM_CNTL_t *)&mem_cntl);
    ULONG ext_mem_cntl = R_BLKIO_L(EXT_MEM_CNTL);
    print_EXT_MEM_CNTL(*(EXT_MEM_CNTL_t *)&ext_mem_cntl);

    return TRUE;
}
