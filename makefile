#!/usr/bin/make

#disable built-in rules
.SUFFIXES:
# don't delete intermeditae .o and build directories
.PRECIOUS: %.o %/.
#enable second expansion, so we can use $(@D) as dependency
.SECONDEXPANSION:

# compiler and linker
CC = m68k-amigaos-gcc
CXX = m68k-amigaos-g++
LD = m68k-amigaos-gcc
STRIP = m68k-amigaos-strip
MKDIR = mkdir -p
COPY = cp

BINDIR ?= _bin/
BUILDDIR ?= _o/
DEBUG ?= 0

CFLAGS ?=
CXXFLAGS ?=
LDFLAGS ?=
LIBS = -lamiga

# Version override values from tag:
#  - vMAJOR.MINOR or vMAJOR.MINOR.PATCH
#  - release_MAJOR.MINOR or release_MAJOR.MINOR.PATCH
# You can still override manually, e.g. make LIB_VERSION=2 LIB_REVISION=7
TAG_NAME ?= $(strip $(if $(GITHUB_REF_NAME),$(GITHUB_REF_NAME),$(shell git describe --tags --exact-match 2>/dev/null)))
TAG_VERSION := $(patsubst v%,%,$(patsubst release_%,%,$(TAG_NAME)))
LIB_VERSION ?= $(shell printf '%s' "$(TAG_VERSION)" | sed -n 's/^\([0-9][0-9]*\)\.\([0-9][0-9]*\)\(\.[0-9][0-9]*\)\{0,1\}$$/\1/p')
LIB_REVISION ?= $(shell printf '%s' "$(TAG_VERSION)" | sed -n 's/^\([0-9][0-9]*\)\.\([0-9][0-9]*\)\(\.[0-9][0-9]*\)\{0,1\}$$/\2/p')
ifeq ($(strip $(LIB_VERSION)),)
LIB_VERSION := 1
endif
ifeq ($(strip $(LIB_REVISION)),)
LIB_REVISION := 0
endif

BUILDFLAGS = -noixemul -mregparm=4 -msmall-code -m68020-60 -mtune=68030 -fno-builtin-strlen -ffreestanding
BUILDFLAGS += -ffunction-sections -fdata-sections
# Prevent the OpenPCI driver from defining its own broken swapl/swapw macros
BUILDFLAGS += -DOPENPCI_SWAP 

ifeq ($(DEBUG),1)
    CFLAGS += -DDBG
    LIBS += -ldebug -lm
    # -g with older amiga-gcc (6.5 / image :latest) yields hunks m68k-amigaos-strip cannot read
    BUILDFLAGS += -O3 -g -ggdb
else
    BUILDFLAGS += -Ofast -fomit-frame-pointer
endif

CFLAGS +=  $(BUILDFLAGS) -Wundef -I. -IPicasso96Develop/Include -IPicasso96Develop/PrivateInclude -Iopenpci
CFLAGS += -DLIB_VERSION=$(LIB_VERSION) -DLIB_REVISION=$(LIB_REVISION)
# Appended in .cpp recipe so target-specific CFLAGS apply
CXXFLAGS_EXTRA = -std=c++14 -fno-exceptions -fno-rtti -fno-use-cxa-atexit
LDFLAGS += $(BUILDFLAGS) -Wl,--gc-sections

###############################################################################

define source_to_object
	$(addprefix ${2},$(patsubst %.c,%.o,\
							$(patsubst %.cpp,%.o,\
							$(patsubst %.s,%.o,\
							$(patsubst %.asm,%.o,${1})))))
endef

define source_to_depend
	$(addprefix ${2},$(patsubst %.c,%.d,$(patsubst %.cpp,%.d,${1})))
endef

# place chip_library / card_library first (ROMTag must be at start of Amiga libs)
define create_objlist
	$(filter %/chip_library.o %/card_library.o chip_library.o card_library.o,$(call source_to_object,${1},${2}))\
	$(filter-out %/chip_library.o %/card_library.o chip_library.o card_library.o,$(call source_to_object,${1},${2}))
endef

define create_deplist
	$(call source_to_depend,$(filter %.c %.cpp,${1}),${2})
endef

define collect_sources
	$(strip $(filter %.c %.cpp %.s %.asm,$(shell find ${1} -name \*)))
