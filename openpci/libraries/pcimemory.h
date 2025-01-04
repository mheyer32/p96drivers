#ifndef LIBRARIES_PCITAGS_H
#define LIBRARIES_PCITAGS_H

/*
**	$VER: pcimemory.h 3.0 (31.08.2024)
**	Includes Release 3.0
**
**	openpci.library structures for memory allocation
**      and release on the PCI bus.
*/

/*****************************************************************************/


/*
** This structure is used by the openpci.library as message
** argument to the DMA memory hook to to allocate DMA memory,
** that is, if the hook object is 'ALOC'
*/

struct AllocationInfo {
  ULONG  am_bytesize;  /* number of bytes to allocate */
  ULONG  am_reqments;  /* memory flags, see openpci.h */
  UBYTE *am_lower;     /* lower end of PCI bridge memory window */
  UBYTE *am_upper;     /* upper end of PCI bridge memory window */
};

/* am_lower and am_higher are logical 68K addresses, susceptible to
** MMU translation and translation of the PCI bridge.
*/


/*
** This structure is used by the openpci.library as message
** argument to the DMA memory hook to release memory, that
** is, if the hook object is 'RELA'
*/

struct ReleaseInfo {
  ULONG  ar_size;
  UBYTE *ar_mem;
};

#endif /* LIBRARIES_PCIMEMORY_H */


