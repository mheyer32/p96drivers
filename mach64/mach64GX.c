#include "mach64GX.h"
#include "chip_mach64.h"
#include "mach64_common.h"

#include <hardware/custom.h>
#include <hardware/intbits.h>

#define CONFIG_STAT1 (0x3A)

#define CRTC_FIFO_OVERFILL(x)   ((x) << 14)
#define CRTC_FIFO_OVERFILL_MASK (0x3 << 14)
#define CRTC_FIFO_LWM(x)        ((x) << 16)
#define CRTC_FIFO_LWM_MASK      (0xF << 16)
#define CRTC_DISPREQ_ONLY       BIT(21)
#define CRTC_DISPREQ_ONLY_MASK  BIT(21)

// DAC_REGS sub-registers (same as VGA I/O ports 0x3c8/0x3c7/0x3c9)
#define DAC_W_INDEX 0
#define DAC_R_INDEX 3
#define DAC_W_DATA  1
#define DAC_R_DATA  2
#define DAC_MASK    2

// DAC_CNTL register layout (based on NOBIOS code):
// Byte 0 (DAC_CNTL1): RS2/RS3 control bits
// Byte 2 (DAC_TYPE): DAC type (bits 2:0)
// Bits 2:0 indicate DAC type: 0 = external DAC, 2 = integrated DAC
// For external DAC, we need to enable RS2 for direct register access
#define DAC_EXT_SEL_RS2      BIT(0)  // Enable RS2 for direct register access (external DAC)
#define DAC_EXT_SEL_RS2_MASK BIT(0)
#define DAC_EXT_SEL_RS3      BIT(1)  // Enable RS3 for direct register access
#define DAC_EXT_SEL_RS3_MASK BIT(1)

static const UBYTE g_VPLLPostDivider[] = {1, 2, 4, 8};

static const UBYTE g_VPLLPostDividerCodes[] = {
    // *1,    *2,   *4,   *8,
    0b00, 0b01, 0b10, 0b11};

static const UBYTE g_MPLLPostDividers[] = {1, 2, 4, 8};

static const UBYTE g_MPLLPostDividerCodes[] = {
    // *1,  *2,   *4,   *8
    0b000, 0b001, 0b010, 0b011};

#define VCLK_SRC_SEL(x)   ((x))
#define VCLK_SRC_SEL_MASK (0x3)
#define PLL_PRESET        BIT(2)
#define PLL_PRESET_MASK   BIT(2)
#define VCLK0_POST_MASK   (0x3)
#define VCLK0_POST(x)     (x)
#define DCLK_BY2_EN       BIT(7)
#define DCLK_BY2_EN_MASK  BIT(7)

// ICS2595 PLL bit-bang interface via CLOCK_CNTL register
// CLOCK_CNTL register bits for ICS2595:
#define ICS2595_FS2_BIT     BIT(2)  // Data bit (bit 2)
#define ICS2595_FS3_BIT     BIT(3)  // Clock bit (bit 3)
#define ICS2595_STROBE_BIT  BIT(6)  // Strobe bit (bit 6)
#define ICS2595_FS2_MASK    BIT(2)
#define ICS2595_FS3_MASK    BIT(3)
#define ICS2595_STROBE_MASK BIT(6)

static const UWORD defaultRegs_GX[] = {0x00a2, 0x6007,   // BUS_CNTL upper
                                       0x00a0, 0x20f8,   // BUS_CNTL lower
                                       0x0018, 0x0000,   // CRTC_INT_CNTL lower
                                       0x001c, 0x0200,   // CRTC_GEN_CNTL lower
                                       0x001e, 0x040b,   // CRTC_GEN_CNTL upper
                                       0x00d2, 0x0000,   // GEN_TEST_CNTL upper
                                       0x00e4, 0x0020,   // CONFIG_STAT0 upper
                                       0x00b0, 0x0021,   // MEM_CNTL lower
                                       0x00b2, 0x0801,   // MEM_CNTL upper
                                       0x00d0, 0x0000,   // GEN_TEST_CNTL lower
                                       0x001e, 0x0000,   // CRTC_GEN_CNTL upper
                                       0x0080, 0x0000,   // SCRATCH_REG0 lower
                                       0x0082, 0x0000,   // SCRATCH_REG0 upper
                                       0x0084, 0x0000,   // SCRATCH_REG1 lower
                                       0x0086, 0x0000,   // SCRATCH_REG1 upper
                                       0x00c4, 0x0000,   // DAC_CNTL lower
                                       0x00c6, 0x8000,   // DAC_CNTL upper
                                       0x007a, 0x0000,   // GP_IO lower
                                       0x00d0, 0x0100};  // GEN_TEST_CNTL lower

/**
 * MEM_CNTL Register Structure (GX-specific)
 * Register offset: 0x2C (MMIO)
 *
 * This register controls memory configuration, including memory size,
 * latching behavior, cycle length, and VGA/Mach memory boundary settings.
 */
