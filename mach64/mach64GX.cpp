#include "mach64GX.h"
#include "chip_mach64.h"
#include "mach64_common.h"

#include <hardware/custom.h>
#include <hardware/intbits.h>

/* Sibling-call into writeRGB514Index past its movem save (still on gcc13 / -mregparm=4).
 * Distinct from the gcc13 fix for mregparm sibcalls that clobber arg regs. */
#pragma GCC optimize("no-optimize-sibling-calls")

using namespace MmioReg;

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_STAT1 (0x3A)

// DAC_REGS sub-registers (same as VGA I/O ports 0x3c8/0x3c7/0x3c9)
#define DAC_W_INDEX 0
#define DAC_R_INDEX 3
#define DAC_W_DATA  1
#define DAC_R_DATA  2
#define DAC_MASK    2

// DAC_CNTL register layout:
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

static void ProgramICS2595Word(BoardInfo_t *bi, UBYTE entry, UWORD programWord);

// ATI 68860 RAMDAC programming structures and functions
typedef struct
{
    UBYTE gmode;   // Graphics mode register value (REG0B)
    UBYTE dsetup;  // Device setup register value (REG0C base)
} A68860_DAC_Table;

/* ATI 68860 mode table. Index = COLOR_DEPTH_* / PIX_WIDTH*.
 * GMR: 82h=4, 83h=8, A0h=15, A1h=16, C0h=24, E3h=32 RGBA.
 * E2 = alpha key (not BGRA); E1 also works for RGBA with A=0. */
static const A68860_DAC_Table A68860_Modes[] = {
    {0x01, 0x63},  // COLOR_DEPTH_1 (unused)
    {0x82, 0x61},  // COLOR_DEPTH_4
    {0x83, 0x61},  // COLOR_DEPTH_8
    {0xA0, 0x60},  // COLOR_DEPTH_15
    {0xA1, 0x60},  // COLOR_DEPTH_16
    {0xC0, 0x60},  // COLOR_DEPTH_24
    {0xE3, 0x60},  // COLOR_DEPTH_32 — RGBA
    {0x80, 0x61}   // VGA
};

/* VBIOS dac_Program68860 preserves REG0C bit7 (snow/delay); mask only that bit. */
#define A860_DELAY_L 0x80

/* Cold sparse table / post_WriteSparseRegTable — VRAM serial latch timing for scanout. */
#define MEM_CNTL_GX_TIMING 0x03F0

/**
 * Set RS2/RS3 control bits in DAC_CNTL register
 * @param bi BoardInfo structure
 * @param dacRS2RS3 Bit mask: DAC_EXT_SEL_RS2 (bit 0) and/or DAC_EXT_SEL_RS3 (bit 1)
 */
static void SetRS2RS3(BoardInfo_t *bi, UBYTE dacRS2RS3)
{
    DRIVER_LOCALS(bi);
    UBYTE current = mmio.readB(DAC_CNTL, 0);
    current       = (current & ~(DAC_EXT_SEL_RS2_MASK | DAC_EXT_SEL_RS3_MASK)) | dacRS2RS3;
    mmio.writeB(DAC_CNTL, 0, current);
}

