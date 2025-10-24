#!/usr/bin/make

#disable built-in rules
.SUFFIXES:
# don't delete intermeditae .o and build directories
.PRECIOUS: %.o %/.
#enable second expansion, so we can use $(@D) as dependency
.SECONDEXPANSION:

# compiler and linker
CC = m68k-amigaos-gcc
LD = m68k-amigaos-gcc
STRIP = m68k-amigaos-strip
MKDIR = mkdir -p
COPY = cp

BINDIR ?= _bin/
BUILDDIR ?= _o/
DEBUG ?= 0

CFLAGS ?=
LDFLAGS ?=
LIBS = -lamiga

BUILDFLAGS = -noixemul -mregparm=4 -msmall-code -m68020-60 -mtune=68030 -g -ggdb

ifeq ($(DEBUG),1)
    CFLAGS += -DDBG
    LIBS += -ldebug -lm
    BUILDFLAGS += -O3
else
    BUILDFLAGS += -Ofast -fomit-frame-pointer
endif

CFLAGS +=  $(BUILDFLAGS) -I. -IPrometheus/include -IPicasso96Develop/Include -IPicasso96Develop/PrivateInclude -Iopenpci
LDFLAGS += $(BUILDFLAGS)

###############################################################################

define source_to_object
	$(addprefix ${2},$(patsubst %.c,%.o,\
							$(patsubst %.s,%.o,\
							$(patsubst %.asm,%.o,${1}))))
endef

define source_to_depend
	$(addprefix ${2},$(patsubst %.c,%.d,${1}))
endef

#place chip_library first
define create_objlist
	$(filter chip_library.o,$(call source_to_object,${1},${2}))\
	$(filter-out chip_library.o,$(call source_to_object,${1},${2}))
endef

define create_deplist
	$(call source_to_depend,$(filter %.c,${1}),${2})
endef

define collect_sources
	$(strip $(filter %.c %.s %.asm,$(shell find ${1} -name \*)))
endef

define build_rules
#create build directory if needed
%/.:
	$$(MKDIR) $$(@D)

# build .c files into .o with dependency generation
# $$$$(@D)/. will be doubly-evaluated to $(@D)/. which will trigger the build directory
# target above
${1}%.o: %.c | makefile $$$$(@D)/.
	echo $$(@D)
	$$(CC) $$(CFLAGS) -MMD -MP -c $$< -o $$@

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

echo "Building: $${${1}_TARGET}"

$$(eval $$(call build_rules,${2}))
${1} : $$(BUILDDIR)=${2}
${1} : $$(${1}_OBJS)
	$$(MKDIR) $$(dir $$(${1}_TARGET))
	$$(LD) $$^ $${LIBS} $$(LDFLAGS) -o $$(<D)/${1}
	$$(STRIP) $$(<D)/${1} -o $$(${1}_TARGET)

# Include dependency files for this target
-include $$(${1}_DEPS)
endef

define make_exe
${1}_SRC = ${3}
${1}_OBJS = $$(call create_objlist,$$(${1}_SRC),${2})
${1}_DEPS = $$(call create_deplist,$$(${1}_SRC),${2})
${1}_TARGET = $$(BINDIR)/${1}

echo "Building: $${${1}_TARGET}"

${1} : CFLAGS += -DTESTEXE -DDBG -DDEBUG
${1} : LIBS += -ldebug -lm

$$(eval $$(call build_rules,${2}))

${1} : $$(BUILDDIR)=${2}
${1} : $$(${1}_OBJS)
	$$(MKDIR) $$(dir $$(${1}_TARGET))
	$$(LD) $$^ $$(LIBS) $$(LDFLAGS) -o $$(<D)/${1}
	$$(STRIP) $$(<D)/${1} -o $$(${1}_TARGET)

# Include dependency files for this target
-include $$(${1}_DEPS)
endef

###############################################################################
# target 'all' (default target)

all : S3Trio64Plus.chip S3Trio3264.chip S3Vision864.chip S3Trio64.card ATIMach64.chip TestMach64 TestS3Trio64Plus TestS3TrioCard

openpci.h : openpci/openpci.fd openpci/clib/openpci_protos.h
	@$(MKDIR) -p openpci/inline
	fd2sfd openpci/openpci.fd openpci/clib/openpci_protos.h openpci/openpci.sfd
	sfdc --output=openpci/inline/openpci.h --target=m68k-amigaos --mode=macros openpci/openpci.sfd
	sfdc --output=openpci/proto/openpci.h --target=m68k-amigaos --mode=proto openpci/openpci.sfd

#@$(COPY) Picasso96_card.h Picasso96Develop/PrivateInclude/clib/picasso96_card_protos.h # SFDC generated a header including this name