typedef struct
{
    ULONG reserved2 : 13;        // Bits 19-31: Reserved
    ULONG mem_bndry_en : 1;      // Bit 18: Memory boundary enable (VGA/Mach split)
    ULONG mem_bndry : 2;         // Bits 16-17: VGA/Mach Memory boundary (0=0K, 1=256K, 2=512K, 3=1M)
    ULONG reserved : 5;          // Bits 11-15: Reserved
    ULONG mem_cyc_lnth : 2;      // Bits 9-10: Memory cycle length for non-paged access (0=5 clks, 1=6 clks, 2=7 clks)
    ULONG mem_full_pls : 1;      // Bit 8: Memory fill pulse width (1 memory clock period)
    ULONG mem_sd_latch_dly : 1;  // Bit 7: Serial port data latch delay (1/2 memory clock period)
    ULONG mem_sd_latch_en : 1;   // Bit 6: Serial port data latch enable
    ULONG mem_rd_latch_dly : 1;  // Bit 5: RAM port data latch delay (1/2 memory clock period)
    ULONG mem_rd_latch_en : 1;   // Bit 4: RAM port data latch enable
    ULONG reserved_bit3 : 1;     // Bit 3: Reserved
    ULONG mem_size : 3;          // Bits 0-2: Video Memory Size (0=512K, 1=1MB, 2=2MB, 3=4MB, 4=6MB, 5=8MB)
} MEM_CNTL_t;

static void ProgramICS2595(BoardInfo_t *bi, UBYTE entry, ULONG freqKhz10);

// ATI 68860 RAMDAC programming structures and functions
typedef struct
{
    UBYTE gmode;   // Graphics mode register value (REG0B)
    UBYTE dsetup;  // Device setup register value (REG0C base)
} A68860_DAC_Table;

// ATI 68860 mode table (from NOBIOS DAC05.C)
static const A68860_DAC_Table A860_Tab[] = {
    {0x01, 0x63},  // Index 0: 4bpp (not typically used)
    {0x82, 0x61},  // Index 1: 8bpp (RGBFB_CLUT)
    {0x83, 0x61},  // Index 2: 15bpp (RGBFB_R5G5B5)
    {0xA0, 0x60},  // Index 3: 16bpp (RGBFB_R5G6B5)
    {0xA1, 0x60},  // Index 4: 16bpp variant
    {0xC0, 0x60},  // Index 5: 24bpp (RGBFB_R8G8B8)
    {0xE2, 0x60},  // Index 6: 32bpp (RGBFB_A8R8G8B8)
    {0x80, 0x61}   // Index 7: VGA mode
};

#define A860_DELAY_L 0  // Bit 7 mask (delay control)

/**
 * Set RS2/RS3 control bits in DAC_CNTL register
 * @param bi BoardInfo structure
 * @param dacRS2RS3 Bit mask: DAC_EXT_SEL_RS2 (bit 0) and/or DAC_EXT_SEL_RS3 (bit 1)
 */
static void SetRS2RS3(BoardInfo_t *bi, UBYTE dacRS2RS3)
{
    MMIOBASE();
    UBYTE current = R_MMIO_B(DAC_CNTL, 0);
    current       = (current & ~(DAC_EXT_SEL_RS2_MASK | DAC_EXT_SEL_RS3_MASK)) | dacRS2RS3;
    W_MMIO_B(DAC_CNTL, 0, current);
}

/**
 * Map RGBFTYPE to NOBIOS color depth index for ATI 68860
 * @param format RGBFTYPE format
 * @return Color depth index (0-7) or 1 (8bpp default) if unknown
 */
static UBYTE RGBFTYPE_to_colorDepth(RGBFTYPE format)
{
    switch (format) {
    case RGBFB_CLUT:  // 8bpp palette
        return 1;
    case RGBFB_R5G5B5:    // 15bpp
    case RGBFB_R5G5B5PC:  // 15bpp packed
    case RGBFB_B5G5R5PC:  // 15bpp packed
        return 2;
    case RGBFB_R5G6B5:    // 16bpp
    case RGBFB_R5G6B5PC:  // 16bpp packed
    case RGBFB_B5G6R5PC:  // 16bpp packed
        return 3;
    case RGBFB_R8G8B8:  // 24bpp
    case RGBFB_B8G8R8:  // 24bpp
        return 5;
    case RGBFB_A8R8G8B8:  // 32bpp
    case RGBFB_A8B8G8R8:  // 32bpp
    case RGBFB_R8G8B8A8:  // 32bpp
    case RGBFB_B8G8R8A8:  // 32bpp
        return 6;
    default:
        // Default to 8bpp for unknown formats
        return 1;
    }
}

/**
 * CONFIG_STAT0 Register Structure (GX-specific)
 * Register offset: 0x39 (MMIO)
 *
 * This register contains configuration and status information about the chip,
 * including bus type, memory type, DAC type, ROM configuration, and various
 * enable/disable flags.
 */
