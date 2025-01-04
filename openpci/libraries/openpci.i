	IFND LIBRARIES_OPENPCI_I
LIBRARIES_OPENPCI_I	SET	1

**
**	$VER: openpci.h 3.0 (30.08.2024)
**	Includes Release 3.0
**
**	openpci.library interface structures and definitions.
**
**

*******************************************************************************

	IFND EXEC_TYPES_I
	INCLUDE "exec/types.i"
	ENDC
** pci_bus() result flags
** Multiple flags may be set if multiple boards are
** found.
**

MediatorA1200Bus	EQU	$1
MediatorZ4Bus		EQU	$2
PrometheusBus		EQU	$4
GrexA1200Bus		EQU	$8
GrexA4000Bus		EQU	$10
PegasosBus		EQU	$20
PowerPciBus		EQU	$40

** pci_allocdma_mem flags **
	
MEM_PCI			EQU	$1  	** on the PCI bus, always implied **
MEM_NONCACHEABLE	EQU	$2  	** target memory must not be cachable for 68K **

MIN_OPENPCI_VERSION 	EQU	3 	** Version 3 or more **

*******************************************************************************

  STRUCTURE pci_dev,0
	APTR	pci_bus			** this is not filled nor used 				**
	APTR	pci_next		** Next pci_dev, or NULL. NOT an exec style list	**
	APTR	pci_pred		** Previous pci_dev, or NULL. NOT an exec style list	**
  
	UBYTE	pci_devfn		** encoded device & function index, (device << 3) | function **
	UBYTE	pci_kludgefill		** not used, stricly for WORD alignment			**
	UWORD	pci_vendor		** PCI vendor ID 					**
	UWORD	pci_device		** PCI device ID 					**
	ULONG   pci_devclass		** 3 bytes: (base<<16,sub<<8,prog-if<<0) 		**
	ULONG	pci_hdr_type		** PCI header type					**
	UWORD	pci_master		** actually, a collection of flags, see below		**
  **
  ** In theory, the irq level can be read from configuration
  ** space and all would be fine.  However, old PCI chips don't
  ** support these registers and return 0 instead.  For example,
  ** the Vision864-P rev 0 chip can uses INTA, but returns 0 in
  ** the interrupt line and pin registers.  pci_init()
  ** initializes this field with the value at PCI_INTERRUPT_LINE
  ** and it is the job of pcibios_fixup() to change it if
  ** necessary.  The field must not be 0 unless the device
  ** cannot generate interrupts at all.
  **
	ULONG	pci_irq			** this is actually the IRQ line as found in the config area 	**
  ** Base registers for this device
	STRUCT	pci_base_address,6*4	** these should actually be APTRs, they are 68K addresses	**
	STRUCT	pci_base_size,6*4	** one's complement of the size, actually, and some flags	**
	ULONG	pci_rom_address		** 68K address of where the ROM is mapped, or NULL 		**
	ULONG	pci_rom_size		** one's complemenet of the size, plus flags			**
	APTR	pci_reserved		** never ever used						**
  ** the following is new in version 3
	APTR	pci_legacy_io		** 68K pointer to legacy IO space				**
	LABEL	pci_SIZEOF

**
** the pci_bus structure is not anymore supported, the library
** flattens the PCI hierarchy (if any)
**


**
** Under PCI, each device has 256 bytes of configuration address space,
** of which the first 64 bytes are standardized as follows:
**
PCI_VENDOR_ID			EQU	$00	** 16 bits
PCI_DEVICE_ID           	EQU	$02     ** 16 bits
PCI_COMMAND			EQU	$04     ** 16 bits
PCI_COMMAND_IO			EQU	$01	** Enable response in I/O space
PCI_COMMAND_MEMORY		EQU	$02	** Enable response in Memory space
PCI_COMMAND_MASTER 		EQU	$04	** Enable bus mastering
PCI_COMMAND_SPECIAL		EQU	$08	** Enable response to special cycles
PCI_COMMAND_INVALIDATE 		EQU	$10     ** Use memory write and invalidate 
PCI_COMMAND_VGA_PALETTE		EQU	$20    	** Enable palette snooping 
PCI_COMMAND_PARITY		EQU     $40     ** Enable parity checking 
PCI_COMMAND_WAIT 		EQU     $80     ** Enable address/data stepping 
PCI_COMMAND_SERR		EQU     $100    ** Enable SERR 
PCI_COMMAND_FAST_BACK		EQU  	$200    ** Enable back-to-back writes 