endef

define build_rules
#create build directory if needed
%/.:
	@ $$(MKDIR) $$(@D)

# build .c files into .o with dependency generation
# $$$$(@D)/. will be doubly-evaluated to $(@D)/. which will trigger the build directory
# target above
${1}%.o: %.c | makefile $$$$(@D)/.
	@ echo compiling $$< ...
	@ $$(CC) $$(CFLAGS) -MMD -MP -c $$< -o $$@

${1}%.o: %.cpp | makefile $$$$(@D)/.
	@ echo compiling $$< ...
	@ $$(CXX) $$(CFLAGS) $$(CXXFLAGS_EXTRA) -MMD -MP -c $$< -o $$@

${1}%.o: %.asm | makefile $$$$(@D)/.
	$$(ASS) $$(AFLAGS) $$< -o $$@

${1}%.o: %.s | makefile $$$$(@D)/.
	$$(ASS) $$(AFLAGS) $$< -o $$@

endef

define make_driver
${1}_SRC = ${3} # $$(call collect_sources,$$(SOURCES))
${1}_OBJS = $$(call create_objlist,$$(${1}_SRC),${2})
${1}_DEPS = $$(call create_deplist,$$(${1}_SRC),${2})
${1}_TARGET = $$(BINDIR)/${1}

${1} : LDFLAGS += -ramiga-lib -nostartfiles

ifeq ($(DEBUG),0)
${1} : LDFLAGS += -nodefaultlibs
endif

$$(eval $$(call build_rules,${2}))
${1} : $$(BUILDDIR)=${2}
${1} : $$(${1}_OBJS)
	$(info ============ Building: ${1} ============)
	@ $$(MKDIR) $$(dir $$(${1}_TARGET))
	@ $$(LD) $$^ $${LIBS} $$(LDFLAGS) -o ${2}${1}
	@ $$(STRIP) ${2}${1} -o $$(${1}_TARGET)

# Include dependency files for this target
-include $$(${1}_DEPS)
endef

define make_exe
${1}_SRC = ${3}
${1}_OBJS = $$(call create_objlist,$$(${1}_SRC),${2})
${1}_DEPS = $$(call create_deplist,$$(${1}_SRC),${2})
${1}_TARGET = $$(BINDIR)/${1}

${1} : CFLAGS += -DTESTEXE -DDBG -DDEBUG
${1} : LIBS += -ldebug -lm

$$(eval $$(call build_rules,${2}))

${1} : $$(BUILDDIR)=${2}
${1} : $$(${1}_OBJS)
	$(info ============ Building: ${1} ============)
	@ $$(MKDIR) $$(dir $$(${1}_TARGET))
	@ $$(LD) $$^ $$(LIBS) $$(LDFLAGS) -o ${2}${1}
	@ $$(STRIP) ${2}${1} -o $$(${1}_TARGET)

# Include dependency files for this target
-include $$(${1}_DEPS)
endef

###############################################################################
# target 'all' (default target)

all : S3Trio64V2.chip \
      S3Trio64Plus.chip \
      S3Trio3264.chip \
      S3Vision864.chip \
      S3Trio64.card \
      Cybervision64.card \
      ATIMach32.card \
      ATIMach64GX.chip \
      ATIMach64.chip \
      ATIMach64.card \
      TestMach32 \
      TestMach32Card \
      TestMach64GX \
      TestMach64 \
      TestMach64Card \
      TestS3Trio3264 \
      TestS3Trio64Plus \
      TestS3Trio64V2 \
      TestS3Vision864 \
      TestS3TrioCard \
      SetS3Clk \
      AT3D.card \
      TestAT3D \
      TestAT3DCard \
      TestCybervision64

openpci.h : openpci/openpci.fd openpci/clib/openpci_protos.h
	@$(MKDIR) -p openpci/inline
	fd2sfd openpci/openpci.fd openpci/clib/openpci_protos.h openpci/openpci.sfd
	sfdc --output=openpci/inline/openpci.h --target=m68k-amigaos --mode=macros openpci/openpci.sfd
	sfdc --output=openpci/proto/openpci.h --target=m68k-amigaos --mode=proto openpci/openpci.sfd