typedef struct
{
    ULONG cfg_ap_4gbyte_dis : 1;       // Bit 31: Disables 4GB Aperture Addressing if set
    ULONG cfg_vlb_rdy_dis : 1;         // Bit 30: Disables VESA local bus compliant RDY if set
    ULONG cfg_local_dac_wr_en : 1;     // Bit 29: Enables local bus DAC writes if set
    ULONG cfg_bus_option : 1;          // Bit 28: Bus option (EISA: POS regs enable/disable, VLB: I/O 102h decode)
    ULONG cfg_rom_addr : 1;            // Bit 27: ROM Address (0=E0000h, 1=C0000h)
    ULONG cfg_local_read_dly_dis : 1;  // Bit 26: Local read delay disable (0=delay by 1 bus clock, 1=no delay)
    ULONG cfg_chip_en : 1;             // Bit 25: Enables chip if set
    ULONG cfg_local_bus_cfg : 1;       // Bit 24: Local Bus configuration (0=config 2, 1=config 1)
    ULONG cfg_vga_en : 1;              // Bit 23: Enables VGA Controller
    ULONG cfg_rom_dis : 1;             // Bit 22: Disables ROM if set
    ULONG cfg_ext_rom_addr : 6;        // Bits 16-21: Extended Mode ROM Base Address (bits 12-17 of ROM base)
    ULONG cfg_tri_buf_dis : 1;         // Bit 15: Tri-stating of output buffers during reset disabled
    ULONG cfg_init_card_id : 3;        // Bits 12-14: Card ID (0-6: Card ID 0-6, 7: Disable Card ID)
    ULONG cfg_init_dac_type : 3;       // Bits 9-11: Initial DAC type (2=ATI68875/TI34075, 3=Bt476/Bt478, 4=Bt481,
                                       // 5=ATI68860/68880, 6=STG1700, 7=SC15021)
    ULONG cfg_local_bus_option : 2;    // Bits 7-8: Local Bus Option (1=opt1, 2=opt2, 3=opt3)
    ULONG cfg_dual_cas_en : 1;         // Bit 6: Dual CAS support enabled
    ULONG cfg_mem_type : 3;  // Bits 3-5: Memory Type (0=DRAM, 1=VRAM, 2=VRAM short, 3=DRAM16, 4=GDRAM, 5=Enh VRAM,
                             // 6=Enh VRAM short)
    ULONG cfg_bus_type : 3;  // Bits 0-2: Host Bus type (0=ISA, 1=EISA, 6=VLB, 7=PCI)
} CONFIG_STAT0_t;

static void print_CONFIG_STAT0(const CONFIG_STAT0_t *reg)
{
    D(0, "CONFIG_STAT0_Register:\n");
    D(0, "  cfg_bus_type            : 0x%lx", reg->cfg_bus_type);
    switch (reg->cfg_bus_type) {
    case 0:
        D(0, " (ISA)\n");
        break;
    case 1:
        D(0, " (EISA)\n");
        break;
    case 6:
        D(0, " (VLB)\n");
        break;
    case 7:
        D(0, " (PCI)\n");
        break;
    default:
        D(0, " (Unknown)\n");
        break;
    }

    D(0, "  cfg_mem_type            : 0x%lx", reg->cfg_mem_type);
    switch (reg->cfg_mem_type) {
    case 0:
        D(0, " (DRAM 256Kx4)\n");
        break;
    case 1:
        D(0, " (VRAM 256Kx4/x8/x16)\n");
        break;
    case 2:
        D(0, " (VRAM 256Kx16 short shift reg)\n");
        break;
    case 3:
        D(0, " (DRAM 256Kx16)\n");
        break;
    case 4:
        D(0, " (Graphics DRAM 256Kx16)\n");
        break;
    case 5:
        D(0, " (Enhanced VRAM 256Kx4/x8/x16)\n");
        break;
    case 6:
        D(0, " (Enhanced VRAM 256Kx16 short shift reg)\n");
        break;
    default:
        D(0, " (Unknown)\n");
        break;
    }

    D(0, "  cfg_dual_cas_en         : 0x%lx %s\n", reg->cfg_dual_cas_en,
      reg->cfg_dual_cas_en ? "(Enabled)" : "(Disabled)");

    D(0, "  cfg_local_bus_option    : 0x%lx", reg->cfg_local_bus_option);
    switch (reg->cfg_local_bus_option) {
    case 1:
        D(0, " (Local option 1)\n");
        break;
    case 2:
        D(0, " (Local option 2)\n");
        break;
    case 3:
        D(0, " (Local option 3)\n");
        break;
    default:
        D(0, " (Invalid/None)\n");
        break;
    }

    D(0, "  cfg_init_dac_type       : 0x%lx", reg->cfg_init_dac_type);
    switch (reg->cfg_init_dac_type) {
    case 2:
        D(0, " (ATI68875/TI34075)\n");
        break;
    case 3:
        D(0, " (Bt476/Bt478)\n");
        break;
    case 4:
        D(0, " (Bt481)\n");
        break;
    case 5:
        D(0, " (ATI68860/ATI68880)\n");
        break;
    case 6:
        D(0, " (STG1700)\n");
        break;
    case 7:
        D(0, " (SC15021)\n");
        break;
    default:
        D(0, " (Unknown/None)\n");
        break;
    }

    D(0, "  cfg_init_card_id        : 0x%lx", reg->cfg_init_card_id);
    if (reg->cfg_init_card_id == 7) {
        D(0, " (Card ID Disabled)\n");
    } else {
        D(0, " (Card ID %ld)\n", reg->cfg_init_card_id);
    }

    D(0, "  cfg_tri_buf_dis         : 0x%lx %s\n", reg->cfg_tri_buf_dis,
      reg->cfg_tri_buf_dis ? "(Disabled)" : "(Enabled)");
    D(0, "  cfg_ext_rom_addr        : 0x%lx (ROM Base: 0x%05lx000)\n", reg->cfg_ext_rom_addr,
      0xC000 + reg->cfg_ext_rom_addr);
    D(0, "  cfg_rom_dis             : 0x%lx %s\n", reg->cfg_rom_dis, reg->cfg_rom_dis ? "(Disabled)" : "(Enabled)");
    D(0, "  cfg_vga_en              : 0x%lx %s\n", reg->cfg_vga_en, reg->cfg_vga_en ? "(Enabled)" : "(Disabled)");
    D(0, "  cfg_local_bus_cfg       : 0x%lx %s\n", reg->cfg_local_bus_cfg,
      reg->cfg_local_bus_cfg ? "(Config 1)" : "(Config 2)");
    D(0, "  cfg_chip_en             : 0x%lx %s\n", reg->cfg_chip_en, reg->cfg_chip_en ? "(Enabled)" : "(Disabled)");
    D(0, "  cfg_local_read_dly_dis  : 0x%lx %s\n", reg->cfg_local_read_dly_dis,
      reg->cfg_local_read_dly_dis ? "(No delay)" : "(1 bus clock delay)");
    D(0, "  cfg_rom_option          : 0x%lx %s\n", reg->cfg_rom_addr, reg->cfg_rom_addr ? "(C0000h)" : "(E0000h)");
    D(0, "  cfg_bus_option          : 0x%lx\n", reg->cfg_bus_option);
    D(0, "  cfg_local_dac_wr_en     : 0x%lx %s\n", reg->cfg_local_dac_wr_en,
      reg->cfg_local_dac_wr_en ? "(Enabled)" : "(Disabled)");
    D(0, "  cfg_vlb_rdy_dis         : 0x%lx %s\n", reg->cfg_vlb_rdy_dis,
      reg->cfg_vlb_rdy_dis ? "(Disabled)" : "(Enabled)");
    D(0, "  cfg_ap_4gbyte_dis       : 0x%lx %s\n", reg->cfg_ap_4gbyte_dis,
      reg->cfg_ap_4gbyte_dis ? "(Disabled)" : "(Enabled)");
}

