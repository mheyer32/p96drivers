	IFND LIBRARIES_PCITAGS_I
LIBRARIES_PCITAGS_I	SET	1

**
**	$VER: pcitags.h 3.0 (30.08.2024)
**	Includes Release 3.0
**
**	openpci.library tags for GetBoardAttrs/GetBoardAttrs/FindBoardTagList
**      the tags defined here are intentionally identical to those of the
**      prometheus.library.
**

*******************************************************************************

* Tags for Prm_FindBoardTagList(), Prm_GetBoardAttrsTagList() *
* and Prm_SetBoardAttrsTagList().                             *
* 'S' - the attribute is settable.                            *
* 'G' - the attribute is gettable.                            *

PRM_Vendor		EQU     $6EDA0000    * [.G] *
PRM_Device            	EQU	$6EDA0001    * [.G] *
PRM_Revision          	EQU	$6EDA0002    * [.G] *
PRM_Class             	EQU	$6EDA0003    * [.G] *
PRM_SubClass          	EQU	$6EDA0004    * [.G] *

* You can depend on fact that the last nybble of PRM_MemoryAddrX and  *
* PRM_MemorySizeX is equal to X. It is guarranted in future releases. *

PRM_MemoryAddr0       	EQU	$6EDA0010    * [.G] *
PRM_MemoryAddr1       	EQU	$6EDA0011    * [.G] *
PRM_MemoryAddr2       	EQU	$6EDA0012    * [.G] *
PRM_MemoryAddr3       	EQU	$6EDA0013    * [.G] *
PRM_MemoryAddr4       	EQU	$6EDA0014    * [.G] *
PRM_MemoryAddr5       	EQU	$6EDA0015    * [.G] *
PRM_ROM_Address       	EQU	$6EDA0016    * [.G] *

PRM_MemorySize0       	EQU	$6EDA0020    * [.G] *
PRM_MemorySize1       	EQU	$6EDA0021    * [.G] *
PRM_MemorySize2       	EQU	$6EDA0022    * [.G] *
PRM_MemorySize3       	EQU	$6EDA0023    * [.G] *
PRM_MemorySize4       	EQU	$6EDA0024    * [.G] *
PRM_MemorySize5       	EQU	$6EDA0025    * [.G] *
PRM_ROM_Size          	EQU	$6EDA0026    * [.G] *

PRM_MemoryFlags0       	EQU	$6EDA0030    * [.G] *
PRM_MemoryFlags1       	EQU	$6EDA0031    * [.G] *
PRM_MemoryFlags2       	EQU	$6EDA0032    * [.G] *
PRM_MemoryFlags3       	EQU	$6EDA0033    * [.G] *
PRM_MemoryFlags4       	EQU	$6EDA0034    * [.G] *
PRM_MemoryFlags5       	EQU	$6EDA0035    * [.G] *
PRM_ROM_Flags          	EQU	$6EDA0036    * [.G] *

PRM_BoardOwner        	EQU	$6EDA0005    * [SG] *
PRM_SlotNumber        	EQU	$6EDA0006    * [.G] *
PRM_FunctionNumber    	EQU	$6EDA0007    * [.G] *
PRM_BusNumber	      	EQU	$6EDA0008    * [.G] *

PRM_HeaderType        	EQU	$6EDA0009    * [.G] *
PRM_SubsysVendor      	EQU	$6EDA000A    * [.G] *
PRM_SubsysID          	EQU	$6EDA000B    * [.G] *
PRM_Interface         	EQU	$6EDA000C    * [.G] *
PRM_InterruptPin	EQU	$6EDA000D    * [.G] *
PRM_LegacyIOSpace	EQU	$6EDA000E    * [.G] *
PRM_PCIToHostOffset	EQU	$6EDA000F    * [.G] *
PRM_InterruptLine	EQU	$6EDA001D    * [.G] *
PRM_ConfigArea		EQU	$6EDA001E    * [.G] *

** The following tags are only for FindBoardTagList() to find a board
** whose memory is mapped into the given area.
   
PRM_MemRangeLower       EQU     $6EDA1000     * [..], find only *
PRM_MemRangeUpper       EQU     $6EDA1001     * [..], find only *

** The following tags contain the full PCI address range (as PCI
** addresses, not 68K addresses) that contain the memory region
** of the device, or during config time, the entire PCI region
** during config time for init tools.
** Use PRM_PCIToHostOffset to convert this to a 68K address.

PRM_PCIMemWindowLow	EQU	$6EDA0050    * [.G] *
PRM_PCIMemWindowHigh	EQU	$6EDA0051    * [.G] *

** The IO region always starts at PCI address 0, and at 68K address
** PRM_LegacyIOSpace. It is always 64K in size.

	ENDC			;LIBRARIES_PCITAGS_I