#@$(COPY) Picasso96_card.h Picasso96Develop/PrivateInclude/clib/picasso96_card_protos.h # SFDC generated a header including this name

P96Headers : Picasso96_card.sfd Picasso96_chip.sfd  makefile
	sfdc --sdi --output=Picasso96Develop/PrivateInclude/inline/picasso96_card.h --target=m68k-amigaos --mode=macros Picasso96_card.sfd
	sfdc --sdi --output=Picasso96Develop/PrivateInclude/proto/picasso96_card.h --target=m68k-amigaos --mode=proto Picasso96_card.sfd
	sfdc --sdi --output=Picasso96Develop/PrivateInclude/clib/picasso96_card_protos.h --target=m68k-amigaos --mode=clib Picasso96_card.sfd
	sfdc --sdi --output=Picasso96Develop/PrivateInclude/inline/picasso96_chip.h --target=m68k-amigaos --mode=macros Picasso96_chip.sfd
	sfdc --sdi --output=Picasso96Develop/PrivateInclude/proto/picasso96_chip.h --target=m68k-amigaos --mode=proto Picasso96_chip.sfd
	sfdc --sdi --output=Picasso96Develop/PrivateInclude/clib/picasso96_chip_protos.h --target=m68k-amigaos --mode=clib Picasso96_chip.sfd

S3TRIO_SRC = common.c \
             s3trio64/s3trio64_common.cpp \
             s3trio64/chip_s3trio64.cpp \
             s3trio64/s3ramdac.cpp \
             s3trio64/s3i2c.cpp \
             s3trio64/s3_reg_smoke.cpp \
             edid_common.c \
             chip_library.c 

S3Trio64Plus.chip : CFLAGS+=-DREGISTER_OFFSET=0x8000 -DMMIOREGISTER_OFFSET=0x8000 -DCONFIG_S3TRIO64PLUS -include s3trio64/s3config.h 
$(eval $(call make_driver,S3Trio64Plus.chip,$(BUILDDIR)s3trio64plus/, ${S3TRIO_SRC}))

S3Trio3264.chip : CFLAGS+=-DREGISTER_OFFSET=0x8000 -DMMIOREGISTER_OFFSET=0x8000 -DCONFIG_S3TRIO3264 -include s3trio64/s3config.h 
$(eval $(call make_driver,S3Trio3264.chip,$(BUILDDIR)s3trio3264/, ${S3TRIO_SRC}))

S3Vision864.chip : CFLAGS+=-DREGISTER_OFFSET=0x8000 -DMMIOREGISTER_OFFSET=0x8000 -DCONFIG_VISION864 -include s3trio64/s3config.h
$(eval $(call make_driver,S3Vision864.chip,$(BUILDDIR)s3vision864/, ${S3TRIO_SRC}))

S3Trio64V2.chip : CFLAGS+=-DREGISTER_OFFSET=0x8000 -DMMIOREGISTER_OFFSET=0x8000 -DCONFIG_S3TRIO64V2 -include s3trio64/s3config.h
$(eval $(call make_driver,S3Trio64V2.chip,$(BUILDDIR)s3trio64v2/, ${S3TRIO_SRC}))

S3TRIOCARD_SRC = common.c \
                 card_common.c \
                 s3trio64/card_s3trio64.cpp \
                 s3trio64/s3trio64_common.cpp \
                 s3trio64/s3i2c.cpp \
                 edid_common.c \
                 card_library.c

S3Trio64.card : CFLAGS+=-DREGISTER_OFFSET=0x8000 -DMMIOREGISTER_OFFSET=0x8000 -DOPENPCI=1
$(eval $(call make_driver,S3Trio64.card,$(BUILDDIR)s3triocard/, ${S3TRIOCARD_SRC}))

S3TRIO_TESTEXE_SRC = common.c \
                     s3trio64/s3trio64_common.cpp \
                     s3trio64/s3ramdac.cpp \
                     s3trio64/chip_s3trio64.cpp \
                     s3trio64/s3_reg_smoke.cpp \
                     s3trio64/s3i2c.cpp \
                     edid_common.c

