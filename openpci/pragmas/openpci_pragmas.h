#ifndef _INCLUDE_PRAGMA_OPENPCI_LIB_H
#define _INCLUDE_PRAGMA_OPENPCI_LIB_H

#ifndef CLIB_OPENPCI_PROTOS_H
#include <clib/openpci_protos.h>
#endif

#if defined(AZTEC_C) || defined(__MAXON__) || defined(__STORM__)
#pragma amicall(OpenPciBase,0x01e,pci_bus())
#pragma amicall(OpenPciBase,0x05a,pci_find_device(d0,d1,a0))
#pragma amicall(OpenPciBase,0x060,pci_find_class(d0,a0))
#pragma amicall(OpenPciBase,0x066,pci_find_slot(d0,d1))
#pragma amicall(OpenPciBase,0x06c,pci_read_config_byte(d0,a0))
#pragma amicall(OpenPciBase,0x072,pci_read_config_word(d0,a0))
#pragma amicall(OpenPciBase,0x078,pci_read_config_long(d0,a0))
#pragma amicall(OpenPciBase,0x07e,pci_write_config_byte(d0,d1,a0))
#pragma amicall(OpenPciBase,0x084,pci_write_config_word(d0,d1,a0))
#pragma amicall(OpenPciBase,0x08a,pci_write_config_long(d0,d1,a0))
#pragma amicall(OpenPciBase,0x090,pci_set_master(a0))
#pragma amicall(OpenPciBase,0x096,pci_add_intserver(a0,a1))
#pragma amicall(OpenPciBase,0x09c,pci_rem_intserver(a0,a1))
#pragma amicall(OpenPciBase,0x0a2,pci_allocdma_mem(d0,d1))
#pragma amicall(OpenPciBase,0x0a8,pci_freedma_mem(a0,d0))
#pragma amicall(OpenPciBase,0x0ae,pci_logic_to_physic_addr(a0,a1))
#pragma amicall(OpenPciBase,0x0b4,pci_physic_to_logic_addr(a0,a1))
#pragma amicall(OpenPciBase,0x0ba,pci_obtain_card(a0))
#pragma amicall(OpenPciBase,0x0c0,pci_release_card(a0))
#pragma amicall(OpenPciBase,0x0cc,FindBoardA(a0,a1))
#pragma amicall(OpenPciBase,0x0d2,GetBoardAttrsA(a0,a1))
#pragma amicall(OpenPciBase,0x0d8,SetBoardAttrsA(a0,a1))
#pragma amicall(OpenPciBase,0x0de,AllocateDMAMemoryForBoard(a0,d0,d1))
#pragma amicall(OpenPciBase,0x0e4,ReleaseDMAMemoryForBoard(a0,a1,d0))
#pragma amicall(OpenPciBase,0x0ea,AddMemoryHandlerForBoard(a0,a1,d0))
#pragma amicall(OpenPciBase,0x0f0,RemMemoryHandlerForBoard(a0,a1))
#endif
#if defined(_DCC) || defined(__SASC)
#pragma  libcall OpenPciBase pci_bus                01e 00
#pragma  libcall OpenPciBase pci_find_device        05a 81003
#pragma  libcall OpenPciBase pci_find_class         060 8002
#pragma  libcall OpenPciBase pci_find_slot          066 1002
#pragma  libcall OpenPciBase pci_read_config_byte   06c 8002
#pragma  libcall OpenPciBase pci_read_config_word   072 8002
#pragma  libcall OpenPciBase pci_read_config_long   078 8002
#pragma  libcall OpenPciBase pci_write_config_byte  07e 81003
#pragma  libcall OpenPciBase pci_write_config_word  084 81003
#pragma  libcall OpenPciBase pci_write_config_long  08a 81003
#pragma  libcall OpenPciBase pci_set_master         090 801
#pragma  libcall OpenPciBase pci_add_intserver      096 9802
#pragma  libcall OpenPciBase pci_rem_intserver      09c 9802
#pragma  libcall OpenPciBase pci_allocdma_mem       0a2 1002
#pragma  libcall OpenPciBase pci_freedma_mem        0a8 0802
#pragma  libcall OpenPciBase pci_logic_to_physic_addr 0ae 9802
#pragma  libcall OpenPciBase pci_physic_to_logic_addr 0b4 9802
#pragma  libcall OpenPciBase pci_obtain_card        0ba 801
#pragma  libcall OpenPciBase pci_release_card       0c0 801
#pragma  libcall OpenPciBase FindBoardA       0cc 9802
#pragma  libcall OpenPciBase GetBoardAttrsA         0d2 9802
#pragma  libcall OpenPciBase SetBoardAttrsA         0d8 9802
#pragma  libcall OpenPciBase AllocateDMAMemoryForBoard 0de 10803
#pragma  libcall OpenPciBase ReleaseDMAMemoryForBoard 0e4 09803
#pragma  libcall OpenPciBase AddMemoryHandlerForBoard 0ea 09803
#pragma  libcall OpenPciBase RemMemoryHandlerForBoard 0f0 9802
#endif
#ifdef __STORM__
#pragma tagcall(OpenPciBase,0x0cc,FindBoard(a0,a1))
#pragma tagcall(OpenPciBase,0x0d2,GetBoardAttrs(a0,a1))
#pragma tagcall(OpenPciBase,0x0d8,SetBoardAttrs(a0,a1))
#endif
#ifdef __SASC_60
#pragma  tagcall OpenPciBase FindBoard          0cc 9802
#pragma  tagcall OpenPciBase GetBoardAttrs          0d2 9802
#pragma  tagcall OpenPciBase SetBoardAttrs          0d8 9802
#endif

#endif	/*  _INCLUDE_PRAGMA_OPENPCI_LIB_H  */