PCI_STATUS              	EQU 	$06     ** 16 bits 
** Bit 3..0:    reserved 
PCI_NEW_CAPABILITIES		EQU	$10	** Support New Capabilities Linked List (PCI 2.2) 
PCI_STATUS_66MHZ		EQU     $20     ** Support 66 Mhz PCI 2.1 bus 
PCI_STATUS_UDF			EQU	$40     ** Support User Definable Features 
PCI_STATUS_FAST_BACK		EQU	$80     ** Accept fast-back to back 
PCI_STATUS_PARITY		EQU	$100    ** Detected parity error 
PCI_STATUS_DEVSEL_MASK		EQU	$600    ** DEVSEL timing 
PCI_STATUS_DEVSEL_FAST		EQU	$000   
PCI_STATUS_DEVSEL_MEDIUM 	EQU 	$200
PCI_STATUS_DEVSEL_SLOW		EQU	$400
** PCI Errors detected 
PCI_STATUS_SIG_TARGET_ABORT	EQU 	$800  	** Set on target abort 
PCI_STATUS_REC_TARGET_ABORT	EQU 	$1000  	** Master ack
PCI_STATUS_REC_MASTER_ABORT	EQU 	$2000  	** Set on master abort 
PCI_STATUS_SIG_SYSTEM_ERROR	EQU 	$4000  	** Set when we drive SERR 
PCI_STATUS_DETECTED_PARITY	EQU	$8000  	** Set on parity error 
PCI_STATUS_ERRBITS 		EQU    	(PCI_STATUS_SIG_TARGET_ABORT|PCI_STATUS_REC_TARGET_ABORT|PCI_STATUS_REC_MASTER_ABORT|PCI_STATUS_SIG_SYSTEM_ERROR|PCI_STATUS_DETECTED_PARITY)

PCI_CLASS_REVISION		EQU	$08     ** High 24 bits are class, low 8 bit are revision 
PCI_REVISION_ID 		EQU     $08     ** Revision ID 
PCI_CLASS_PROG	 		EQU     $09     ** Reg. Level Programming Interface 
PCI_CLASS_DEVICE		EQU     $0a     ** Device class 

PCI_CACHE_LINE_SIZE		EQU     $0c     ** 8 bits 
PCI_LATENCY_TIMER 		EQU     $0d     ** 8 bits 
PCI_HEADER_TYPE  		EQU     $0e     ** 8 bits 
PCI_HEADER_TYPE_NORMAL		EQU	0	** bit 0=0 
PCI_HEADER_TYPE_BRIDGE		EQU	1	** bit 0=1 
PCI_HEADER_TYPE_CARDBUS		EQU	2	** bit 1=1 
** PCI_HEADER_TYPE bit 2 to 6 are reserved bit7=1 multi function device 

PCI_BIST    			EQU      $0f    ** 8 bits 
PCI_BIST_CODE_MASK		EQU      $0f    ** Return result 
PCI_BIST_START  		EQU      $40    ** 1 to start BIST, 2 secs or less 
PCI_BIST_CAPABLE 		EQU      $80    ** 1 if BIST capable 

**
** Base addresses specify locations in memory or I/O space.
** Decoded size can be determined by writing a value of 
** $ffffffff to the register, and reading it back.  Only 
** 1 bits are decoded.
**	
 
PCI_BASE_ADDRESS_0 		EQU     $10     ** 32 bits 
PCI_BASE_ADDRESS_1 		EQU     $14     ** 32 bits [htype 0,1 only] 
PCI_BASE_ADDRESS_2 		EQU	$18     ** 32 bits [htype 0 only] 
PCI_BASE_ADDRESS_3 		EQU     $1c     ** 32 bits 
PCI_BASE_ADDRESS_4 		EQU     $20     ** 32 bits 
PCI_BASE_ADDRESS_5 		EQU     $24     ** 32 bits 
PCI_BASE_ADDRESS_SPACE 		EQU	$01     ** 0 = memory, 1 = I/O 
PCI_BASE_ADDRESS_SPACE_IO 	EQU	$01
PCI_BASE_ADDRESS_SPACE_MEMORY 	EQU	$00
PCI_BASE_ADDRESS_MEM_TYPE_MASK 	EQU	$06
PCI_BASE_ADDRESS_MEM_TYPE_32   	EQU	$00     ** 32 bit address 
PCI_BASE_ADDRESS_MEM_TYPE_1M   	EQU	$02     ** Below 1M 
PCI_BASE_ADDRESS_MEM_TYPE_64   	EQU	$04     ** 64 bit address 
PCI_BASE_ADDRESS_MEM_PREFETCH  	EQU	$08     ** prefetchable? 
PCI_BASE_ADDRESS_MEM_MASK      	EQU	(~$0f)
PCI_BASE_ADDRESS_IO_MASK       	EQU	(~$03)
** bit 1 is reserved if address_space = 1 