static UBYTE RGBFTYPE_to_colorDepth(ULONG format)
{
    switch ((RGBFTYPE)format) {
    case RGBFB_CLUT:
        return COLOR_DEPTH_8;
    case RGBFB_R5G5B5:
    case RGBFB_R5G5B5PC:
    case RGBFB_B5G5R5PC:
        return COLOR_DEPTH_15;
    case RGBFB_R5G6B5:
    case RGBFB_R5G6B5PC:
    case RGBFB_B5G6R5PC:
        return COLOR_DEPTH_16;
    case RGBFB_R8G8B8:
    case RGBFB_B8G8R8:
        return COLOR_DEPTH_24;
    case RGBFB_R8G8B8A8:
    case RGBFB_A8R8G8B8:
    case RGBFB_B8G8R8A8:
    case RGBFB_A8B8G8R8:
        return COLOR_DEPTH_32;
    default:
        return COLOR_DEPTH_8;
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
    ULONG cfg_init_dac_type : 3;       // Bits 9-11: Initial DAC type (1=IBM RGB514, 2=ATI68875/TI34075,
                                       // 3=Bt476/Bt478, 4=Bt481, 5=ATI68860/68880, 6=STG1700, 7=SC15021)
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
    case 1:
        D(0, " (IBM RGB514)\n");
        break;
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

    DRIVER_LOCALS(bi);
    LOCAL_SYSBASE();

    // Turn off memory boundary; apply BIOS-like serial latch timing (see mach64gx_vbios_hw_init.md).
    mmio.writeMaskL(MEM_CNTL, (0x7 << 16), 0);
    mmio.writeMaskL(MEM_CNTL, ~0x7u, MEM_CNTL_GX_TIMING);

    static const ULONG memorySizes[] = {0x200000, 0x100000, 0x80000};
    static const ULONG memoryCodes[] = {2, 1, 0};

    volatile UBYTE *fb = reinterpret_cast<volatile UBYTE *>(bi->MemoryBase);
    ULONG memCntlSave  = mmio.readL(MEM_CNTL);

    for (int i = 0; i < ARRAY_SIZE(memorySizes); i++) {
        ULONG size     = memorySizes[i];
        bi->MemorySize = size;
        D(VERBOSE, "\nProbing memory size %ld\n", size);

        mmio.writeMaskL(MEM_CNTL, 0x7, memoryCodes[i]);

        ULONG high = size - 1;
        ULONG mid  = size >> 1;

        fb[0]    = 0x00;
        fb[mid]  = 0x5A;
        fb[high] = 0xA5;

        CacheClearU();

        UBYTE r0   = fb[0];
        UBYTE rMid = fb[mid];
        UBYTE rHi  = fb[high];

        D(VERBOSE, "Byte probe 0=0x%02lx mid@0x%lx=0x%02lx high@0x%lx=0x%02lx\n", (ULONG)r0, mid, (ULONG)rMid, high,
          (ULONG)rHi);

        if (r0 == 0x00 && rMid == 0x5A && rHi == 0xA5) {
            D(VERBOSE, "Memory size sucessfully probed.\n\n");
            return TRUE;
        }

        if (r0 == 0xff) {
            D(WARN, "BAR0 wedged after MEM_SIZE=%ld; restoring MEM_CNTL 0x%08lx\n", memoryCodes[i], memCntlSave);
            mmio.writeL(MEM_CNTL, memCntlSave & ~(0x7 << 16));
            memCntlSave = mmio.readL(MEM_CNTL);
        }
    }
    D(VERBOSE, "Memory size probe failed.\n\n");
    return FALSE;
}

// CLOCK_CNTL register bits for clock selection (bits 0-3 select ICS2595 entry)
#define CLOCK_SEL_MASK_GX (0x0F)  // Bits 0-3: Clock select (0-15)
#define CLOCK_SEL_GX(x)   ((x) & 0x0F)

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
    DRIVER_LOCALS(bi);
    ULONG mask = ICS2595_FS2_MASK | ICS2595_FS3_MASK;
    mmio.writeMaskL(CLOCK_CNTL, mask, bits);
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
    DRIVER_LOCALS(bi);
    delayMicroSeconds(26);  // 26us settle delay

    // Set strobe bit - hardware will auto-clear it
    mmio.writeMaskL(CLOCK_CNTL, ICS2595_STROBE_MASK, ICS2595_STROBE_BIT);
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
    ics2595_setFSBits(bi, ICS2595_FS3_BIT | data);
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
    // VBIOS FreqToIcs2595Word scales until freq >= 8000 (80MHz in 10kHz units).
    while (adjustedFreq < 8000 && divider > 0) {
        adjustedFreq <<= 1;
        divider--;
    }

    // Calculate N value: N = (freq * refDivider + refFreq/2) / refFreq
    // Round using RefFreq/2 (not RefDivider/2).
    temp = (ULONG)adjustedFreq * cs->referenceDivider;
    temp += (cs->referenceFrequency >> 1);
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

/* GX-only: pack ICS program word into PLLValue_t (VT/GT keep N/Pidx as VPLL fields). */
static void ics2595_packWord(PLLValue_t *pll, UWORD word)
{
    pll->N    = (UBYTE)(word & 0xff);
    pll->Pidx = (UBYTE)((word >> 8) & 0xff);
}

static UWORD ics2595_unpackWord(const PLLValue_t *pll)
{
    return (UWORD)pll->N | ((UWORD)pll->Pidx << 8);
}

/* Inverse of ics2595_calculateProgramWord — frequency in 10 kHz units. */
static UWORD ics2595_freqFromWord(const BoardInfo_t *bi, UWORD word)
{
    const ChipSpecific_t *cs = getConstChipSpecific(bi);
    UWORD n                  = word & 0xff;
    UWORD divider            = (word >> 9) & 3; /* 3=/1, 2=/2, 1=/4, 0=/8 */
    ULONG adjusted;

    if (!cs->referenceFrequency || !cs->referenceDivider)
        return 0;

    adjusted = ((ULONG)(n + 257) * cs->referenceFrequency) / cs->referenceDivider;
    return (UWORD)(adjusted >> (3 - divider));
}

/**
 * Program ICS2595 with a precomputed 13-bit word.
 */
static void ProgramICS2595Word(BoardInfo_t *bi, UBYTE entry, UWORD programWord)
{
    DFUNC(VERBOSE, "Programming ICS2595: entry=%ld, word=0x%04lx\n", (ULONG)entry, (ULONG)programWord);

    DRIVER_LOCALS(bi);
    LOCAL_SYSBASE();
    Disable();

    mmio.writeL(CLOCK_CNTL, 0);
    ics2595_strobe(bi);
    mmio.writeL(CLOCK_CNTL, 1);
    ics2595_strobe(bi);

    ics2595_sendBit(bi, 1);
    ics2595_sendBit(bi, 0);
    ics2595_sendBit(bi, 0);

    for (int i = 0; i < 5; i++) {
        ics2595_sendBit(bi, entry & 1);
        entry >>= 1;
    }

    for (int i = 0; i < 13; i++) {
        ics2595_sendBit(bi, programWord & 1);
        programWord >>= 1;
    }

    Enable();
    mmio.writeL(CLOCK_CNTL, 0);
    DFUNC(VERBOSE, "ICS2595 programming complete\n");
}

#define CLOCK_DIV_MASK 0x30
#define CLOCK_DIV4     0x20

static void setCrtcPixWidth(BoardInfo_t *bi, ULONG format)
{
    DRIVER_LOCALS(bi);
    static const UBYTE bitWidths[] = {
        COLOR_DEPTH_4,   // RGBFB_NONE
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
    };
    if (format < ARRAY_SIZE(bitWidths))
        mmio.writeMaskL(CRTC_GEN_CNTL, CRTC_PIX_WIDTH_MASK, CRTC_PIX_WIDTH(bitWidths[format]));
}

static void writeDacPalette(BoardInfo_t *bi, ULONG format)
{
    DRIVER_LOCALS(bi);
    mmio.writeB(DAC_REGS, DAC_W_INDEX, 0);
    if (format != RGBFB_CLUT) {
        for (UWORD c = 0; c < 256; c++) {
            UBYTE gray = (UBYTE)c;
            mmio.writeB(DAC_REGS, DAC_W_DATA, gray);
            mmio.writeB(DAC_REGS, DAC_W_DATA, gray);
            mmio.writeB(DAC_REGS, DAC_W_DATA, gray);
        }
    } else {
        for (UWORD c = 0; c < 256; c++) {
            mmio.writeB(DAC_REGS, DAC_W_DATA, bi->CLUT[c].Red);
            mmio.writeB(DAC_REGS, DAC_W_DATA, bi->CLUT[c].Green);
            mmio.writeB(DAC_REGS, DAC_W_DATA, bi->CLUT[c].Blue);
        }
    }
}

static void applyGxDac8BitBlanking(BoardInfo_t *bi)
{
    DRIVER_LOCALS(bi);
    ULONG dacBits = DAC_8BIT_EN;
    if (!(bi->CardFlags & CFF_BLACKLEVEL_BLACK))
        dacBits |= DAC_BLANKING;
    mmio.writeMaskL(DAC_CNTL, DAC_8BIT_EN_MASK | DAC_BLANKING_MASK, dacBits);
}

void ASM Mach64Driver::setDAC_GX(__REGD0(UWORD region), __REGD7(RGBFTYPE_REG format))
{
    (void)region;
    DRIVER_LOCALS(this);

    const A68860_DAC_Table *pDacProgTab = &A68860_Modes[RGBFTYPE_to_colorDepth(format)];

    UBYTE mask;
    if (MemorySize < 0x100000)
        mask = 4;
    else if (MemorySize == 0x100000)
        mask = 8;
    else
        mask = 0x0c;

    UBYTE gmode = pDacProgTab->gmode;
    /* Do not OR 0x10 here — that yields F2/F3 and different packing on this DAC. */

    /* Match VBIOS dac_Program68860_SI_CH (113-25517-100). */
    SetRS2RS3(this, DAC_EXT_SEL_RS2 | DAC_EXT_SEL_RS3);
    mmio.writeB(DAC_REGS, DAC_W_INDEX, mmio.readB(DAC_REGS, DAC_W_INDEX) & 0xfd);
    SetRS2RS3(this, DAC_EXT_SEL_RS3);
    mmio.writeB(DAC_REGS, DAC_MASK, 0x1d);
    mmio.writeB(DAC_REGS, DAC_R_INDEX, gmode);
    mmio.writeB(DAC_REGS, DAC_W_INDEX, 0x02);
    SetRS2RS3(this, DAC_EXT_SEL_RS2 | DAC_EXT_SEL_RS3);

    UBYTE d = pDacProgTab->dsetup;
    /* REG0C bit0: 1=6bit LUT, 0=8bit. P96 wants 8bit (DAC_CNTL bit8). */
    d &= 0xfe;
    UBYTE reg0CValue = (d | mask) | (mmio.readB(DAC_REGS, DAC_W_INDEX) & A860_DELAY_L);
    mmio.writeB(DAC_REGS, DAC_W_INDEX, reg0CValue);

    SetRS2RS3(this, 0);
    mmio.writeB(DAC_REGS, DAC_MASK, 0xff);
    applyGxDac8BitBlanking(this);

    setCrtcPixWidth(this, format);
    /* Hi-color still indexes the LUT — need identity ramp (SDK init_palettized). */
    if (format != RGBFB_CLUT)
        writeDacPalette(this, format);

    DFUNC(VERBOSE, "SetDAC 68860: gmode=0x%02lx reg0C=0x%02lx format=%ld\n", (ULONG)gmode, (ULONG)reg0CValue,
          (ULONG)format);
}

static void ASM SetDAC_GX(__REGA0(struct BoardInfo *bi), __REGD0(UWORD region), __REGD7(RGBFTYPE_REG format))
{
    asMach64(bi)->setDAC_GX(region, format);
}

/* IBM RGB514. Indexed via RS2 only.
 * Table keeps pixel_cntl only (PITCH_INFO_DAC / 8-bit LUT path). */
typedef struct
{
    UBYTE pixel_dly;
    UBYTE misc2_cntl;
    UBYTE pixel_rep;
    UBYTE pixel_cntl_index;
    UBYTE pixel_cntl; /* pixel control */
} RGB514_DAC_Table;

static const RGB514_DAC_Table RGB514_Modes[] = {
    {0xff, 0xff, 0x00, 0x00, 0x00}, /* COLOR_DEPTH_1 — sleep sentinel */
    {0x00, 0x41, 0x02, 0x71, 0x45}, /* 4bpp */
    {0x00, 0x41, 0x03, 0x71, 0x45}, /* 8bpp */
    {0x00, 0x45, 0x04, 0x0c, 0x00}, /* 555 */
    {0x00, 0x45, 0x04, 0x0c, 0x02}, /* 565 */
    {0x02, 0x45, 0x05, 0x0d, 0x00}, /* 24bpp */
    {0x02, 0x45, 0x06, 0x0e, 0x00}, /* 32bpp */
    {0x00, 0x00, 0x03, 0x71, 0x04}, /* VGA */
};

#define RGB514_REF_FREQ 1432 /* 14.32 MHz in 10 kHz units */
#define RGB514_MIN_FREQ 12000
#define RGB514_MAX_FREQ 24000
#define RGB514_MAX_N    0x1f
#define RGB514_MAX_M    0x3f

/* IBM RGB514 REG06: Border Color R/G/B. */
#define RGB514_BORDER_R 0x60
#define RGB514_BORDER_G 0x61
#define RGB514_BORDER_B 0x62

static void writeRGB514Index(BoardInfo_t *bi, UWORD index, UBYTE data)
{
    DRIVER_LOCALS(bi);
    SetRS2RS3(bi, DAC_EXT_SEL_RS2);
    mmio.writeB(DAC_REGS, DAC_W_INDEX, (UBYTE)(index & 0xff));
    mmio.writeB(DAC_REGS, DAC_W_DATA, (UBYTE)((index >> 8) & 0xff));
    mmio.writeB(DAC_REGS, DAC_MASK, data);
    SetRS2RS3(bi, 0);
}

static UBYTE readRGB514Index(BoardInfo_t *bi, UWORD index)
{
    DRIVER_LOCALS(bi);
    SetRS2RS3(bi, DAC_EXT_SEL_RS2);
    mmio.writeB(DAC_REGS, DAC_W_INDEX, (UBYTE)(index & 0xff));
    mmio.writeB(DAC_REGS, DAC_W_DATA, (UBYTE)((index >> 8) & 0xff));
    {
        UBYTE data = mmio.readB(DAC_REGS, DAC_MASK);
        SetRS2RS3(bi, 0);
        return data;
    }
}

static void convRGB514PLLValue(UWORD *mhz100)
{
    static const UWORD pllconvtable[][3] = {
        {3200, 3220, 3118},   {4990, 5010, 4980},   {5660, 5670, 5670},
        {6500, 6510, 6490},   {6750, 6760, 6760},   {7500, 7520, 7470},
        {11000, 11020, 11020},{13500, 13520, 13520},{15600, 15620, 15620},
        {0, 0, 0},
    };
    UWORD i;

    for (i = 0; pllconvtable[i][0]; ++i) {
        if (*mhz100 >= pllconvtable[i][0] && *mhz100 <= pllconvtable[i][1]) {
            *mhz100 = pllconvtable[i][2];
            return;
        }
    }
}

/* Pack: [15:14]=p, [13:8]=m, [5:0]=n. */
static UWORD rgb514_calculateProgramWord(UWORD mhz100)
{
    UBYTE p, m, n, save_m = 0, save_n = 2, save_p = 0;
    ULONG bestErr = ~0UL;
    UWORD target;

    if (mhz100 < (RGB514_MIN_FREQ >> 3))
        mhz100 = RGB514_MIN_FREQ >> 3;
    if (mhz100 > RGB514_MAX_FREQ)
        mhz100 = RGB514_MAX_FREQ;
    convRGB514PLLValue(&mhz100);
    target = mhz100;

    for (p = 3; p > 0; --p) {
        if (mhz100 < RGB514_MIN_FREQ)
            mhz100 <<= 1;
        else
            break;
    }

    for (m = 0; m <= RGB514_MAX_M; ++m) {
        for (n = 2; n <= RGB514_MAX_N; ++n) {
            ULONG actual = ((ULONG)RGB514_REF_FREQ * (m + 65)) / ((ULONG)n << (3 - p));
            ULONG err;

            if (actual >= target)
                continue;
            err = target - actual;
            if (err < bestErr) {
                bestErr = err;
                save_m  = m;
                save_n  = n;
                save_p  = p;
            }
        }
    }

    return (UWORD)(((save_m & 0x3f) | ((save_p & 3) << 6)) << 8) | save_n;
}

static UWORD rgb514_freqFromWord(UWORD word)
{
    UBYTE m = (UBYTE)(word >> 8);
    UBYTE n = (UBYTE)(word & 0xff);
    UBYTE p = m >> 6;
    UBYTE mv = m & 0x3f;

    if (!n)
        return 0;
    return (UWORD)(((ULONG)RGB514_REF_FREQ * (mv + 65)) / ((ULONG)n << (3 - p)));
}

void ASM Mach64Driver::setDAC_RGB514(__REGD0(UWORD region), __REGD7(RGBFTYPE_REG format))
{
    UBYTE depth = RGBFTYPE_to_colorDepth(format);
    const RGB514_DAC_Table *tab = &RGB514_Modes[depth];

    (void)region;
    DRIVER_LOCALS(this);

    mmio.writeMaskL(GEN_TEST_CNTL, GEN_OVS_EN_MASK, GEN_OVS_EN);

    writeRGB514Index(this, 0x90, 0x00);
    if (tab->pixel_dly == 0xff) {
        writeRGB514Index(this, 0x05, 0x01);
        return;
    }

    writeRGB514Index(this, 0x04, tab->pixel_dly);
    writeRGB514Index(this, 0x05, 0x00);
    writeRGB514Index(this, 0x02, 0x01);
    writeRGB514Index(this, 0x71, tab->misc2_cntl);
    writeRGB514Index(this, 0x0a, tab->pixel_rep);
    writeRGB514Index(this, tab->pixel_cntl_index, tab->pixel_cntl);

    if (mmio.readL(CRTC_GEN_CNTL) & CRTC_INTERLACE_EN) {
        UBYTE misc2 = readRGB514Index(this, 0x71);
        writeRGB514Index(this, 0x71, (UBYTE)(misc2 | 0x20));
    }

    mmio.writeB(DAC_REGS, DAC_MASK, 0xff);
    applyGxDac8BitBlanking(this);

    setCrtcPixWidth(this, format);
    /* pixel_cntl keeps LUT in path — identity ramp for 15/16/32bpp. */
    if (format != RGBFB_CLUT) {
        writeDacPalette(this, format);
        writeRGB514Index(this, RGB514_BORDER_R, 0);
        writeRGB514Index(this, RGB514_BORDER_G, 0);
        writeRGB514Index(this, RGB514_BORDER_B, 0);
    }

    DFUNC(VERBOSE, "SetDAC RGB514: depth=%ld fmt=%ld cntl_idx=0x%02lx cntl=0x%02lx\n", (ULONG)depth, (ULONG)format,
          (ULONG)tab->pixel_cntl_index, (ULONG)tab->pixel_cntl);
}

static void ASM SetDAC_RGB514(__REGA0(struct BoardInfo *bi), __REGD0(UWORD region), __REGD7(RGBFTYPE_REG format))
{
    asMach64(bi)->setDAC_RGB514(region, format);
}

/**
 * Compute VCLK from GX-packed ICS word in PLLValue_t (N=lo, Pidx=hi).
 * Not the internal CT/GT VPLL formula — VT/GT use computeFrequencyKhz10FromPllValue.
 */
static ULONG computeVCLKFrequency_ICS2595(const struct BoardInfo *bi, const struct PLLValue *pllValues)
{
    return ics2595_freqFromWord(bi, ics2595_unpackWord(pllValues));
}

/* Known-good ICS words from bring-up (10 kHz units → word). Seeded into the table. */
static const struct
{
    UWORD freqKhz10;
    UWORD word;
} g_ics2595KnownGood[] = {
    {2517, 0x1a40}, /* ~25.175 MHz 640x480 */
    {4000, 0x1c00}, /* 40 MHz 800x600 */
    {6500, 0x1ca1}, /* 65 MHz 1024x768 */
};

static void InitICS2595ClockTable(BoardInfo_t *bi)
{
    ChipSpecific_t *cs = getChipSpecific(bi);
    LOCAL_SYSBASE();

    if (cs->maxPClock <= cs->minPClock) {
        DFUNC(ERROR, "invalid PCLK range min=%ld max=%ld\n", (ULONG)cs->minPClock, (ULONG)cs->maxPClock);
        return;
    }

    UWORD maxNumEntries   = (UWORD)((cs->maxPClock - cs->minPClock) / 100u + 2u + ARRAY_SIZE(g_ics2595KnownGood));
    PLLValue_t *pllValues = static_cast<PLLValue_t *>(AllocVec(sizeof(PLLValue_t) * maxNumEntries, MEMF_ANY));
    if (!pllValues) {
        DFUNC(ERROR, "AllocVec ICS clock table failed\n");
        return;
    }
    cs->vclkPllValues = pllValues;

    UWORD e        = 0;
    UWORD lastFreq = 0;
    UWORD lastWord = 0xffff;
    UWORD target   = cs->minPClock;
    while (target < cs->maxPClock && e < maxNumEntries) {
        UWORD word = ics2595_calculateProgramWord(bi, target);
        UWORD freq = ics2595_freqFromWord(bi, word);
        if (freq && !(freq == lastFreq && word == lastWord)) {
            ics2595_packWord(&pllValues[e], word);
            lastFreq = freq;
            lastWord = word;
            ++e;
        }
        target += 100;
    }
    if (e < maxNumEntries) {
        UWORD word = ics2595_calculateProgramWord(bi, cs->maxPClock);
        UWORD freq = ics2595_freqFromWord(bi, word);
        if (freq && !(freq == lastFreq && word == lastWord)) {
            ics2595_packWord(&pllValues[e], word);
            ++e;
        }
    }

    /* Ensure bring-up known-good words are present (sorted insert by achieved freq). */
    for (UWORD k = 0; k < ARRAY_SIZE(g_ics2595KnownGood) && e < maxNumEntries; ++k) {
        UWORD word = g_ics2595KnownGood[k].word;
        UWORD freq = ics2595_freqFromWord(bi, word);
        UWORD i;

        for (i = 0; i < e; ++i) {
            if (ics2595_unpackWord(&pllValues[i]) == word)
                break;
        }
        if (i < e)
            continue;

        for (i = 0; i < e; ++i) {
            if (ics2595_freqFromWord(bi, ics2595_unpackWord(&pllValues[i])) > freq)
                break;
        }
        for (UWORD j = e; j > i; --j)
            pllValues[j] = pllValues[j - 1];
        ics2595_packWord(&pllValues[i], word);
        ++e;
    }

    if (e == 0) {
        DFUNC(ERROR, "ICS2595 clock table empty\n");
        return;
    }

    const ULONG maxHiColorFreq = 8000; /* 80 MHz — same GX/VT-class cap as InitVClockPLLTable */
    for (int i = 0; i < 5; i++)
        bi->PixelClockCount[i] = 0;

    for (UWORD i = 0; i < e; ++i) {
        ULONG frequency = computeVCLKFrequency_ICS2595(bi, &pllValues[i]);
        bi->PixelClockCount[CHUNKY]++;
        if (frequency <= maxHiColorFreq) {
            bi->PixelClockCount[HICOLOR]++;
            bi->PixelClockCount[TRUECOLOR]++;
            bi->PixelClockCount[TRUEALPHA]++;
        }
    }

    DFUNC(VERBOSE, "GX ICS table: %ld clocks, CHUNKY %ld, range %ld0..%ld0 kHz\n", (ULONG)e,
          (ULONG)bi->PixelClockCount[CHUNKY], (ULONG)computeVCLKFrequency_ICS2595(bi, &pllValues[0]),
          (ULONG)computeVCLKFrequency_ICS2595(bi, &pllValues[e - 1]));
}

/* Accelerator VCLK uses ICS entry 0 (same as the successful bring-up log). */
#define GX_VCLK_ENTRY 0

static void ics2595_selectAndStrobe(BoardInfo_t *bi, UBYTE entry)
{
    DRIVER_LOCALS(bi);
    /* CLOCK_CNTL bits 0–3 = entry, bit 6 = strobe (auto-clears). Working log: 0x40. */
    mmio.writeL(CLOCK_CNTL, CLOCK_SEL_GX(entry) | ICS2595_STROBE_BIT);
    delayMicroSeconds(26);
}

void ASM Mach64Driver::setClock_GX()
{
    DFUNC(VERBOSE, "\n");
    DRIVER_LOCALS(this);

    /* ICS2595 clock: EXT_DISP on, program entry, CLOCK_SEL|STROBE. */
    mmio.writeMaskL(CRTC_GEN_CNTL, CRTC_EXT_DISP_EN_MASK, CRTC_EXT_DISP_EN);

    /* Precomputed at ResolvePixelClock — GX packs ICS word into pll1/pll2. */
    UWORD word = (UWORD)ModeInfo->pll1.Numerator | ((UWORD)ModeInfo->pll2.Denominator << 8);

    D(VERBOSE, "SetClock_GX: %ld Hz -> ICS word 0x%04lx\n", ModeInfo->PixelClock, (ULONG)word);
    ProgramICS2595Word(this, GX_VCLK_ENTRY, word);
    delayMilliSeconds(1);
    ics2595_selectAndStrobe(this, GX_VCLK_ENTRY);
}

static void ASM SetClock_GX(__REGA0(struct BoardInfo *bi))
{
    asMach64(bi)->setClock_GX();
}

static ULONG computeVCLKFrequency_RGB514(const struct BoardInfo *bi, const struct PLLValue *pllValues)
{
    (void)bi;
    /* Reuse ICS pack helpers: PLLValue_t holds a 16-bit RGB514 program word. */
    return rgb514_freqFromWord(ics2595_unpackWord(pllValues));
}

static void InitRGB514ClockTable(BoardInfo_t *bi)
{
    ChipSpecific_t *cs = getChipSpecific(bi);
    LOCAL_SYSBASE();

    if (cs->maxPClock <= cs->minPClock) {
        DFUNC(ERROR, "invalid PCLK range min=%ld max=%ld\n", (ULONG)cs->minPClock, (ULONG)cs->maxPClock);
        return;
    }

    UWORD maxNumEntries   = (UWORD)((cs->maxPClock - cs->minPClock) / 100u + 2u);
    PLLValue_t *pllValues = static_cast<PLLValue_t *>(AllocVec(sizeof(PLLValue_t) * maxNumEntries, MEMF_ANY));
    if (!pllValues) {
        DFUNC(ERROR, "AllocVec RGB514 clock table failed\n");
        return;
    }
    cs->vclkPllValues = pllValues;

    UWORD e        = 0;
    UWORD lastFreq = 0;
    UWORD lastWord = 0xffff;
    UWORD target   = cs->minPClock;
    while (target < cs->maxPClock && e < maxNumEntries) {
        UWORD word = rgb514_calculateProgramWord(target);
        UWORD freq = rgb514_freqFromWord(word);
        if (freq && !(freq == lastFreq && word == lastWord)) {
            ics2595_packWord(&pllValues[e], word);
            lastFreq = freq;
            lastWord = word;
            ++e;
        }
        target += 100;
    }
    if (e < maxNumEntries) {
        UWORD word = rgb514_calculateProgramWord(cs->maxPClock);
        UWORD freq = rgb514_freqFromWord(word);
        if (freq && !(freq == lastFreq && word == lastWord)) {
            ics2595_packWord(&pllValues[e], word);
            ++e;
        }
    }

    if (e == 0) {
        DFUNC(ERROR, "RGB514 clock table empty\n");
        return;
    }

    {
        const ULONG maxHiColorFreq = 8000;
        int i;

        for (i = 0; i < 5; i++)
            bi->PixelClockCount[i] = 0;

        for (UWORD i = 0; i < e; ++i) {
            ULONG frequency = computeVCLKFrequency_RGB514(bi, &pllValues[i]);
            bi->PixelClockCount[CHUNKY]++;
            if (frequency <= maxHiColorFreq) {
                bi->PixelClockCount[HICOLOR]++;
                bi->PixelClockCount[TRUECOLOR]++;
                bi->PixelClockCount[TRUEALPHA]++;
            }
        }
    }

    DFUNC(VERBOSE, "GX RGB514 table: %ld clocks, CHUNKY %ld, range %ld0..%ld0 kHz\n", (ULONG)e,
          (ULONG)bi->PixelClockCount[CHUNKY], (ULONG)computeVCLKFrequency_RGB514(bi, &pllValues[0]),
          (ULONG)computeVCLKFrequency_RGB514(bi, &pllValues[e - 1]));
}

void ASM Mach64Driver::setClock_RGB514()
{
    UWORD word;

    DFUNC(VERBOSE, "\n");
    DRIVER_LOCALS(this);

    mmio.writeMaskL(CRTC_GEN_CNTL, CRTC_EXT_DISP_EN_MASK, CRTC_EXT_DISP_EN);

    word = (UWORD)ModeInfo->pll1.Numerator | ((UWORD)ModeInfo->pll2.Denominator << 8);
    D(VERBOSE, "SetClock_RGB514: %ld Hz -> word 0x%04lx\n", ModeInfo->PixelClock, (ULONG)word);

    writeRGB514Index(this, 0x06, 0x02); /* DAC Operation */
    writeRGB514Index(this, 0x10, 0x01); /* PLL Control 1 */
    writeRGB514Index(this, 0x70, 0x01); /* Misc Control 1 */
    writeRGB514Index(this, 0x8f, 0x1f); /* PLL Ref. Divider Input */
    writeRGB514Index(this, 0x03, 0x00); /* Sync Control */
    writeRGB514Index(this, 0x05, 0x00); /* Power Management */
    writeRGB514Index(this, 0x20, (UBYTE)(word >> 8));
    writeRGB514Index(this, 0x21, (UBYTE)(word & 0xff));
}

static void ASM SetClock_RGB514(__REGA0(struct BoardInfo *bi))
{
    asMach64(bi)->setClock_RGB514();
}

/* IBM RGB514: REG06 0x60/61/62 = Border Color R/G/B. */
void ASM Mach64Driver::setColorArray_RGB514(__REGD0(UWORD startIndex), __REGD1(UWORD count))
{
    DFUNC(VERBOSE, "startIndex %ld, count %ld\n", (ULONG)startIndex, (ULONG)count);

    SetColorArrayInternal(this, startIndex, count, CLUT);

    /* DAC border RGB: follow CLUT[0] in 4/8 bpp; black in direct color. */
    if (startIndex == 0 && count > 0) {
        UBYTE r = 0, g = 0, b = 0;
        if (!ModeInfo || ModeInfo->Depth <= 8) {
            r = CLUT[0].Red;
            g = CLUT[0].Green;
            b = CLUT[0].Blue;
        }
        writeRGB514Index(this, RGB514_BORDER_R, r);
        writeRGB514Index(this, RGB514_BORDER_G, g);
        writeRGB514Index(this, RGB514_BORDER_B, b);
    }
}

static void ASM SetColorArray_RGB514(__REGA0(struct BoardInfo *bi), __REGD0(UWORD startIndex),
                                     __REGD1(UWORD count))
{
    asMach64(bi)->setColorArray_RGB514(startIndex, count);
}

/* ATI68860 cursor colors live in the DAC, not CUR_CLR0/1 (SDK HWCURSOR.C). */
void ASM Mach64Driver::setSpriteColor_GX(__REGD0(UBYTE index), __REGD1(UBYTE red), __REGD2(UBYTE green), __REGD3(UBYTE blue),
                                         __REGD7(RGBFTYPE_REG fmt))
{
    DRIVER_LOCALS(this);
    UBYTE pen;

    (void)fmt;
    DFUNC(VERBOSE, "Index %ld, Red %ld, Green %ld, Blue %ld\n", (ULONG)index, (ULONG)red, (ULONG)green, (ULONG)blue);

    if (index == 0)
        pen = 0;
    else if (index == 2)
        pen = 1;
    else
        return;

    cd->cursorRGB[pen][0] = red;
    cd->cursorRGB[pen][1] = green;
    cd->cursorRGB[pen][2] = blue;

    {
        UBYTE dacCntl0 = mmio.readB(DAC_CNTL, 0);

        /* DAC_CNTL[1:0]=RS2 selects 68860 cursor color bank. */
        mmio.writeB(DAC_CNTL, 0, (dacCntl0 & ~(DAC_EXT_SEL_RS2_MASK | DAC_EXT_SEL_RS3_MASK)) | DAC_EXT_SEL_RS2);
        mmio.writeB(DAC_REGS, DAC_W_INDEX, 0);
        mmio.writeB(DAC_REGS, DAC_W_DATA, cd->cursorRGB[0][0]);
        mmio.writeB(DAC_REGS, DAC_W_DATA, cd->cursorRGB[0][1]);
        mmio.writeB(DAC_REGS, DAC_W_DATA, cd->cursorRGB[0][2]);
        mmio.writeB(DAC_REGS, DAC_W_DATA, cd->cursorRGB[1][0]);
        mmio.writeB(DAC_REGS, DAC_W_DATA, cd->cursorRGB[1][1]);
        mmio.writeB(DAC_REGS, DAC_W_DATA, cd->cursorRGB[1][2]);
        mmio.writeB(DAC_CNTL, 0, mmio.readB(DAC_CNTL, 0) & ~(DAC_EXT_SEL_RS2_MASK | DAC_EXT_SEL_RS3_MASK));
    }
}

static void ASM SetSpriteColor_GX(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE index), __REGD1(UBYTE red),
                                  __REGD2(UBYTE green), __REGD3(UBYTE blue), __REGD7(RGBFTYPE_REG fmt))
{
    asMach64(bi)->setSpriteColor_GX(index, red, green, blue, fmt);
}

