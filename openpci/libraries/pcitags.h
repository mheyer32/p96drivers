#ifndef LIBRARIES_PCITAGS_H
#define LIBRARIES_PCITAGS_H

/*
**	$VER: pcitags.h 3.0 (30.08.2024)
**	Includes Release 3.0
**
**	openpci.library tags for GetBoardAttrs/GetBoardAttrs/FindBoardTagList
**      the tags defined here are intentionally identical to those of the
**      prometheus.library.
*/

/*****************************************************************************/

/* Tags for Prm_FindBoardTagList(), Prm_GetBoardAttrsTagList() */
/* and Prm_SetBoardAttrsTagList().                             */
/* 'S' - the attribute is settable.                            */
/* 'G' - the attribute is gettable.                            */

#define PRM_Vendor            0x6EDA0000    /* [.G] */
#define PRM_Device            0x6EDA0001    /* [.G] */
#define PRM_Revision          0x6EDA0002    /* [.G] */
#define PRM_Class             0x6EDA0003    /* [.G] */
#define PRM_SubClass          0x6EDA0004    /* [.G] */

/* You can depend on fact that the last nybble of PRM_MemoryAddrX and  */
/* PRM_MemorySizeX is equal to X. It is guaranted in future releases. */

#define PRM_MemoryAddr0       0x6EDA0010    /* [.G] */
#define PRM_MemoryAddr1       0x6EDA0011    /* [.G] */
#define PRM_MemoryAddr2       0x6EDA0012    /* [.G] */
#define PRM_MemoryAddr3       0x6EDA0013    /* [.G] */
#define PRM_MemoryAddr4       0x6EDA0014    /* [.G] */
#define PRM_MemoryAddr5       0x6EDA0015    /* [.G] */
#define PRM_ROM_Address       0x6EDA0016    /* [.G] */

#define PRM_MemorySize0       0x6EDA0020    /* [.G] */
#define PRM_MemorySize1       0x6EDA0021    /* [.G] */
#define PRM_MemorySize2       0x6EDA0022    /* [.G] */
#define PRM_MemorySize3       0x6EDA0023    /* [.G] */
#define PRM_MemorySize4       0x6EDA0024    /* [.G] */
#define PRM_MemorySize5       0x6EDA0025    /* [.G] */
#define PRM_ROM_Size          0x6EDA0026    /* [.G] */

#define PRM_MemoryFlags0      0x6EDA0030    /* [.G] */
#define PRM_MemoryFlags1      0x6EDA0031    /* [.G] */
#define PRM_MemoryFlags2      0x6EDA0032    /* [.G] */
#define PRM_MemoryFlags3      0x6EDA0033    /* [.G] */
#define PRM_MemoryFlags4      0x6EDA0034    /* [.G] */
#define PRM_MemoryFlags5      0x6EDA0035    /* [.G] */
#define PRM_ROM_Flags         0x6EDA0036    /* [.G] */

#define PRM_BoardOwner        0x6EDA0005    /* [SG] */
#define PRM_SlotNumber        0x6EDA0006    /* [.G] */
#define PRM_FunctionNumber    0x6EDA0007    /* [.G] */
#define PRM_BusNumber	      0x6EDA0008    /* [.G] */

#define PRM_HeaderType        0x6EDA0009    /* [.G] */
#define PRM_SubsysVendor      0x6EDA000A    /* [.G] */
#define PRM_SubsysID          0x6EDA000B    /* [.G] */
#define PRM_Interface         0x6EDA000C    /* [.G] */
#define PRM_InterruptPin      0x6EDA000D    /* [.G] */
#define PRM_LegacyIOSpace     0x6EDA000E    /* [.G] */
#define PRM_PCIToHostOffset   0x6EDA000F    /* [.G] */
#define PRM_InterruptLine     0x6EDA001D    /* [.G] */
#define PRM_ConfigArea        0x6EDA001E    /* [.G] */

/*
** The following tags are only for FindBoardTagList() to find a board
** whose memory is mapped into the given area.
*/
#define PRM_MemRangeLower     0x6EDA1000     /* [..], find only */
#define PRM_MemRangeUpper     0x6EDA1001     /* [..], find only */

/*
** The following tags contain the full PCI address range (as PCI
** addresses, not 68K addresses) that contain the memory region
** of the device, or during config time, the entire PCI region
** during config time for init tools.
** Use PRM_PCIToHostOffset to convert this to a 68K address.
*/

#define PRM_PCIMemWindowLow   0x6EDA0050    /* [.G] */
#define PRM_PCIMemWindowHigh  0x6EDA0051    /* [.G] */

/*
** The IO region always starts at PCI address 0, and at 68K address
** PRM_LegacyIOSpace. It is always 64K in size.
*/

#endif /* LIBRARIES_PCITAGS_H */