** Header type 0 (normal devices) 
PCI_CARDBUS_CIS         		EQU	$28
PCI_SUBSYSTEM_VENDOR_ID 		EQU	$2c
PCI_SUBSYSTEM_ID        		EQU	$2e  
PCI_ROM_ADDRESS         		EQU	$30     ** Bits 31..11 are address, 10..1 reserved 
PCI_ROM_ADDRESS_ENABLE 			EQU	$01
PCI_ROM_ADDRESS_MASK    		EQU	(~$7ff)

PCI_CAPABILITIES_PTR    		EQU	$34	** 8 bits (PCI 2.2) 
** $38-$3b are reserved 
PCI_INTERRUPT_LINE      		EQU	$3c     ** 8 bits 
PCI_INTERRUPT_PIN       		EQU	$3d     ** 8 bits 
PCI_MIN_GNT             		EQU	$3e     ** 8 bits 
PCI_MAX_LAT             		EQU	$3f     ** 8 bits 

** Header type 1 (PCI-to-PCI bridges) 
PCI_PRIMARY_BUS         		EQU	$18     ** Primary bus number 
PCI_SECONDARY_BUS       		EQU	$19     ** Secondary bus number 
PCI_SUBORDINATE_BUS     		EQU	$1a     ** Highest bus number behind the bridge 
PCI_SEC_LATENCY_TIMER   		EQU	$1b     ** Latency timer for secondary interface 
PCI_IO_BASE             		EQU	$1c     ** I/O range behind the bridge 
PCI_IO_LIMIT            		EQU	$1d
PCI_IO_RANGE_TYPE_MASK 			EQU	$0f     ** I/O bridging type 
PCI_IO_RANGE_TYPE_16   			EQU	$00
PCI_IO_RANGE_TYPE_32   			EQU	$01
PCI_IO_RANGE_MASK      			EQU	~$0f
PCI_SEC_STATUS          		EQU	$1e     ** Secondary status register, only bit 14 used 
PCI_MEMORY_BASE         		EQU	$20     ** Memory range behind 
PCI_MEMORY_LIMIT        		EQU	$22
PCI_MEMORY_RANGE_TYPE_MASK 		EQU	$0f
PCI_MEMORY_RANGE_MASK  			EQU	~$0f
PCI_PREF_MEMORY_BASE    		EQU	$24     ** Prefetchable memory range behind 
PCI_PREF_MEMORY_LIMIT   		EQU	$26
PCI_PREF_RANGE_TYPE_MASK 		EQU	$0f
PCI_PREF_RANGE_TYPE_32 			EQU	$00
PCI_PREF_RANGE_TYPE_64 			EQU	$01
PCI_PREF_RANGE_MASK    			EQU	~$0f
PCI_PREF_BASE_UPPER32   		EQU	$28     ** Upper half of prefetchable memory range 
PCI_PREF_LIMIT_UPPER32  		EQU	$2c
PCI_IO_BASE_UPPER16     		EQU	$30     ** Upper half of I/O addresses 
PCI_IO_LIMIT_UPPER16    		EQU	$32
** $34-$3b is reserved 
PCI_ROM_ADDRESS1        		EQU	$38     ** Same as PCI_ROM_ADDRESS, but for htype 1 
** $3c-$3d are same as for htype 0 
PCI_BRIDGE_CONTROL      		EQU	$3e
PCI_BRIDGE_CTL_PARITY  			EQU	$01     ** Enable parity detection on secondary interface 
PCI_BRIDGE_CTL_SERR			EQU    	$02     ** The same for SERR forwarding 
PCI_BRIDGE_CTL_NO_ISA			EQU  	$04     ** Disable bridging of ISA ports 
PCI_BRIDGE_CTL_VGA 			EQU    	$08     ** Forward VGA addresses 
PCI_BRIDGE_CTL_MASTER_ABORT		EQU 	$20   	** Report master aborts 
PCI_BRIDGE_CTL_BUS_RESET		EQU 	$40   	** Secondary bus reset 
PCI_BRIDGE_CTL_FAST_BACK		EQU 	$80   	** Fast Back2Back enabled on secondary interface 

