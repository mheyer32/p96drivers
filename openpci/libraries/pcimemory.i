	IFND LIBRARIES_PCIMEMORY_I
LIBRARIES_PCIMEMORY	SET	1

**
**	$VER: pcimemory.h 3.0 (31.08.2024)
**	Includes Release 3.0
**
**	openpci.library structures for memory allocation
**      and release on the PCI bus.
**

*******************************************************************************

	IFND EXEC_TYPES_I
	INCLUDE "exec/types.i"
	ENDC
**
** This structure is used by the openpci.library as message
** argument to the DMA memory hook to to allocate DMA memory,
** that is, if the hook object is 'ALOC'
**

  STRUCTURE AllocationInfo, 0
	ULONG	am_bytesize		** number of bytes to allocate
	ULONG	am_reqments		** memory flags, see openpci.i
	APTR	am_lower		** lower end of PCI bridge memory window
	UBYTE	am_upper		** upper end of PCI bridge memory window
	LABEL	am_SIZEOF

** am_lower and am_higher are logical 68K addresses, susceptible to
** MMU translation and translation of the PCI bridge.
**


**
** This structure is used by the openpci.library as message
** argument to the DMA memory hook to release memory, that
** is, if the hook object is 'RELA'
**
  STRUCTURE ReleaseInfo,0
	ULONG  	ar_size
	APTR	ar_mem
	LABEL	ar_SIZEOF


	ENDC			; LIBRARIES_PCIMEMORY_I


