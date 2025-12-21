#include "ch8398ramdac.h"
#include "et4000_common.h"

// CH8398 Identification Register value
#define CH8398_ID_VALUE 0xC0

#define CWA DAC_WR_AD
#define CDR DAC_DATA
#define CR  DAC_MASK
#define CSA DAC_RD_AD
#define CAR DAC_WR_AD  //(RS2 = 0, RS3 = 1)

// CH8398 PLL frequency table from Chrontel specification
// Table 11: Sample Frequency Coefficients (reference frequency = 14.318 MHz)
// Format: { frequency_10khz, M, N, K } where Fout = 14.318 * ((M+2) * (2^K)) / (N+8)
// Frequency is stored in 10kHz units (e.g., 1431 = 14.318 MHz)
static const CH8398_PLLValue_t ch8398_pll_table[] = {
    {1431, 8, 72, 1},    // 14.318 MHz
    {2506, 2, 27, 1},    // 25.06 MHz
    {2520, 23, 168, 1},  // 25.20 MHz
    {2595, 6, 50, 1},    // 25.95 MHz
    {2744, 7, 61, 1},    // 27.44 MHz
    {2828, 8, 71, 1},    // 28.28 MHz
    {2833, 21, 174, 1},  // 28.33 MHz
    {2864, 2, 24, 2},    // 28.64 MHz
    {3007, 3, 34, 2},    // 30.07 MHz
    {3150, 8, 80, 2},    // 31.50 MHz
    {3257, 8, 83, 2},    // 32.57 MHz
    {3375, 5, 58, 2},    // 33.75 MHz
    {3589, 3, 17, 1},    // 35.89 MHz
    {3759, 6, 34, 1},    // 37.59 MHz
    {3848, 6, 35, 1},    // 38.48 MHz
    {4009, 8, 48, 1},    // 40.09 MHz
    {4398, 5, 35, 1},    // 43.98 MHz
    {4474, 6, 42, 1},    // 44.74 MHz
    {4725, 3, 25, 1},    // 47.25 MHz
    {5011, 3, 27, 1},    // 50.11 MHz
    {5190, 6, 50, 1},    // 51.90 MHz
    {5489, 7, 61, 1},    // 54.89 MHz
    {5656, 8, 71, 1},    // 56.56 MHz
    {5727, 2, 24, 1},    // 57.27 MHz
    {6014, 3, 34, 2},    // 60.14 MHz
    {6300, 8, 80, 1},    // 63.00 MHz
    {6515, 8, 83, 1},    // 65.15 MHz
    {6750, 5, 58, 1},    // 67.50 MHz
    {7159, 3, 17, 0},    // 71.59 MHz
    {7517, 6, 34, 0},    // 75.17 MHz
    {7696, 6, 35, 0},    // 76.96 MHz
    {8018, 8, 48, 0},    // 80.18 MHz
    {8795, 5, 35, 0},    // 87.95 MHz
    {8949, 6, 42, 0},    // 89.49 MHz
    {9450, 3, 25, 0},    // 94.50 MHz
    {10023, 4, 34, 0},   // 100.23 MHz
    {10500, 7, 58, 0},   // 105.00 MHz
    {10818, 7, 60, 0},   // 108.18 MHz
    {10977, 7, 61, 0},   // 109.77 MHz
    {11812, 2, 25, 0},   // 118.12 MHz
    {12027, 3, 34, 0},   // 120.27 MHz
    {13091, 5, 56, 0},   // 130.91 MHz
    {13500, 7, 58, 0},   // 135.00 MHz
};

#define CH8398_PLL_TABLE_SIZE (sizeof(ch8398_pll_table) / sizeof(ch8398_pll_table[0]))

BOOL CheckForCH8398(struct BoardInfo *bi)
{
    REGBASE();

    /* probe for Chrontel CH8398 RAMDAC */
    /*
     * The CH8398 has an Identification Register (IDR) that returns 0xC0.
     * The IDR can ONLY be accessed via the alternate access method:
     * 1. Set RS[2:0] to [010] (accesses RMR - Read Mask Register)
     * 2. Read RMR three consecutive times
     * 3. Fourth read returns IDR value (0xC0)
     *
     * ET4000 only has two RS pins (RS0, RS1). On this card:
     * - RS3 on CH8398 is connected to +5V (always 1)
     * - RS2 on CH8398 is connected to CS3 (clock select bit 3)
     * - CRTC register 0x31 bit 6 controls CS3
     * - When CS3=1: RS2=1
     * - When CS3=0: RS2=0
     *
     * For alternate access to IDR (RS[2:0] = 010):
     * - RS0 = 0 (from VGA address, 0x3C6)
     * - RS1 = 1 (from VGA address, 0x3C7)
     * - RS2 = 0 (from CS3=0, clear CRTC 0x31 bit 6)
     * - RS3 = 1 (always, from +5V, not used in RS[2:0])
     *
     * According to CH8398 register map:
     * RS[3:0] = 0110 (RS3=1, RS2=0, RS1=1, RS0=0) = RMR (Read Mask Register)
     * After three reads of RMR, fourth read returns IDR
     */

    UBYTE saveCR31;
    BOOL found = FALSE;

    // Save current CRTC 0x31 state
    saveCR31 = R_CR(0x31);

    // Set CS3=0 to make RS2=0 for alternate access (RS[2:0] = 010)
    W_CR_MASK(0x31, 0x40, 0x00);  // Clear bit 6 to set CS3=0 (RS2=0)

    // Alternate access method to read IDR:
    // RS[2:0] = 010 means RS[1:0] = 10, RS2 = 0
    // RS[1:0] = 10 comes from 0x3C6 (DAC_MASK)
    // Read RMR three times, then fourth read returns IDR
    R_REG(DAC_MASK);                  // First read (RMR, RS[2:0]=010)
    R_REG(DAC_MASK);                  // Second read (RMR, RS[2:0]=010)
    R_REG(DAC_MASK);                  // Third read (RMR, RS[2:0]=010)
    UBYTE idValue = R_REG(DAC_MASK);  // Fourth read returns IDR (0xC0)

    // Restore CRTC 0x31
    W_CR(0x31, saveCR31);

    if (idValue == CH8398_ID_VALUE) {
        DFUNC(0, "Found Chrontel CH8398 RAMDAC (ID: 0x%02x)\n", idValue);
        found = TRUE;
    } else {
        DFUNC(0, "CH8398 not detected, ID: 0x%02x (expected: 0x%02x)\n", idValue, CH8398_ID_VALUE);
    }

    return found;
}