/* RGB514 Mode 0: 00=trans, 01=C1, 10=C2, 11=C3. */
/* Cursor Control 0x30: Mode0 | 64×64 | UPDT immediate | PIX ORDR left→right. */
#define RGB514_CURS_CTRL_ON  0x2d
#define RGB514_CURS_CTRL_OFF 0x00

static UBYTE rgb514_mode0Pix(UBYTE p0, UBYTE p1)
{
    return (UBYTE)(((p1 & 1) << 1) | (p0 & 1));
}

/* Pack one 16-pixel Mode0 group into 4 bytes (PIX ORDR=1: left pixel in bits 7:6). */
static void rgb514_packMode0Word(UBYTE *dst, UWORD plane0, UWORD plane1)
{
    UWORD x;

    for (x = 0; x < 16; x += 4) {
        UBYTE b = 0;
        UWORD i;

        for (i = 0; i < 4; ++i) {
            UWORD s  = 15 - (x + i);
            UBYTE p0 = (UBYTE)((plane0 >> s) & 1);
            UBYTE p1 = (UBYTE)((plane1 >> s) & 1);
            b |= (UBYTE)(rgb514_mode0Pix(p0, p1) << (6 - 2 * i));
        }
        *dst++ = b;
    }
}

/* Image is top-left in the 64×64 array; unused cells stay 00 (transparent). */
static void packRgb514Mode0Cursor(BoardInfo_t *bi)
{
    UBYTE *cursor = bi->MouseImageBuffer;
    UWORD height  = bi->MouseHeight;
    UWORD y, i;

    if (height > 64)
        height = 64;

    for (i = 0; i < 1024; ++i)
        cursor[i] = 0;

    if (bi->Flags & BIF_HIRESSPRITE) {
        const ULONG *image = reinterpret_cast<const ULONG *>(bi->MouseImage) + 2;
        for (y = 0; y < height; ++y) {
            ULONG plane0 = *image++;
            ULONG plane1 = *image++;
            UBYTE *row   = cursor + y * 16;

            rgb514_packMode0Word(row, (UWORD)(plane0 >> 16), (UWORD)(plane1 >> 16));
            rgb514_packMode0Word(row + 4, (UWORD)plane0, (UWORD)plane1);
        }
    } else if (bi->Flags & BIF_BIGSPRITE) {
        UWORD srcH         = height >> 1;
        const UWORD *image = bi->MouseImage + 2;

        if (srcH > 32)
            srcH = 32;
        for (y = 0; y < srcH; ++y) {
            UWORD plane0 = *image++;
            UWORD plane1 = *image++;
            UBYTE row[16];
            UWORD x;

            for (i = 0; i < 16; ++i)
                row[i] = 0;
            for (x = 0; x < 16; ++x) {
                UWORD s   = 15 - x;
                UBYTE pix = rgb514_mode0Pix((UBYTE)((plane0 >> s) & 1), (UBYTE)((plane1 >> s) & 1));
                UWORD dx  = x * 2;
                UWORD bi0 = dx >> 2;
                UWORD sh0 = 6 - 2 * (dx & 3);
                UWORD bi1 = (dx + 1) >> 2;
                UWORD sh1 = 6 - 2 * ((dx + 1) & 3);

                row[bi0] |= (UBYTE)(pix << sh0);
                row[bi1] |= (UBYTE)(pix << sh1);
            }
            for (i = 0; i < 16; ++i) {
                cursor[(y * 2) * 16 + i]     = row[i];
                cursor[(y * 2 + 1) * 16 + i] = row[i];
            }
        }
    } else {
        const UWORD *image = bi->MouseImage + 2;
        for (y = 0; y < height; ++y) {
            UWORD plane0 = *image++;
            UWORD plane1 = *image++;

            rgb514_packMode0Word(cursor + y * 16, plane0, plane1);
        }
    }
}