** Header type 2 (CardBus bridges) 
** $14-$15 reserved 
PCI_CB_SEC_STATUS 			EQU     $16     ** Secondary status 
PCI_CB_PRIMARY_BUS			EQU     $18     ** PCI bus number 
PCI_CB_CARD_BUS 			EQU     $19     ** CardBus bus number 
PCI_CB_SUBORDINATE_BUS			EQU  	$1a     ** Subordinate bus number 
PCI_CB_LATENCY_TIMER			EQU    	$1b     ** CardBus latency timer 
PCI_CB_MEMORY_BASE_0			EQU    	$1c
PCI_CB_MEMORY_LIMIT_0			EQU   	$20
PCI_CB_MEMORY_BASE_1			EQU    	$24
PCI_CB_MEMORY_LIMIT_1			EQU   	$28
PCI_CB_IO_BASE_0			EQU     $2c
PCI_CB_IO_BASE_0_HI			EQU     $2e
PCI_CB_IO_LIMIT_0			EQU     $30
PCI_CB_IO_LIMIT_0_HI 			EQU   	$32
PCI_CB_IO_BASE_1 			EQU     $34
PCI_CB_IO_BASE_1_HI			EQU     $36
PCI_CB_IO_LIMIT_1 			EQU     $38
PCI_CB_IO_LIMIT_1_HI			EQU    	$3a
PCI_CB_IO_RANGE_MASK			EQU   	~$03
** $3c-$3d are same as for htype 0 
PCI_CB_BRIDGE_CONTROL			EQU   	$3e
PCI_CB_BRIDGE_CTL_PARITY		EQU     $01     ** Similar to standard bridge control register 
PCI_CB_BRIDGE_CTL_SERR			EQU    	$02
PCI_CB_BRIDGE_CTL_ISA			EQU     $04
PCI_CB_BRIDGE_CTL_VGA			EQU     $08
PCI_CB_BRIDGE_CTL_MASTER_ABORT		EQU 	$20
PCI_CB_BRIDGE_CTL_CB_RESET		EQU     $40     ** CardBus reset 
PCI_CB_BRIDGE_CTL_16BIT_INT		EQU    	$80     ** Enable interrupt for 16-bit cards 
PCI_CB_BRIDGE_CTL_PREFETCH_MEM0		EQU 	$100    ** Prefetch enable for both memory regions 
PCI_CB_BRIDGE_CTL_PREFETCH_MEM1		EQU 	$200
PCI_CB_BRIDGE_CTL_POST_WRITES		EQU  	$400
PCI_CB_SUBSYSTEM_VENDOR_ID		EQU 	$40
PCI_CB_SUBSYSTEM_ID			EQU     $42
PCI_CB_LEGACY_MODE_BASE			EQU 	$44     ** 16-bit PC Card legacy mode base address (ExCa) 
** $48-$7f reserved 

** Device classes and subclasses 

PCI_CLASS_NOT_DEFINED			EQU     $0000
PCI_CLASS_NOT_DEFINED_VGA 		EQU     $0001

PCI_BASE_CLASS_STORAGE 			EQU     $01
PCI_CLASS_STORAGE_SCSI 			EQU     $0100
PCI_CLASS_STORAGE_IDE			EQU     $0101
PCI_CLASS_STORAGE_FLOPPY		EQU     $0102
PCI_CLASS_STORAGE_IPI			EQU     $0103
PCI_CLASS_STORAGE_RAID 			EQU     $0104
PCI_CLASS_STORAGE_OTHER			EQU     $0180

PCI_BASE_CLASS_NETWORK			EQU     $02
PCI_CLASS_NETWORK_ETHERNET 		EQU     $0200
PCI_CLASS_NETWORK_TOKEN_RING		EQU    	$0201
PCI_CLASS_NETWORK_FDDI			EQU     $0202
PCI_CLASS_NETWORK_ATM 			EQU     $0203
PCI_CLASS_NETWORK_OTHER			EQU     $0280