BOOL InitCH8398(struct BoardInfo *bi)
{
    REGBASE();

    /*
     * Initialize CH8398 RAMDAC
     * The CH8398 has programmable PLLs and control registers
     * For basic operation, we may need to set up:
     * - Control Register (CR) for display modes
     * - Clock Select Register (CSR) for PLL selection
     * - Auxiliary Register (AUX) for clock output levels
     */

    // CH8398 initialization would go here
    // This is a placeholder - actual initialization would:
    // 1. Set up Control Register for desired display modes
    // 2. Program PLL RAM entries if needed
    // 3. Set Clock Select Register
    // 4. Configure Auxiliary Register if needed

    DFUNC(0, "CH8398 RAMDAC initialized\n");

    W_REG(DAC_MASK, 0xFF);

    return TRUE;
}

// void writeCWA(struct BoardInfo *bi, UBYTE value)
// {
//     REGBASE();

//     writeReg(DAC_WR_AD)

// }

ULONG SetMemoryClockCH8398(struct BoardInfo *bi, ULONG clockHz)
{
    REGBASE();

    /*
     * Set memory clock using CH8398 PLL
     * Performs binary search in PLL table to find closest match
     * Programs the MPLL (Memory PLL) in the CH8398
     */

    // Convert clockHz to 10kHz units for comparison
    ULONG targetFreq10khz = clockHz / 10000;

    DFUNC(INFO, "SetMemoryClock: requested %ld Hz (%ld * 10kHz)\n", clockHz, targetFreq10khz);

    // Binary search for closest frequency
    UWORD left     = 0;
    UWORD right    = CH8398_PLL_TABLE_SIZE - 1;
    UWORD best     = 0;
    ULONG bestDiff = 0xFFFFFFFF;

    while (left <= right) {
        UWORD mid     = (left + right) / 2;
        ULONG midFreq = ch8398_pll_table[mid].freq10khz;

        ULONG diff = (midFreq > targetFreq10khz) ? (midFreq - targetFreq10khz) : (targetFreq10khz - midFreq);

        if (diff < bestDiff) {
            bestDiff = diff;
            best     = mid;
        }

        if (midFreq == targetFreq10khz) {
            best = mid;
            break;
        } else if (midFreq < targetFreq10khz) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    const CH8398_PLLValue_t *pllValue = &ch8398_pll_table[best];
    ULONG actualFreqHz                = pllValue->freq10khz * 10000;

    DFUNC(INFO, "SetMemoryClock: selected entry %ld: %ld Hz (M=%ld, N=%ld, K=%ld)\n", (ULONG)best, actualFreqHz,
          (ULONG)pllValue->M, (ULONG)pllValue->N, (ULONG)pllValue->K);

    // Save CRTC 0x31 to control RS2 via CS3
    UBYTE saveCR31 = R_CR(0x31);

    // Set CS3=0 to make RS2=0 for accessing CWA and CDR
    // RS[3:0] = 1000 = CWA (Clock RAM Write Address)
    // RS[3:0] = 1001 = CDR (Clock RAM Data Register)
    // RS3 = 1 (always, from +5V)
    // RS2 = 0 (from CS3=0, clear bit 6 of CRTC 0x31)
    W_CR_MASK(0x31, 0x40, 0x00);  // Clear bit 6 to set CS3=0 (RS2=0)

    // Access CWA (RS[3:0] = 1000)
    // RS[1:0] = 00, so use 0x3C8 (DAC Write Address)
    // RS2 = 0 (from CS3=0)
    // RS3 = 1 (always, from +5V)
    // MPLL starts at address 0x10 (16 locations, 32 bytes, after VPLL's 32 bytes)
    // For now, we'll program MPLL location 0 (address 0x10)
    W_REG(CWA, 0x10);  // Set CWA to MPLL location 0

    // Pack M, N, K into PLL data format:
    // LSB (first byte): N7, N6, N5, N4, N3, N2, N1, N0
    // MSB (second byte): K1, K0, M5, M4, M3, M2, M1, M0
    UBYTE pllLSB = pllValue->N;                                // N value (LSB)
    UBYTE pllMSB = (pllValue->K << 6) | (pllValue->M & 0x3F);  // M and K (MSB)

    // Access CDR (RS[3:0] = 1001)
    // RS[1:0] = 01, so use 0x3C9 (DAC Data)
    // RS2 = 0 (from CS3=0)
    // RS3 = 1 (always, from +5V)
    // CH8398 PLL RAM requires LSB first, then MSB
    W_REG(CDR, pllLSB);  // Write LSB first (N)
    W_REG(CDR, pllMSB);  // Write MSB second (M and K)

    // Restore CRTC 0x31
    W_CR(0x31, saveCR31);

    DFUNC(INFO, "SetMemoryClock: programmed MPLL to %ld Hz\n", actualFreqHz);

    return actualFreqHz;
}