TestS3Trio3264 : CFLAGS+=-DCONFIG_S3TRIO3264 -DREGISTER_OFFSET=0 -DMMIOREGISTER_OFFSET=0 -include s3trio64/s3config.h
$(eval $(call make_exe,TestS3Trio3264,$(BUILDDIR)tests3trio3264/, ${S3TRIO_TESTEXE_SRC}))

TestS3Trio64Plus : CFLAGS+=-DCONFIG_S3TRIO64PLUS -DREGISTER_OFFSET=0 -DMMIOREGISTER_OFFSET=0 -include s3trio64/s3config.h
$(eval $(call make_exe,TestS3Trio64Plus,$(BUILDDIR)tests3trio64plus/, ${S3TRIO_TESTEXE_SRC}))

TestS3Trio64V2 : CFLAGS+=-DCONFIG_S3TRIO64V2 -DREGISTER_OFFSET=0 -DMMIOREGISTER_OFFSET=0 -include s3trio64/s3config.h
$(eval $(call make_exe,TestS3Trio64V2,$(BUILDDIR)tests3trio64v2/, ${S3TRIO_TESTEXE_SRC}))

TestS3Vision864 : CFLAGS+=-DCONFIG_VISION864 -DREGISTER_OFFSET=0 -DMMIOREGISTER_OFFSET=0 -include s3trio64/s3config.h
$(eval $(call make_exe,TestS3Vision864,$(BUILDDIR)tests3vision864/, ${S3TRIO_TESTEXE_SRC}))

S3TRIOCARD_TESTEXE_SRC = common.c \
                         card_common.c \
                         s3trio64/s3trio64_common.cpp \
                         s3trio64/card_s3trio64.cpp 

TestS3TrioCard : CFLAGS+=-DREGISTER_OFFSET=0x8000 -DMMIOREGISTER_OFFSET=0x8000 -DOPENPCI=1
$(eval $(call make_exe,TestS3TrioCard,$(BUILDDIR)tests3triocard/, ${S3TRIOCARD_TESTEXE_SRC}))

SETS3CLK_SRC = common.c \
               s3trio64/set_s3_mclk.cpp

SetS3Clk : CFLAGS+=-DREGISTER_OFFSET=0 -DMMIOREGISTER_OFFSET=0 -DBIGENDIAN_IO=0 -DBIGENDIAN_MMIO=0 -DOPENPCI=1
$(eval $(call make_exe,SetS3Clk,$(BUILDDIR)sets3clk/, ${SETS3CLK_SRC}))


CYBERVISION64CARD_SRC = common.c \
                 card_common.c \
                 s3trio64/card_cybervision64.cpp \
                 s3trio64/s3trio64_common.cpp \
                 s3trio64/chip_s3trio64.cpp \
                 s3trio64/s3_reg_smoke.cpp \
                 s3trio64/s3ramdac.cpp \
                 s3trio64/s3i2c.cpp \
                 edid_common.c \
                 card_library.c

Cybervision64.card : CFLAGS+=-DREGISTER_OFFSET=0x8000 -DMMIOREGISTER_OFFSET=0x8000 -DCONFIG_CYBERVISION64  -include s3trio64/s3config.h 
$(eval $(call make_driver,Cybervision64.card,$(BUILDDIR)cybervision64card/, ${CYBERVISION64CARD_SRC}))

CYBERVISION64CARD_TESTEXE_SRC = common.c \
                 card_common.c \
                 s3trio64/card_cybervision64.cpp \
                 s3trio64/s3trio64_common.cpp \
                 s3trio64/chip_s3trio64.cpp \
                 s3trio64/s3_reg_smoke.cpp \
                 s3trio64/s3ramdac.cpp \
                 s3trio64/s3i2c.cpp \
                 edid_common.c

TestCybervision64 : CFLAGS+=-DCONFIG_CYBERVISION64=1 -DREGISTER_OFFSET=0x8000 -DMMIOREGISTER_OFFSET=0x8000 -include s3trio64/s3config.h 
$(eval $(call make_exe,TestCybervision64,$(BUILDDIR)testcybervision64card/, ${CYBERVISION64CARD_TESTEXE_SRC}))


ATIMACH64_COMMON_SRC = common.c \
                mach64/mach64_common.cpp \
                mach64/chip_mach64.cpp \
                mach64/mach64_i2c.cpp \
                mach64/mach64_eeprom.cpp \
                edid_common.c \
                chip_library.c