PCI_BASE_CLASS_DISPLAY			EQU     $03
PCI_CLASS_DISPLAY_VGA 			EQU     $0300
PCI_CLASS_DISPLAY_XGA 			EQU     $0301
PCI_CLASS_DISPLAY_OTHER			EQU     $0380

PCI_BASE_CLASS_MULTIMEDIA		EQU     $04
PCI_CLASS_MULTIMEDIA_VIDEO		EQU     $0400
PCI_CLASS_MULTIMEDIA_AUDIO		EQU     $0401
PCI_CLASS_MULTIMEDIA_OTHER		EQU	$0480

PCI_BASE_CLASS_MEMORY			EQU     $05
PCI_CLASS_MEMORY_RAM 			EQU     $0500
PCI_CLASS_MEMORY_FLASH			EQU     $0501
PCI_CLASS_MEMORY_OTHER			EQU     $0580

PCI_BASE_CLASS_BRIDGE 			EQU     $06
PCI_CLASS_BRIDGE_HOST			EQU     $0600
PCI_CLASS_BRIDGE_ISA			EQU     $0601
PCI_CLASS_BRIDGE_EISA 			EQU     $0602
PCI_CLASS_BRIDGE_MC  			EQU     $0603
PCI_CLASS_BRIDGE_PCI 			EQU     $0604
PCI_CLASS_BRIDGE_PCMCIA			EQU     $0605
PCI_CLASS_BRIDGE_NUBUS 			EQU     $0606
PCI_CLASS_BRIDGE_CARDBUS		EQU     $0607
PCI_CLASS_BRIDGE_OTHER 			EQU     $0680

PCI_BASE_CLASS_COMMUNICATION		EQU    	$07
PCI_CLASS_COMMUNICATION_SERIAL		EQU  	$0700
PCI_CLASS_COMMUNICATION_PARALLEL	EQU 	$0701
PCI_CLASS_COMMUNICATION_OTHER		EQU   	$0780

PCI_BASE_CLASS_SYSTEM			EQU     $08
PCI_CLASS_SYSTEM_PIC			EQU     $0800
PCI_CLASS_SYSTEM_DMA 			EQU     $0801
PCI_CLASS_SYSTEM_TIMER			EQU     $0802
PCI_CLASS_SYSTEM_RTC			EQU     $0803
PCI_CLASS_SYSTEM_OTHER			EQU     $0880

PCI_BASE_CLASS_INPUT			EQU     $09
PCI_CLASS_INPUT_KEYBOARD		EQU     $0900
PCI_CLASS_INPUT_PEN 			EQU     $0901
PCI_CLASS_INPUT_MOUSE			EQU     $0902
PCI_CLASS_INPUT_OTHER			EQU     $0980

PCI_BASE_CLASS_DOCKING			EQU     $0a
PCI_CLASS_DOCKING_GENERIC		EQU     $0a00
PCI_CLASS_DOCKING_OTHER			EQU     $0a01

PCI_BASE_CLASS_PROCESSOR		EQU     $0b
PCI_CLASS_PROCESSOR_386 		EQU     $0b00
PCI_CLASS_PROCESSOR_486			EQU     $0b01
PCI_CLASS_PROCESSOR_PENTIUM		EQU     $0b02
PCI_CLASS_PROCESSOR_ALPHA		EQU     $0b10
PCI_CLASS_PROCESSOR_POWERPC		EQU     $0b20
PCI_CLASS_PROCESSOR_CO			EQU     $0b40

PCI_BASE_CLASS_SERIAL			EQU     $0c
PCI_CLASS_SERIAL_FIREWIRE		EQU     $0c00
PCI_CLASS_SERIAL_ACCESS			EQU     $0c01
PCI_CLASS_SERIAL_SSA			EQU     $0c02
PCI_CLASS_SERIAL_USB			EQU     $0c03
PCI_CLASS_SERIAL_FIBER			EQU     $0c04

PCI_CLASS_OTHERS			EQU     $ff


**
** The PCI interface treats multi-function devices as independent
** devices.  The slot/function address of each device is encoded
** in a single byte as follows:
**
**      7:3 = slot
**      2:0 = function
**      
	
	ENDC		; LIBRARIES_OPENPCI_H
