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

BINDIR ?= _bin/
BUILDDIR ?= _o/
DEBUG ?= 0

BUILDFLAGS = -noixemul -msmall-code -m68020-60 -mtune=68030 -g -ggdb -fomit-frame-pointer

CFLAGS ?=
CFLAGS +=  $(BUILDFLAGS) -I. -IPicasso96Develop/Include -IPicasso96Develop/PrivateInclude -IPrometheus/include
LDFLAGS ?=
LDFLAGS += $(BUILDFLAGS)
LIBS = -lamiga

ifeq ($(DEBUG),1)
	CFLAGS += -DDBG
	LIBS += -ldebug
    BUILDFLAGS += -O0
else
    BUILDFLAGS += -O3 -mregparm=4
endif

###############################################################################

define source_to_object
	$(addprefix ${2},$(patsubst %.c,%.o,\
							$(patsubst %.s,%.o,\
							$(patsubst %.asm,%.o,${1}))))
endef

#place chip_library first
define create_objlist
	$(filter chip_library.o,$(call source_to_object,${1},${2}))\
	$(filter-out chip_library.o,$(call source_to_object,${1},${2}))
endef

define collect_sources
	$(strip $(filter %.c %.s %.asm,$(shell find ${1} -name \*)))
endef

define build_rules
#create build directory if needed
%/.:
	$$(MKDIR) $$(@D)

# build .c .asm and .s files into .o
# $$$$(@D)/. will be doubly-evaluated to $(@D)/. which will trigger the build directory
# target above
${1}%.o: %.c | makefile $$$$(@D)/.
	echo $$(@D)
	$$(CC) $$(CFLAGS) -c $$< -o $$@

${1}%.o: %.asm | makefile $$$$(@D)/.
	$$(ASS) $$(AFLAGS) $$< -o $$@

${1}%.o: %.s | makefile $$$$(@D)/.
	$$(ASS) $$(AFLAGS) $$< -o $$@

endef

define make_driver
${1}_SRC = ${3} # $$(call collect_sources,$$(SOURCES))
${1}_OBJS = $$(call create_objlist,$$(${1}_SRC),${2})
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
endef

define make_exe
${1}_SRC = ${3}
${1}_OBJS = $$(call create_objlist,$$(${1}_SRC),${2})
${1}_TARGET = $$(BINDIR)/${1}

echo "Building: $${${1}_TARGET}"

${1} : CFLAGS += -DTESTEXE -DDBG -DDEBUG
${1} : LIBS += -ldebug

$$(eval $$(call build_rules,${2}))

${1} : $$(BUILDDIR)=${2}
${1} : $$(${1}_OBJS)
	$$(MKDIR) $$(dir $$(${1}_TARGET))
	$$(LD) $$^ $$(LIBS) $$(LDFLAGS) -o $$(<D)/${1}
	$$(STRIP) $$(<D)/${1} -o $$(${1}_TARGET)
endef

###############################################################################

# target 'all' (default target)

all : S3Trio64Plus.chip S3Trio3264.chip S3Vision864.chip ATIMach64.chip TestMach64

S3TRIO_SRC = common.c \
             s3trio64/chip_s3trio64.c \
             s3trio64/s3ramdac.c \
             chip_library.c 


S3Trio64Plus.chip : CFLAGS+=-DBIGENDIAN_MMIO=1 -DBUILD_VISION864=0
$(eval $(call make_driver,S3Trio64Plus.chip,$(BUILDDIR)s3trio64plus/, ${S3TRIO_SRC}))

S3Trio3264.chip : CFLAGS+=-DBIGENDIAN_MMIO=0 -DBUILD_VISION864=0
$(eval $(call make_driver,S3Trio3264.chip,$(BUILDDIR)s3trio3264/, ${S3TRIO_SRC}))

S3Vision864.chip : CFLAGS+=-DBIGENDIAN_MMIO=0 -DBUILD_VISION864=1
$(eval $(call make_driver,S3Vision864.chip,$(BUILDDIR)s3vision864/, ${S3TRIO_SRC}))


ATIMACH64_SRC = common.c \
                mach64/chip_mach64.c \
                chip_library.c

ATIMach64.chip : CFLAGS+=-DBIGENDIAN_MMIO=0
$(eval $(call make_driver,ATIMach64.chip,$(BUILDDIR)mach64/, ${ATIMACH64_SRC}))

ATIMACH64_TESTEXE_SRC = common.c \
                        mach64/chip_mach64.c

$(eval $(call make_exe,TestMach64,$(BUILDDIR)testmach64/, ${ATIMACH64_TESTEXE_SRC}))


# target 'clean'

clean:
	rm -rf $(BUILDDIR)*
	rm -rf $(BINDIR)*.o
