#ifndef  CLIB_OPENPCI_PROTOS_H
#define  CLIB_OPENPCI_PROTOS_H

#ifndef  EXEC_TYPES_H
#include <exec/types.h>
#endif
#ifndef  EXEC_INTERRUPTS_H
#include <exec/interrupts.h>
#endif
#ifndef  UTILITY_TAGITEM_H
#include <utility/tagitem.h>
#endif
#ifndef  UTILITY_HOOKS_H
#include <utility/hooks.h>
#endif
#ifndef  DEVICES_TIMER_H
#include <devices/timer.h>
#endif

/* Return the bus type found */
UWORD pci_bus(void);

/* Pci Find Functions */
struct pci_dev *pci_find_device( unsigned short vendor, unsigned short device, struct pci_dev *pcidev);
struct pci_dev *pci_find_class( unsigned long devclass,  struct  pci_dev *pcidev);
struct pci_dev *pci_find_slot( unsigned char bus,  unsigned long devfn);

/* Pci Bus Read/Write Config functions */
UBYTE pci_read_config_byte(UBYTE reg_offset,struct pci_dev *pcidev);
UWORD pci_read_config_word(UBYTE reg_offset,struct pci_dev *pcidev);
ULONG pci_read_config_long(UBYTE reg_offset,struct pci_dev *pcidev);
void pci_write_config_byte(UBYTE reg_offset,UBYTE val,struct pci_dev *pcidev);
void pci_write_config_word(UBYTE reg_offset,UWORD val,struct pci_dev *pcidev);
void pci_write_config_long(UBYTE reg_offset,ULONG val,struct pci_dev *pcidev);

/* Pci Set Bus Master */
BOOL pci_set_master(struct pci_dev *pcidev);

/* Interrupt functions */
BOOL pci_add_intserver(struct Interrupt *PciInterrupt, struct pci_dev *pcidev);
void pci_rem_intserver(struct Interrupt *PciInterrupt, struct pci_dev *pcidev);

/* Alloc/Free DMA memory */
APTR pci_allocdma_mem(ULONG size,ULONG flags);
void pci_freedma_mem(APTR buffer,ULONG size);

/* Logic<->Physic Addr conversion */
APTR pci_logic_to_physic_addr(APTR PciLogicalAddr, struct pci_dev *pcidev);
APTR pci_physic_to_logic_addr(APTR PciPhysicalAddr, struct pci_dev *pcidev);

/* Obtain/Release PCI card */
BOOL pci_obtain_card(struct pci_dev *pcidev);
void pci_release_card(struct pci_dev *pcidev);

/* New functions in rev.3 */
struct pci_dev *FindBoardA(struct pci_dev *previous,struct TagItem *tags);
struct pci_dev *FindBoard(struct pci_dev *previous,...);

ULONG GetBoardAttrsA(struct pci_dev *pcidev,struct TagItem *tags);
ULONG GetBoardAttrs(struct pci_dev *pcidev,...);

BOOL SetBoardAttrsA(struct pci_dev *pcidev,struct TagItem *tags);
BOOL SetBoardAttrs(struct pci_dev *pcidev,...);

APTR AllocateDMAMemoryForBoard(struct pci_dev *pcidev,ULONG size,ULONG flags);
void ReleaseDMAMemoryForBoard(struct pci_dev *pcidev,APTR mem,ULONG size);

BOOL AddMemoryHandlerForBoard(struct pci_dev *pcidev,struct Hook *hk,BYTE pri);
BOOL RemMemoryHandlerForBoard(struct pci_dev *pcidev,struct Hook *hk);
#endif	 