/**
 * CONFIG_STAT1 Register Structure (GX-specific)
 * Register offset: 0x3A (MMIO)
 *
 * This register contains additional configuration information about the chip.
 * Only bit 0 is defined; bits 1-31 are reserved.
 */
typedef struct
{
    ULONG reserved : 31;        // Bits 1-31: Reserved
    ULONG cfg_pci_dac_cfg : 1;  // Bit 0: PCI DAC Configuration (0=direct connection, 1=through latch)
} CONFIG_STAT1_t;

static void print_CONFIG_STAT1(const CONFIG_STAT1_t *reg)
{
    D(0, "CONFIG_STAT1_Register:\n");
    D(0, "  reserved                : 0x%lx\n", reg->reserved);
    D(0, "  cfg_pci_dac_cfg         : 0x%lx %s\n", reg->cfg_pci_dac_cfg,
      reg->cfg_pci_dac_cfg ? "(Through latch)" : "(Direct connection)");
}

static void print_MEM_CNTL(const MEM_CNTL_t *reg)
{
    D(0, "MEM_CNTL_Register:\n");
    D(0, "  mem_size         : 0x%lx", reg->mem_size);
    switch (reg->mem_size) {
    case 0:
        D(0, " (512K)\n");
        break;
    case 1:
        D(0, " (1MB)\n");
        break;
    case 2:
        D(0, " (2MB)\n");
        break;
    case 3:
        D(0, " (4MB)\n");
        break;
    case 4:
        D(0, " (6MB)\n");
        break;
    case 5:
        D(0, " (8MB)\n");
        break;
    default:
        D(0, " (Unknown)\n");
        break;
    }
    D(0, "  mem_rd_latch_en  : 0x%lx %s\n", reg->mem_rd_latch_en, reg->mem_rd_latch_en ? "(Enabled)" : "(Disabled)");
    D(0, "  mem_rd_latch_dly  : 0x%lx %s\n", reg->mem_rd_latch_dly, reg->mem_rd_latch_dly ? "(Delayed)" : "(No delay)");
    D(0, "  mem_sd_latch_en  : 0x%lx %s\n", reg->mem_sd_latch_en, reg->mem_sd_latch_en ? "(Enabled)" : "(Disabled)");
    D(0, "  mem_sd_latch_dly  : 0x%lx %s\n", reg->mem_sd_latch_dly, reg->mem_sd_latch_dly ? "(Delayed)" : "(No delay)");
    D(0, "  mem_fill_pls      : 0x%lx %s\n", reg->mem_full_pls, reg->mem_full_pls ? "(1 mem clock)" : "(Disabled)");
    D(0, "  mem_cyc_lnth      : 0x%lx", reg->mem_cyc_lnth);
    switch (reg->mem_cyc_lnth) {
    case 0:
        D(0, " (5 mem clock periods)\n");
        break;
    case 1:
        D(0, " (6 mem clock periods)\n");
        break;
    case 2:
        D(0, " (7 mem clock periods)\n");
        break;
    default:
        D(0, " (Unknown)\n");
        break;
    }
    D(0, "  mem_bndry         : 0x%lx", reg->mem_bndry);
    switch (reg->mem_bndry) {
    case 0:
        D(0, " (0K)\n");
        break;
    case 1:
        D(0, " (256K)\n");
        break;
    case 2:
        D(0, " (512K)\n");
        break;
    case 3:
        D(0, " (1MB)\n");
        break;
    default:
        D(0, " (Unknown)\n");
        break;
    }
    D(0, "  mem_bndry_en      : 0x%lx %s\n", reg->mem_bndry_en,
      reg->mem_bndry_en ? "(Enabled - VGA/Mach split)" : "(Disabled - Shared)");
}