static void writeRGB514CursorColors(BoardInfo_t *bi)
{
    DRIVER_LOCALS(bi);
    UWORD pen;
    SetRS2RS3(bi, DAC_EXT_SEL_RS2);
    mmio.writeB(DAC_REGS, DAC_R_INDEX, 1);
    mmio.writeB(DAC_REGS, DAC_W_INDEX, 0x40);
    mmio.writeB(DAC_REGS, DAC_W_DATA, 0);
    for (pen = 0; pen < 3; ++pen) {
        mmio.writeB(DAC_REGS, DAC_MASK, cd->cursorRGB[pen][0]);
        mmio.writeB(DAC_REGS, DAC_MASK, cd->cursorRGB[pen][1]);
        mmio.writeB(DAC_REGS, DAC_MASK, cd->cursorRGB[pen][2]);
    }
    SetRS2RS3(bi, 0);
}

void ASM Mach64Driver::setSpriteColor_RGB514(__REGD0(UBYTE index), __REGD1(UBYTE red), __REGD2(UBYTE green), __REGD3(UBYTE blue),
                                             __REGD7(RGBFTYPE_REG fmt))
{
    ChipData_t *cd = chip();

    (void)fmt;
    DFUNC(VERBOSE, "Index %ld, Red %ld, Green %ld, Blue %ld\n", (ULONG)index, (ULONG)red, (ULONG)green, (ULONG)blue);

    if (index > 2)
        return;

    cd->cursorRGB[index][0] = red;
    cd->cursorRGB[index][1] = green;
    cd->cursorRGB[index][2] = blue;
    writeRGB514CursorColors(this);
}