ATIMACH64_GX_SRC = ${ATIMACH64_COMMON_SRC} \
                mach64/mach64_reg_smoke.cpp \
                mach64/mach64GX.cpp \
                mach64/mach64CT.cpp

ATIMACH64_VT_SRC = ${ATIMACH64_COMMON_SRC} \
                mach64/mach64_reg_smoke.cpp \
                mach64/mach64GT.cpp \
                mach64/mach64VT.cpp

ATIMach64GX.chip : CFLAGS+=-DCONFIG_ATIMACH64_GX -include mach64/mach64config.h
$(eval $(call make_driver,ATIMach64GX.chip,$(BUILDDIR)mach64gx/, ${ATIMACH64_GX_SRC}))

ATIMach64.chip : CFLAGS+=-DCONFIG_ATIMACH64_VT -include mach64/mach64config.h
$(eval $(call make_driver,ATIMach64.chip,$(BUILDDIR)mach64/, ${ATIMACH64_VT_SRC}))

ATIMACH64CARD_SRC = common.c \
                    card_common.c \
                    mach64/card_mach64.cpp \
                    mach64/mach64_common.cpp \
                    card_library.c

ATIMach64.card : CFLAGS+=-DCONFIG_ATIMACH64 -include mach64/mach64config.h
$(eval $(call make_driver,ATIMach64.card,$(BUILDDIR)mach64card/, ${ATIMACH64CARD_SRC}))

ATIMACH64_TESTEXE_COMMON_SRC = common.c \
                        mach64/mach64_common.cpp \
                        mach64/chip_mach64.cpp \
                        mach64/mach64_i2c.cpp \
                        mach64/mach64_eeprom.cpp \
                        edid_common.c

ATIMACH64_GX_TESTEXE_SRC = ${ATIMACH64_TESTEXE_COMMON_SRC} \
                        mach64/mach64_reg_smoke.cpp \
                        mach64/mach64GX.cpp \
                        mach64/mach64CT.cpp

ATIMACH64_VT_TESTEXE_SRC = ${ATIMACH64_TESTEXE_COMMON_SRC} \
                        mach64/mach64_reg_smoke.cpp \
                        mach64/mach64GT.cpp \
                        mach64/mach64VT.cpp

TestMach64GX : CFLAGS+=-DCONFIG_ATIMACH64_GX -DBIGENDIAN_MMIO=0 -include mach64/mach64config.h
$(eval $(call make_exe,TestMach64GX,$(BUILDDIR)testmach64gx/, ${ATIMACH64_GX_TESTEXE_SRC}))

TestMach64 : CFLAGS+=-DCONFIG_ATIMACH64_VT -DBIGENDIAN_MMIO=0 -include mach64/mach64config.h
$(eval $(call make_exe,TestMach64,$(BUILDDIR)testmach64/, ${ATIMACH64_VT_TESTEXE_SRC}))

TestMach64Card : CFLAGS+=-DCONFIG_ATIMACH64 -DBIGENDIAN_IO=0 -DBIGENDIAN_MMIO=0 -include mach64/mach64config.h
TESTATIMACH64CARD_SRC = common.c \
						card_common.c \
						mach64/card_mach64.cpp \
						mach64/mach64_common.cpp \
						mach64/mach64_i2c.cpp \
						edid_common.c
					
$(eval $(call make_exe,TestMach64Card,$(BUILDDIR)testmach64card/, ${TESTATIMACH64CARD_SRC}))


ATIMACH32_SRC = common.c \
                mach32/mach32_ramdac.cpp \
                mach32/mach32_eeprom.cpp \
                mach32/chip_mach32.cpp \
                mach32/mach32_reg_smoke.cpp \
                chip_library.c

ATIMach32.chip : CFLAGS+= -DCONFIG_ATIMACH32 -include mach32/mach32config.h
$(eval $(call make_driver,ATIMach32.chip,$(BUILDDIR)mach32/, ${ATIMACH32_SRC}))

ATIMACH32CARD_SRC = common.c \
                      card_common.c \
                      mach32/mach32_ramdac.cpp \
                      mach32/mach32_eeprom.cpp \
                      mach32/chip_mach32.cpp \
                      mach32/mach32_reg_smoke.cpp \
                      mach32/card_mach32.cpp \
                      card_library.c

