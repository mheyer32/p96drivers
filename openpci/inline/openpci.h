/* Automatically generated header (sfdc 1.11e)! Do not edit! */

#ifndef _INLINE_OPENPCI_H
#define _INLINE_OPENPCI_H

#ifndef _SFDC_VARARG_DEFINED
#define _SFDC_VARARG_DEFINED
#ifdef __HAVE_IPTR_ATTR__
typedef APTR _sfdc_vararg __attribute__((iptr));
#else
typedef ULONG _sfdc_vararg;
#endif /* __HAVE_IPTR_ATTR__ */
#endif /* _SFDC_VARARG_DEFINED */

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif /* !__INLINE_MACROS_H */

#ifndef OPENPCI_BASE_NAME
#define OPENPCI_BASE_NAME OpenPciBase
#endif /* !OPENPCI_BASE_NAME */

#define pci_bus() \
      LP0(0x1e, UWORD, pci_bus ,\
      , OPENPCI_BASE_NAME)

#define pci_find_device(___vendor, ___device, ___pcidev) \
      LP3(0x5a, struct pci_dev *, pci_find_device , unsigned short, ___vendor, d0, unsigned short, ___device, d1, struct pci_dev *, ___pcidev, a0,\
      , OPENPCI_BASE_NAME)

#define pci_find_class(___devclass, ___pcidev) \
      LP2(0x60, struct pci_dev *, pci_find_class , unsigned long, ___devclass, d0, struct pci_dev *, ___pcidev, a0,\
      , OPENPCI_BASE_NAME)

#define pci_find_slot(___bus, ___devfn) \
      LP2(0x66, struct pci_dev *, pci_find_slot , unsigned char, ___bus, d0, unsigned long, ___devfn, d1,\
      , OPENPCI_BASE_NAME)

#define pci_read_config_byte(___registernum, ___pcidev) \
      LP2(0x6c, UBYTE, pci_read_config_byte , UBYTE, ___registernum, d0, struct pci_dev *, ___pcidev, a0,\
      , OPENPCI_BASE_NAME)

#define pci_read_config_word(___registernum, ___pcidev) \
      LP2(0x72, UWORD, pci_read_config_word , UBYTE, ___registernum, d0, struct pci_dev *, ___pcidev, a0,\
      , OPENPCI_BASE_NAME)

#define pci_read_config_long(___registernum, ___pcidev) \
      LP2(0x78, ULONG, pci_read_config_long , UBYTE, ___registernum, d0, struct pci_dev *, ___pcidev, a0,\
      , OPENPCI_BASE_NAME)

#define pci_write_config_byte(___registernum, ___val, ___pcidev) \
      LP3NR(0x7e, pci_write_config_byte , UBYTE, ___registernum, d0, UBYTE, ___val, d1, struct pci_dev *, ___pcidev, a0,\
      , OPENPCI_BASE_NAME)

#define pci_write_config_word(___registernum, ___val, ___pcidev) \
      LP3NR(0x84, pci_write_config_word , UBYTE, ___registernum, d0, UWORD, ___val, d1, struct pci_dev *, ___pcidev, a0,\
      , OPENPCI_BASE_NAME)

#define pci_write_config_long(___registernum, ___val, ___pcidev) \
      LP3NR(0x8a, pci_write_config_long , UBYTE, ___registernum, d0, ULONG, ___val, d1, struct pci_dev *, ___pcidev, a0,\
      , OPENPCI_BASE_NAME)

#define pci_set_master(___pcidev) \
      LP1(0x90, BOOL, pci_set_master , struct pci_dev *, ___pcidev, a0,\
      , OPENPCI_BASE_NAME)

#define pci_add_intserver(___PciInterrupt, ___pcidev) \
      LP2(0x96, BOOL, pci_add_intserver , struct Interrupt *, ___PciInterrupt, a0, struct pci_dev *, ___pcidev, a1,\
      , OPENPCI_BASE_NAME)

#define pci_rem_intserver(___PciInterrupt, ___pcidev) \
      LP2NR(0x9c, pci_rem_intserver , struct Interrupt *, ___PciInterrupt, a0, struct pci_dev *, ___pcidev, a1,\
      , OPENPCI_BASE_NAME)