static void ASM SetSpriteColor_RGB514(__REGA0(struct BoardInfo *bi), __REGD0(UBYTE index), __REGD1(UBYTE red),
                                      __REGD2(UBYTE green), __REGD3(UBYTE blue), __REGD7(RGBFTYPE_REG fmt))
{
    asMach64(bi)->setSpriteColor_RGB514(index, red, green, blue, fmt);
}

/* Upload Mode0 cursor RAM @ 0x100 + hotspot @ 0x35. */
void ASM Mach64Driver::setSpriteImage_RGB514(__REGD7(RGBFTYPE_REG fmt))
{
    const UBYTE *src;
    UWORD i;
    UBYTE hotX, hotY;

    (void)fmt;
    DFUNC(VERBOSE, "\n");

    packRgb514Mode0Cursor(this);
    src = MouseImageBuffer;

    /* Left-aligned image → hotspot is Mouse*Offset (SDK's 64-width+hot is for right-aligned uploads). */
    hotX = MouseXOffset;
    hotY = MouseYOffset;
    if (hotX > 63)
        hotX = 63;
    if (hotY > 63)
        hotY = 63;

    {
        DRIVER_LOCALS(this);
        SetRS2RS3(this, DAC_EXT_SEL_RS2);
        mmio.writeB(DAC_REGS, DAC_R_INDEX, 1);
        mmio.writeB(DAC_REGS, DAC_W_INDEX, 0x00);
        mmio.writeB(DAC_REGS, DAC_W_DATA, 0x01); /* cursor array @ 0x100 */
        for (i = 0; i < 1024; ++i)
            mmio.writeB(DAC_REGS, DAC_MASK, src[i]);

        mmio.writeB(DAC_REGS, DAC_W_INDEX, 0x35);
        mmio.writeB(DAC_REGS, DAC_W_DATA, 0);
        mmio.writeB(DAC_REGS, DAC_MASK, hotX);
        mmio.writeB(DAC_REGS, DAC_MASK, hotY);
        SetRS2RS3(this, 0);
    }
}