P96Headers : Picasso96_card.sfd Picasso96Develop/PrivateInclude/clib/Picasso96_card_protos.h Picasso96_chip.sfd Picasso96Develop/PrivateInclude/clib/Picasso96_chip_protos.h makefile
	sfdc --sdi --output=Picasso96Develop/PrivateInclude/inline/picasso96_card.h --target=m68k-amigaos --mode=macros Picasso96_card.sfd
	sfdc --sdi --output=Picasso96Develop/PrivateInclude/proto/picasso96_card.h --target=m68k-amigaos --mode=proto Picasso96_card.sfd
	sfdc --sdi --output=Picasso96Develop/PrivateInclude/clib/picasso96_card_protos.h --target=m68k-amigaos --mode=clib Picasso96_card.sfd
	sfdc --sdi --output=Picasso96Develop/PrivateInclude/inline/picasso96_chip.h --target=m68k-amigaos --mode=macros Picasso96_chip.sfd
	sfdc --sdi --output=Picasso96Develop/PrivateInclude/proto/picasso96_chip.h --target=m68k-amigaos --mode=proto Picasso96_chip.sfd
	sfdc --sdi --output=Picasso96Develop/PrivateInclude/clib/picasso96_chip_protos.h --target=m68k-amigaos --mode=clib Picasso96_chip.sfd



S3TRIO_SRC = common.c \
             s3trio64/s3trio64_common.c \
             s3trio64/chip_s3trio64.c \
             s3trio64/s3ramdac.c \
             chip_library.c 

S3Trio64Plus.chip : CFLAGS+=-DBIGENDIAN_MMIO=1 -DBUILD_VISION864=0 -DREGISTER_OFFSET=0x8000 -DMMIOREGISTER_OFFSET=0x8000 -DMMIO_ONLY=1 -DBIGENDIAN_IO=1
$(eval $(call make_driver,S3Trio64Plus.chip,$(BUILDDIR)s3trio64plus/, ${S3TRIO_SRC}))

S3Trio3264.chip : CFLAGS+=-DBIGENDIAN_MMIO=0 -DBUILD_VISION864=0 -DREGISTER_OFFSET=0x8000 -DMMIOREGISTER_OFFSET=0x8000
$(eval $(call make_driver,S3Trio3264.chip,$(BUILDDIR)s3trio3264/, ${S3TRIO_SRC}))

S3Vision864.chip : CFLAGS+=-DBIGENDIAN_MMIO=0 -DBUILD_VISION864=1 -DREGISTER_OFFSET=0x8000 -DMMIOREGISTER_OFFSET=0x8000
$(eval $(call make_driver,S3Vision864.chip,$(BUILDDIR)s3vision864/, ${S3TRIO_SRC}))

S3TRIOCARD_SRC = common.c \
                 s3trio64/card_s3trio64.c \
                 s3trio64/s3trio64_common.c \
                 card_library.c

S3Trio64.card : CFLAGS+=-DREGISTER_OFFSET=0x8000 -DMMIOREGISTER_OFFSET=0x8000
$(eval $(call make_driver,S3Trio64.card,$(BUILDDIR)s3triocard/, ${S3TRIOCARD_SRC}))

S3TRIO64PLUS_TESTEXE_SRC = common.c \
                           s3trio64/s3trio64_common.c \
                           s3trio64/s3ramdac.c \
                           s3trio64/chip_s3trio64.c

TestS3Trio64Plus : CFLAGS+=-DBIGENDIAN_MMIO=1 -DBUILD_VISION864=0 -DREGISTER_OFFSET=0x8000 -DMMIOREGISTER_OFFSET=0x8000

$(eval $(call make_exe,TestS3Trio64Plus,$(BUILDDIR)tests3trio64plus/, ${S3TRIO64PLUS_TESTEXE_SRC}))


S3TRIOCARD_TESTEXE_SRC = common.c \
				 s3trio64/s3trio64_common.c \
                 s3trio64/card_s3trio64.c 
                 
TestS3TrioCard : CFLAGS+=-DREGISTER_OFFSET=0x8000 -DMMIOREGISTER_OFFSET=0x8000
$(eval $(call make_exe,TestS3TrioCard,$(BUILDDIR)tests3triocard/, ${S3TRIOCARD_TESTEXE_SRC}))


ATIMACH64_SRC = common.c \
                mach64/mach64GT.c \
                mach64/mach64VT.c \
                mach64/mach64_common.c \
                mach64/chip_mach64.c \
                chip_library.c

ATIMach64.chip : CFLAGS+=-DBIGENDIAN_MMIO=0 -DREGISTER_OFFSET=0x0 -DMMIOREGISTER_OFFSET=0x0
ATIMach64.chip : LDFLAGS+=-lm
$(eval $(call make_driver,ATIMach64.chip,$(BUILDDIR)mach64/, ${ATIMACH64_SRC}))

ATIMACH64_TESTEXE_SRC = common.c \
                        mach64/mach64GT.c \
                        mach64/mach64VT.c \
                        mach64/mach64_common.c \
                        mach64/chip_mach64.c

TestMach64 : CFLAGS+=-DBIGENDIAN_MMIO=0 -DREGISTER_OFFSET=0x0 -DMMIOREGISTER_OFFSET=0x0

$(eval $(call make_exe,TestMach64,$(BUILDDIR)testmach64/, ${ATIMACH64_TESTEXE_SRC}))


# target 'clean'

clean:
	rm -rf $(BUILDDIR)*
	rm -rf $(BINDIR)*