#define pci_allocdma_mem(___size, ___flags) \
      LP2(0xa2, APTR, pci_allocdma_mem , ULONG, ___size, d0, ULONG, ___flags, d1,\
      , OPENPCI_BASE_NAME)

#define pci_freedma_mem(___buffer, ___size) \
      LP2NR(0xa8, pci_freedma_mem , APTR, ___buffer, a0, ULONG, ___size, d0,\
      , OPENPCI_BASE_NAME)

#define pci_logic_to_physic_addr(___PciLogicalAddr, ___pcidev) \
      LP2(0xae, APTR, pci_logic_to_physic_addr , APTR, ___PciLogicalAddr, a0, struct pci_dev *, ___pcidev, a1,\
      , OPENPCI_BASE_NAME)

#define pci_physic_to_logic_addr(___PciPhysicalAddr, ___pcidev) \
      LP2(0xb4, APTR, pci_physic_to_logic_addr , APTR, ___PciPhysicalAddr, a0, struct pci_dev *, ___pcidev, a1,\
      , OPENPCI_BASE_NAME)

#define pci_obtain_card(___pcidev) \
      LP1(0xba, BOOL, pci_obtain_card , struct pci_dev *, ___pcidev, a0,\
      , OPENPCI_BASE_NAME)

#define pci_release_card(___pcidev) \
      LP1NR(0xc0, pci_release_card , struct pci_dev *, ___pcidev, a0,\
      , OPENPCI_BASE_NAME)

#define FindBoardA(___board, ___tags) \
      LP2(0xcc, struct pci_dev *, FindBoardA , struct pci_dev *, ___board, a0, struct TagItem *, ___tags, a1,\
      , OPENPCI_BASE_NAME)

#ifndef NO_INLINE_STDARG
#define FindBoard(___board, ___tags, ...) \
    ({_sfdc_vararg _tags[] = { ___tags, __VA_ARGS__ }; FindBoardA((___board), (struct TagItem *) _tags); })
#endif /* !NO_INLINE_STDARG */

#define GetBoardAttrsA(___board, ___tags) \
      LP2(0xd2, ULONG, GetBoardAttrsA , struct pci_dev *, ___board, a0, struct TagItem *, ___tags, a1,\
      , OPENPCI_BASE_NAME)

#ifndef NO_INLINE_STDARG
#define GetBoardAttrs(___board, ___tags, ...) \
    ({_sfdc_vararg _tags[] = { ___tags, __VA_ARGS__ }; GetBoardAttrsA((___board), (struct TagItem *) _tags); })
#endif /* !NO_INLINE_STDARG */

#define SetBoardAttrsA(___board, ___tags) \
      LP2(0xd8, BOOL, SetBoardAttrsA , struct pci_dev *, ___board, a0, struct TagItem *, ___tags, a1,\
      , OPENPCI_BASE_NAME)

#ifndef NO_INLINE_STDARG
#define SetBoardAttrs(___board, ___tags, ...) \
    ({_sfdc_vararg _tags[] = { ___tags, __VA_ARGS__ }; SetBoardAttrsA((___board), (struct TagItem *) _tags); })
#endif /* !NO_INLINE_STDARG */

#define AllocateDMAMemoryForBoard(___board, ___size, ___flags) \
      LP3(0xde, APTR, AllocateDMAMemoryForBoard , struct pci_dev *, ___board, a0, ULONG, ___size, d0, ULONG, ___flags, d1,\
      , OPENPCI_BASE_NAME)

#define ReleaseDMAMemoryForBoard(___board, ___addr, ___size) \
      LP3NR(0xe4, ReleaseDMAMemoryForBoard , struct pci_dev *, ___board, a0, APTR, ___addr, a1, ULONG, ___size, d0,\
      , OPENPCI_BASE_NAME)

#define AddMemoryHandlerForBoard(___board, ___hook, ___pri) \
      LP3(0xea, BOOL, AddMemoryHandlerForBoard , struct pci_dev *, ___board, a0, struct Hook *, ___hook, a1, BYTE, ___pri, d0,\
      , OPENPCI_BASE_NAME)

#define RemMemoryHandlerForBoard(___board, ___hook) \
      LP2(0xf0, BOOL, RemMemoryHandlerForBoard , struct pci_dev *, ___board, a0, struct Hook *, ___hook, a1,\
      , OPENPCI_BASE_NAME)

#endif /* !_INLINE_OPENPCI_H */