static void ASM SetSpriteImage_RGB514(__REGA0(struct BoardInfo *bi), __REGD7(RGBFTYPE_REG fmt))
{
    asMach64(bi)->setSpriteImage_RGB514(fmt);
}

void ASM Mach64Driver::setSpritePosition_RGB514(__REGD0(WORD xpos), __REGD1(WORD ypos), __REGD7(RGBFTYPE_REG fmt))
{
    WORD spriteX, spriteY;

    (void)fmt;
    DFUNC(VERBOSE, "\n");

    MouseX = xpos;
    MouseY = ypos;

    spriteX = xpos - XOffset;
    spriteY = ypos - YOffset + YSplit;

    /* Match chip_mach64 SetSpritePosition (DAC position is hotspot on screen). */
    if (ModeInfo && (ModeInfo->Flags & GMF_DOUBLESCAN))
        spriteY *= 2;

    if (spriteX < 0)
        spriteX = 0;
    if (spriteY < 0)
        spriteY = 0;

    {
        DRIVER_LOCALS(this);
        SetRS2RS3(this, DAC_EXT_SEL_RS2);
        mmio.writeB(DAC_REGS, DAC_R_INDEX, 1);
        mmio.writeB(DAC_REGS, DAC_W_INDEX, 0x31);
        mmio.writeB(DAC_REGS, DAC_W_DATA, 0);
        mmio.writeB(DAC_REGS, DAC_MASK, (UBYTE)(spriteX & 0xff));
        mmio.writeB(DAC_REGS, DAC_MASK, (UBYTE)((spriteX >> 8) & 0xff));
        mmio.writeB(DAC_REGS, DAC_MASK, (UBYTE)(spriteY & 0xff));
        mmio.writeB(DAC_REGS, DAC_MASK, (UBYTE)((spriteY >> 8) & 0xff));
        SetRS2RS3(this, 0);
    }

    D(CHATTY, "RGB514 SpritePos X: %ld Y: %ld\n", (LONG)spriteX, (LONG)spriteY);
}