ATIMach32.card : CFLAGS+= -DCONFIG_ATIMACH32 -DMACH32_EMBEDDED_CHIP=1 -include mach32/mach32config.h
$(eval $(call make_driver,ATIMach32.card,$(BUILDDIR)mach32card/, ${ATIMACH32CARD_SRC}))

ATIMACH32_TESTEXE_SRC = common.c \
                        mach32/mach32_ramdac.cpp \
                        mach32/mach32_eeprom.cpp \
                        mach32/chip_mach32.cpp \
                        mach32/mach32_reg_smoke.cpp

TestMach32 : CFLAGS+= -DCONFIG_ATIMACH32 -include mach32/mach32config.h
$(eval $(call make_exe,TestMach32,$(BUILDDIR)testmach32/, ${ATIMACH32_TESTEXE_SRC}))

TESTATIMACH32CARD_SRC = common.c \
                        card_common.c \
                        mach32/mach32_ramdac.cpp \
                        mach32/mach32_eeprom.cpp \
                        mach32/chip_mach32.cpp \
                        mach32/mach32_reg_smoke.cpp \
                        mach32/card_mach32.cpp

TestMach32Card : CFLAGS+= -DCONFIG_ATIMACH32 -DMACH32_EMBEDDED_CHIP=1 -include mach32/mach32config.h
$(eval $(call make_exe,TestMach32Card,$(BUILDDIR)testmach32card/, ${TESTATIMACH32CARD_SRC}))

AT3D_SRC = common.c \
           at3d/at3d_common.cpp \
           at3d/at3d_i2c.cpp \
           at3d/chip_at3d.cpp \
           at3d/at3d_reg_smoke.cpp \
           edid_common.c \
           chip_library.c

AT3D.chip : CFLAGS+=-DREGISTER_OFFSET=0 -DMMIOREGISTER_OFFSET=0 -include at3d/at3dconfig.h
$(eval $(call make_driver,AT3D.chip,$(BUILDDIR)at3d/, ${AT3D_SRC}))

AT3D_TESTEXE_SRC = common.c \
                   at3d/at3d_common.cpp \
                   at3d/at3d_i2c.cpp \
                   at3d/chip_at3d.cpp \
           at3d/at3d_reg_smoke.cpp \
                   edid_common.c

TestAT3D : CFLAGS+=-DREGISTER_OFFSET=0 -DMMIOREGISTER_OFFSET=0 -include at3d/at3dconfig.h
$(eval $(call make_exe,TestAT3D,$(BUILDDIR)testat3d/, ${AT3D_TESTEXE_SRC}))

AT3DCARD_SRC = common.c \
               card_common.c \
               at3d/chip_at3d.cpp \
           at3d/at3d_reg_smoke.cpp \
               edid_common.c \
               at3d/card_at3d.cpp \
               at3d/at3d_common.cpp \
               at3d/at3d_i2c.cpp \
               card_library.c

AT3D.card : CFLAGS+=-DREGISTER_OFFSET=0 -DMMIOREGISTER_OFFSET=0 -DOPENPCI=1 -DAT3D_EMBEDDED_CHIP=1
$(eval $(call make_driver,AT3D.card,$(BUILDDIR)at3dcard/, ${AT3DCARD_SRC}))

AT3DCARD_TESTEXE_SRC = common.c \
                       card_common.c \
                       at3d/chip_at3d.cpp \
           at3d/at3d_reg_smoke.cpp \
                       edid_common.c \
                       at3d/at3d_common.cpp \
                       at3d/at3d_i2c.cpp \
                       at3d/card_at3d.cpp

TestAT3DCard : CFLAGS+=-DREGISTER_OFFSET=0 -DMMIOREGISTER_OFFSET=0 -DOPENPCI=1 -DAT3D_EMBEDDED_CHIP=1
$(eval $(call make_exe,TestAT3DCard,$(BUILDDIR)testat3dcard/, ${AT3DCARD_TESTEXE_SRC}))


# target 'clean'

clean:
	rm -rf $(BUILDDIR)*
	rm -rf $(BINDIR)*