static BOOL probeMemorySize(BoardInfo_t *bi)
{
    DFUNC(VERBOSE, "\n");

    MMIOBASE();
    LOCAL_SYSBASE();

    static const ULONG memorySizes[] = {0x400000, 0x200000, 0x100000, 0x80000};
    static const ULONG memoryCodes[] = {3, 2, 1, 0};

    volatile ULONG *framebuffer = (volatile ULONG *)bi->MemoryBase;
    framebuffer[0]              = 0;

    for (int i = 0; i < ARRAY_SIZE(memorySizes); i++) {
        bi->MemorySize = memorySizes[i];
        D(VERBOSE, "\nProbing memory size %ld\n", bi->MemorySize);

        W_MMIO_MASK_L(MEM_CNTL, 0x7, memoryCodes[i]);

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

// CLOCK_CNTL register bits for clock selection (bits 0-3 select ICS2595 entry)
#define CLOCK_SEL_MASK_GX (0x0F)  // Bits 0-3: Clock select (0-15)
#define CLOCK_SEL_GX(x)   ((x) & 0x0F)

static void setMemoryClock(BoardInfo_t *bi, UWORD freqKhz10)
{
    DFUNC(VERBOSE, "Setting Memory Clock to %ld0 KHz\n", (ULONG)freqKhz10);

    if (freqKhz10 < 1475) {
        freqKhz10 = 1475;
    }
    // SGRAM is rated up to 100Mhz, Core engine up to 83Mhz
    if (freqKhz10 > 6800) {
        freqKhz10 = 6800;
    }

    MMIOBASE();

    // Reset GUI Controller
    W_MMIO_MASK_L(GEN_TEST_CNTL, GEN_GUI_RESETB_MASK, GEN_GUI_RESETB);
    W_MMIO_MASK_L(GEN_TEST_CNTL, GEN_GUI_RESETB_MASK, 0);

    W_MMIO_MASK_L(CRTC_GEN_CNTL, CRTC_EXT_DISP_EN_MASK, CRTC_EXT_DISP_EN);

    // Apparently MCLK Select (MS0, MS1) on the ATI card is wired to 1,1, corresponding to MCLK Address 3, 0b10011
    ProgramICS2595(bi, 0b10011, freqKhz10);

    delayMilliSeconds(5);

    DFUNC(VERBOSE, "MCLK0 set to %ld0 KHz\n", (ULONG)freqKhz10);

    return;
}

/**
 * ICS2595 PLL Bit-Bang Functions
 *
 * The ICS2595 is an external clock generator PLL that is programmed via
 * a serial bit-bang protocol through the mach64gx's CLOCK_CNTL register.
 *
 * Protocol:
 * - FS2 (bit 2): Data bit
 * - FS3 (bit 3): Clock bit
 * - STROBE (bit 6): Strobe bit
 *
 * Sequence:
 * 1. Initialize: Write 0, strobe, write 1, strobe
 * 2. Start bits: 1, 0, 0
 * 3. 5 bits: Entry (register address)
 * 4. 13 bits: Program word (8 bits N, 1 bit, 2 bits divider, 2 bits stop)
 */

/**
 * Set FS2 (data) and/or FS3 (clock) bits in CLOCK_CNTL register
 * @param bi BoardInfo structure
 * @param bits OR combination of ICS2595_FS2_BIT and/or ICS2595_FS3_BIT
 *             Example: ICS2595_FS2_BIT sets FS2=1, FS3=0
 *                      ICS2595_FS3_BIT sets FS2=0, FS3=1
 *                      ICS2595_FS2_BIT | ICS2595_FS3_BIT sets both to 1
 */
static void ics2595_setFSBits(BoardInfo_t *bi, ULONG bits)
{
    MMIOBASE();
    ULONG mask = ICS2595_FS2_MASK | ICS2595_FS3_MASK;
    W_MMIO_MASK_L(CLOCK_CNTL, mask, bits);
}

/**
 * Generate strobe pulse on CLOCK_CNTL register
 * @param bi BoardInfo structure
 *
 * Note: The strobe bit is auto-cleared by hardware after being set.
 * We only need to set it; the hardware handles the pulse generation.
 */
static void ics2595_strobe(BoardInfo_t *bi)
{
    MMIOBASE();
    delayMicroSeconds(26);  // 26us delay as per NOBIOS code

    // Set strobe bit - hardware will auto-clear it
    W_MMIO_MASK_L(CLOCK_CNTL, ICS2595_STROBE_MASK, ICS2595_STROBE_BIT);
}

/**
 * Send a single bit to ICS2595 PLL
 * @param bi BoardInfo structure
 * @param data Bit value to send (0 or 1)
 */
static void ics2595_sendBit(BoardInfo_t *bi, UBYTE data)
{
    data <<= 2;  // Align data to FS2 bit position
    // Set FS2 (data bit) and FS3 (clock bit = 0)
    ics2595_setFSBits(bi, data);
    ics2595_strobe(bi);
    // Set FS3 (clock bit = 1)
    ics2595_setFSBits(bi,ICS2595_FS3_BIT | data);
    ics2595_strobe(bi);
}

/**
 * Calculate ICS2595 program word from frequency
 * @param freqKhz10 Frequency in 0.1 kHz units (e.g., 25000 = 2.5 MHz)
 * @param refFreqKhz10 Reference frequency in 0.1 kHz units
 * @param refDivider Reference divider value
 * @return Program word for ICS2595
 */
static UWORD ics2595_calculateProgramWord(const BoardInfo_t *bi, UWORD freqKhz10)
{
    const ChipSpecific_t *cs = getConstChipSpecific(bi);
    ULONG minFreq            = cs->minPClock;  // 1.0 MHz minimum (10000 = 1.0 MHz in 0.1 kHz units)
    ULONG maxFreq            = cs->maxPClock;  // 20.0 MHz maximum (200000 = 20.0 MHz in 0.1 kHz units)
    ULONG divider            = 3;              // Start with divider 3 (divide by 1)
    ULONG programWord;
    ULONG temp;
    ULONG adjustedFreq = freqKhz10;
    UWORD n_adj        = 257;  // N adjustment value (calibration offset)

    // Clamp frequency to valid range
    if (adjustedFreq < minFreq) {
        adjustedFreq = minFreq;
    }
    if (adjustedFreq > maxFreq) {
        adjustedFreq = maxFreq;
    }

    // Find appropriate divider (divide by 1, 2, 4, or 8)
    // divider: 3=divide by 1, 2=divide by 2, 1=divide by 4, 0=divide by 8
    // We need to scale up the frequency until it's >= (minFreq << 3) = 8 * minFreq
    // This ensures the N value will be in the valid range (0x80-0xFF preferred)
    while (adjustedFreq < (minFreq << 3) && divider > 0) {
        adjustedFreq <<= 1;  // Double the frequency
        divider--;           // Reduce divider (which effectively divides by more)
    }

    // Calculate N value: N = (freq * refDivider + refFreq/2) / refFreq
    // The + refFreq/2 is for rounding to nearest integer
    temp = (ULONG)adjustedFreq * cs->referenceDivider;
    temp += (cs->referenceDivider >> 1);  // Add half for rounding
    temp        = temp / cs->referenceFrequency;
    programWord = (UWORD)temp;

    DFUNC(CHATTY, "Freq: %ld, AdjFreq: %ld, Divider: %ld, N: %ld\n", (ULONG)freqKhz10, (ULONG)adjustedFreq,
          (ULONG)divider, (ULONG)programWord)

    if (programWord > n_adj) {
        programWord -= n_adj;
    } else {
        programWord = 0;
    }

    // Clamp N to 8-bit range (0-255)
    // Note: If N is 0, the PLL may not work correctly. Consider adjusting
    // the divider or reference frequency if this occurs.
    if (programWord > 0xFF) {
        programWord = 0xFF;
    }

    // Validate that we have a reasonable N value
    // If N is too small (< 0x80), we might want to use a smaller divider
    // to get N into the preferred range, but this is optional

    // Assemble the complete program word:
    // Bits 0-7:   N value (8 bits)
    // Bit 8:      Unused (0)
    // Bits 9-10:  Divider (2 bits: 0=div8, 1=div4, 2=div2, 3=div1)
    // Bits 11-12: Stop bits (2 bits: should be 0b11 = 3)
    programWord &= 0xFF;            // Ensure N is only 8 bits
    programWord |= (divider << 9);  // Divider in bits 9-10
    programWord |= 0x1800;          // Stop bits (bits 11-12 = 0b11)

    return (UWORD)programWord;
}

/**
 * Program ICS2595 PLL with specified entry and frequency
 * @param bi BoardInfo structure
 * @param entry Entry/register address (5 bits, 0-31)
 * @param freqKhz10 Frequency in 0.1 kHz units
 */
static void ProgramICS2595(BoardInfo_t *bi, UBYTE entry, ULONG freqKhz10)
{
    DFUNC(VERBOSE, "Programming ICS2595: entry=%ld, freq=%ld0 KHz\n", (ULONG)entry, freqKhz10);

    MMIOBASE();

    const ChipSpecific_t *cs = getConstChipSpecific(bi);
    ULONG refFreqKhz10       = cs->referenceFrequency;
    UBYTE refDivider         = cs->referenceDivider;

    UWORD programWord = ics2595_calculateProgramWord(bi, freqKhz10);

    // the ICS programming sequence needs to be timely.
    // Any pauses (e.g., due to interrupts) may cause the chip to exit the programming sequence
    LOCAL_SYSBASE();
    Disable();

    // Initialize: Write 0, strobe, write 1, strobe
    W_MMIO_L(CLOCK_CNTL, 0);
    ics2595_strobe(bi);
    W_MMIO_L(CLOCK_CNTL, 1);
    ics2595_strobe(bi);

    // Send start bits: 1, 0, 0
    ics2595_sendBit(bi, 1);
    ics2595_sendBit(bi, 0);
    ics2595_sendBit(bi, 0);

    // Send 5 bits for entry (register address), LSB first
    for (int i = 0; i < 5; i++) {
        ics2595_sendBit(bi, entry & 1);
        entry >>= 1;
    }

    // Send 13 bits for program word, LSB first
    // Format: 8 bits N, 1 bit (unused), 2 bits divider, 2 bits stop
    for (int i = 0; i < 13; i++) {
        ics2595_sendBit(bi, programWord & 1);
        programWord >>= 1;
    }

    Enable();

    W_MMIO_L(CLOCK_CNTL, 0);

    DFUNC(VERBOSE, "ICS2595 programming complete\n");
}

/**
 * SetDAC function for Mach64 GX with ATI 68860 RAMDAC
 * Programs the external ATI 68860 RAMDAC based on RGBFTYPE format and memory size
 * @param bi BoardInfo structure
 * @param format RGBFTYPE format
 */
static void ASM SetDAC_GX(__REGA0(struct BoardInfo *bi), __REGD0(UWORD region), __REGD7(RGBFTYPE format))
{
    MMIOBASE();

    DFUNC(VERBOSE, "SetDAC_GX: format %ld\n", (ULONG)format);

    // Map RGBFTYPE to color depth index
    UBYTE colorDepth                    = RGBFTYPE_to_colorDepth(format);
    const A68860_DAC_Table *pDacProgTab = &A860_Tab[colorDepth];

    // Calculate memory size mask for REG0C bits 2-3
    UBYTE mask;
    if (bi->MemorySize < 0x100000) {          // < 1MB
        mask = 4;                             // Bit 2
    } else if (bi->MemorySize == 0x100000) {  // = 1MB
        mask = 8;                             // Bit 3
    } else {                                  // > 1MB
        mask = 0x0c;                          // Bits 2+3
    }

    // Program ATI 68860 RAMDAC following NOBIOS sequence:
    // 1. Enable RS3 for extended register access
    SetRS2RS3(bi, DAC_EXT_SEL_RS3);

    // 2. Write REG0A = 0x1d to DAC_MASK (register 0x3c6)
    W_MMIO_B(DAC_REGS, DAC_MASK, 0x1d);

    // 3. Write REG0B (Graphics Mode Register) = gmode to DAC_R_INDEX (register 0x3c7)
    W_MMIO_B(DAC_REGS, DAC_R_INDEX, pDacProgTab->gmode);

    // 4. Select REG0C for writing: Write 0x02 to DAC_W_INDEX (register 0x3c8)
    W_MMIO_B(DAC_REGS, DAC_W_INDEX, 0x02);

    // 5. Enable both RS2 and RS3 for extended register access
    SetRS2RS3(bi, DAC_EXT_SEL_RS2 | DAC_EXT_SEL_RS3);

    // 6. Write REG0C (Device Setup Register A):
    //    - Base value from dsetup
    //    - Memory size mask in bits 2-3
    //    - Preserve bit 7 (delay control) from current value
    UBYTE d = pDacProgTab->dsetup;
    // Note: PITCH_INFO_DAC handling would clear bit 0 here if needed
    // For now, we use the base dsetup value
    UBYTE currentReg0C = R_MMIO_B(DAC_REGS, DAC_W_INDEX);
    UBYTE reg0CValue   = (d | mask) | (currentReg0C & A860_DELAY_L);
    W_MMIO_B(DAC_REGS, DAC_W_INDEX, reg0CValue);

    // 7. Disable RS2/RS3
    SetRS2RS3(bi, 0);

    // Set CRTC pixel width (like standard SetDAC)
    // Map RGBFTYPE to COLOR_DEPTH for CRTC_PIX_WIDTH
    static const UBYTE bitWidths[] = {
        COLOR_DEPTH_4,   // RGBFB_NONE 4bit
        COLOR_DEPTH_8,   // RGBFB_CLUT
        COLOR_DEPTH_24,  // RGBFB_R8G8B8
        COLOR_DEPTH_24,  // RGBFB_B8G8R8
        COLOR_DEPTH_16,  // RGBFB_R5G6B5PC
        COLOR_DEPTH_15,  // RGBFB_R5G5B5PC
        COLOR_DEPTH_32,  // RGBFB_A8R8G8B8
        COLOR_DEPTH_32,  // RGBFB_A8B8G8R8
        COLOR_DEPTH_32,  // RGBFB_R8G8B8A8
        COLOR_DEPTH_32,  // RGBFB_B8G8R8A8
        COLOR_DEPTH_16,  // RGBFB_R5G6B5
        COLOR_DEPTH_15,  // RGBFB_R5G5B5
        COLOR_DEPTH_16,  // RGBFB_B5G6R5PC
        COLOR_DEPTH_15,  // RGBFB_B5G5R5PC
        COLOR_DEPTH_1,   // RGBFB_YUV422CGX
        COLOR_DEPTH_1,   // RGBFB_YUV411
        COLOR_DEPTH_1,   // RGBFB_YUV411PC
        COLOR_DEPTH_1,   // RGBFB_YUV422
        COLOR_DEPTH_1,   // RGBFB_YUV422PC
        COLOR_DEPTH_1,   // RGBFB_YUV422PA
        COLOR_DEPTH_1,   // RGBFB_YUV422PAPC
    };
    if (format < ARRAY_SIZE(bitWidths)) {
        W_MMIO_MASK_L(CRTC_GEN_CNTL, CRTC_PIX_WIDTH_MASK, CRTC_PIX_WIDTH(bitWidths[format]));
    }

    // Set up palette/gamma ramp (like standard SetDAC)
    // Duplicate SetColorArrayInternal logic since it's static in chip_mach64.c
    const UBYTE bppDiff = 0;  // 8 - bi->BitsPerCannon;
    W_MMIO_B(DAC_REGS, DAC_W_INDEX, 0);
    if (format != RGBFB_CLUT) {
        // In Hi-Color modes, the palette acts as gamma ramp
        for (UWORD c = 0; c < 256; c++) {
            UBYTE gray = (UBYTE)c;
            writeReg(MMIOBase, DWORD_OFFSET(DAC_REGS) + DAC_W_DATA, gray >> bppDiff);
            writeReg(MMIOBase, DWORD_OFFSET(DAC_REGS) + DAC_W_DATA, gray >> bppDiff);
            writeReg(MMIOBase, DWORD_OFFSET(DAC_REGS) + DAC_W_DATA, gray >> bppDiff);
        }
    } else {
        // Use CLUT from BoardInfo
        for (UWORD c = 0; c < 256; c++) {
            writeReg(MMIOBase, DWORD_OFFSET(DAC_REGS) + DAC_W_DATA, bi->CLUT[c].Red >> bppDiff);
            writeReg(MMIOBase, DWORD_OFFSET(DAC_REGS) + DAC_W_DATA, bi->CLUT[c].Green >> bppDiff);
            writeReg(MMIOBase, DWORD_OFFSET(DAC_REGS) + DAC_W_DATA, bi->CLUT[c].Blue >> bppDiff);
        }
    }

    DFUNC(VERBOSE, "SetDAC_GX: ATI 68860 programmed (colorDepth=%ld, gmode=0x%02x, dsetup=0x%02x, mask=0x%02x)\n",
          (ULONG)colorDepth, pDacProgTab->gmode, pDacProgTab->dsetup, mask);
}


/**
 * Compute VCLK frequency from ICS2595 PLL values
 * Uses the standard PLL formula: frequency = (2 * R * N) / (M * P)
 * Note: The ICS2595-specific calculation with n_adj is only used when programming
 * the chip; for frequency computation we use the standard formula that matches
 * how computePLLValues calculates the values.
 * @param bi BoardInfo structure
 * @param pllValues PLL values containing N and Pidx
 * @return Frequency in 0.1 kHz units
 */
static ULONG computeVCLKFrequency_ICS2595(const struct BoardInfo *bi, const struct PLLValue *pllValues)
{
    return computeFrequencyKhz10FromPllValue(bi, pllValues, g_VPLLPostDivider);
}

static void ASM SetClock_GX(__REGA0(struct BoardInfo *bi))
{
    DFUNC(VERBOSE, "\n");
    // Program the ICS2595 MCLK0 with the desired frequency at the selected entry
    // We're using VCLK 0
    ProgramICS2595(bi, 0b00000, bi->ModeInfo->PixelClock / 10000);
}


BOOL InitMach64GX(struct BoardInfo *bi)
{
    DFUNC(INFO, "\n");
    MMIOBASE();

    ULONG chipID         = R_MMIO_L(CONFIG_CHIP_ID);
    const char *chipType = "unknown";
    switch (chipID & 0xFFFF) {
    case 0xD7:  // Mach64 GX
        chipType = "GX";
        break;
    case 0x57:  // Mach64 CX
        chipType = "CX";
        break;
    case 0x97:  // Mach64 EX
        chipType = "EX";
        break;
    case 0x53:  // Mach64 CT
        chipType = "CT";
        break;
    };
    DFUNC(INFO, "Detected ATI Mach64 %s (Device ID: 0x%04lx, Chip  Revision 0x%02lx)\n", chipType, chipID & 0xFFFF,
          chipID >> 24);

    ULONG configStat0 = R_MMIO_L(CONFIG_STAT0);
    print_CONFIG_STAT0((CONFIG_STAT0_t *)&configStat0);
    ULONG configStat1 = R_MMIO_L(CONFIG_STAT1);
    print_CONFIG_STAT1((CONFIG_STAT1_t *)&configStat1);
    ULONG memCntl = R_MMIO_L(MEM_CNTL);
    print_MEM_CNTL((MEM_CNTL_t *)&memCntl);

    setMemoryClock(bi, getChipSpecific(bi)->maxVRAMClock);
    if (!probeMemorySize(bi)) {
        return FALSE;
    }


    getChipSpecific(bi)->computeVCLKFrequency = computeVCLKFrequency_ICS2595;

    InitVClockPLLTable(bi, g_VPLLPostDivider, ARRAY_SIZE(g_VPLLPostDivider));

    bi->SetDAC   = SetDAC_GX;
    bi->SetClock = SetClock_GX;

    return TRUE;
}