static void ASM SetSpritePosition_RGB514(__REGA0(struct BoardInfo *bi), __REGD0(WORD xpos), __REGD1(WORD ypos),
                                         __REGD7(RGBFTYPE_REG fmt))
{
    asMach64(bi)->setSpritePosition_RGB514(xpos, ypos, fmt);
}

BOOL ASM Mach64Driver::setSprite_RGB514(__REGD0(BOOL activate), __REGD7(RGBFTYPE_REG RGBFormat))
{
    DFUNC(VERBOSE, "activate=%ld\n", (ULONG)activate);
    DRIVER_LOCALS(this);

    /* GEN_CUR_ENABLE still required on RGB514 boards (SDK HWCURSOR.C) plus DAC 0x30. */
    mmio.writeMaskL(GEN_TEST_CNTL, GEN_CUR_ENABLE_MASK, (activate ? GEN_CUR_ENABLE : 0));
    writeRGB514Index(this, 0x30, activate ? RGB514_CURS_CTRL_ON : RGB514_CURS_CTRL_OFF);

    if (activate) {
        setSpriteColor(0, CLUT[17].Red, CLUT[17].Green, CLUT[17].Blue, RGBFormat);
        setSpriteColor(1, CLUT[18].Red, CLUT[18].Green, CLUT[18].Blue, RGBFormat);
        setSpriteColor(2, CLUT[19].Red, CLUT[19].Green, CLUT[19].Blue, RGBFormat);
    }

    return TRUE;
}

static BOOL ASM SetSprite_RGB514(__REGA0(struct BoardInfo *bi), __REGD0(BOOL activate), __REGD7(RGBFTYPE_REG RGBFormat))
{
    return asMach64(bi)->setSprite_RGB514(activate, RGBFormat);
}

BOOL InitMach64GX(struct BoardInfo *bi)
{
    DFUNC(INFO, "\n");
    DRIVER_LOCALS(bi);

    ULONG chipID         = mmio.readL(CONFIG_CHIP_ID);
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
    case 0x53:  // Mach64 CT — must use InitMach64CT
        DFUNC(ERROR, "Mach64 CT requires InitMach64CT (integrated DAC/PLL), aborting.\n");
        return FALSE;
    };
    DFUNC(INFO, "Detected ATI Mach64 %s (Device ID: 0x%04lx, Chip  Revision 0x%02lx)\n", chipType, chipID & 0xFFFF,
          chipID >> 24);

    if (getChipData(bi)->chipFamily == MACH64CT) {
        DFUNC(ERROR, "MACH64CT family routed to InitMach64GX — use InitMach64CT\n");
        return FALSE;
    }

    ULONG configStat0 = mmio.readL(CONFIG_STAT0);
    print_CONFIG_STAT0(reinterpret_cast<CONFIG_STAT0_t *>(&configStat0));

    ULONG dacType = (configStat0 >> 9) & 7;
    if (dacType != 1 && dacType != 5) {
        DFUNC(ERROR, "Unsupported DAC type %ld aborting.\n", dacType);
        return FALSE;
    }

    ULONG configStat1 = mmio.readL(static_cast<MmioReg::Id>(CONFIG_STAT1));
    print_CONFIG_STAT1(reinterpret_cast<CONFIG_STAT1_t *>(&configStat1));
    ULONG memCntl = mmio.readL(MEM_CNTL);
    print_MEM_CNTL(reinterpret_cast<MEM_CNTL_t *>(&memCntl));

    /* Leave factory MCLK (ICS2595 on both 68860 and RGB514 boards). */
    if (!probeMemorySize(bi)) {
        return FALSE;
    }

    if (dacType == 1) {
        DFUNC(INFO, "DAC: IBM RGB514 (VCLK on DAC; ICS left for MCLK)\n");
        getChipSpecific(bi)->computeVCLKFrequency = computeVCLKFrequency_RGB514;
        InitRGB514ClockTable(bi);
        /* RGB514 0x0E=0x00 scanout is BGRA on the LE aperture. */
        bi->RGBFormats |= RGBFF_B8G8R8A8;
        P96_HOOK(bi->SetDAC, SetDAC_RGB514);
        P96_HOOK(bi->SetClock, SetClock_RGB514);
        P96_HOOK(bi->SetColorArray, SetColorArray_RGB514);
        /* On-DAC Mode0 cursor — override image/position/enable, not only colors. */
        P96_HOOK(bi->SetSprite, SetSprite_RGB514);
        P96_HOOK(bi->SetSpritePosition, SetSpritePosition_RGB514);
        P96_HOOK(bi->SetSpriteImage, SetSpriteImage_RGB514);
        P96_HOOK(bi->SetSpriteColor, SetSpriteColor_RGB514);
    } else {
        DFUNC(INFO, "DAC: ATI68860 + ICS2595 VCLK\n");
        getChipSpecific(bi)->computeVCLKFrequency = computeVCLKFrequency_ICS2595;
        InitICS2595ClockTable(bi);
        /* 68860 GMR E3 is RGBA on the LE aperture. */
        bi->RGBFormats |= RGBFF_R8G8B8A8;
        P96_HOOK(bi->SetDAC, SetDAC_GX);
        P96_HOOK(bi->SetClock, SetClock_GX);
        /* Keep Mach64 CUR_* sprite path; only colors live in the 68860. */
        P96_HOOK(bi->SetSpriteColor, SetSpriteColor_GX);
    }

    return TRUE;
}

#ifdef __cplusplus
}
#endif
