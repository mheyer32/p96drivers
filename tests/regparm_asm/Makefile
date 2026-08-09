# Register-parameter attribute probe suite (m68k-amigaos-gcc / g++)
#
# Host-side objdump check (primary CI-style gate):
#   source ~/bin/startAmigaGCC.sh && make check
#
# Optional: build Amiga TestRegParm binary for on-target runtime checks:
#   make TestRegParm
#   SQUIRT_HOST=192.168.0.110 ../../squirt.sh _bin/TestRegParm SYS:c/

CROSS     ?= m68k-amigaos-
CC        := $(CROSS)gcc
CXX       := $(CROSS)g++
OBJDUMP   := $(CROSS)objdump
MKDIR     := mkdir -p

ROOT      := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
GEN       := $(ROOT)/generated
BINDIR    := $(ROOT)/_bin
OBJDIR    := $(ROOT)/_o

# Match p96drivers production flags
REGPARM_FLAGS := -noixemul -mregparm=4 -m68020-60 -mtune=68030 -ffreestanding \
                 -Ofast -fomit-frame-pointer
NORP_FLAGS    := -noixemul -m68020-60 -mtune=68030 -ffreestanding \
                 -Ofast -fomit-frame-pointer

CFLAGS   += $(REGPARM_FLAGS) -I$(ROOT)
CXXFLAGS += $(REGPARM_FLAGS) -std=c++14 -fno-exceptions -fno-rtti -I$(ROOT)

PROBES_C   := $(GEN)/probes_c.c
PROBES_CXX := $(GEN)/probes_cxx.cpp
MANIFEST   := $(GEN)/manifest.json

OBJS_REGPARM := $(OBJDIR)/probes_c.o $(OBJDIR)/probes_cxx.o
OBJS_NORP    := $(OBJDIR)/probes_c_norp.o $(OBJDIR)/probes_cxx_norp.o

.PHONY: all generate check check-norp objdump TestRegParm clean squirt

all: check

generate: $(MANIFEST)

$(MANIFEST): gen_probes.py regparm_macros.h regparm_types.h
	@python3 $(ROOT)/gen_probes.py

$(PROBES_C) $(PROBES_CXX): $(MANIFEST)

$(OBJDIR)/probes_c.o: $(PROBES_C) regparm_macros.h regparm_types.h
	@$(MKDIR) $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/probes_cxx.o: $(PROBES_CXX) regparm_macros.h regparm_types.h
	@$(MKDIR) $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/probes_c_norp.o: $(PROBES_C) regparm_macros.h regparm_types.h
	@$(MKDIR) $(OBJDIR)
	$(CC) $(NORP_FLAGS) -I$(ROOT) -c $< -o $@

$(OBJDIR)/probes_cxx_norp.o: $(PROBES_CXX) regparm_macros.h regparm_types.h
	@$(MKDIR) $(OBJDIR)
	$(CXX) $(NORP_FLAGS) -std=c++14 -fno-exceptions -fno-rtti -I$(ROOT) -c $< -o $@

build: generate $(OBJS_REGPARM)

check: build
	@echo "=== objdump register check (-mregparm=4) ==="
	python3 $(ROOT)/check_regparm.py --manifest $(MANIFEST) $(OBJS_REGPARM)

check-norp: generate $(OBJS_NORP)
	@echo "=== objdump register check (no -mregparm) ==="
	python3 $(ROOT)/check_regparm.py --manifest $(MANIFEST) $(OBJS_NORP)

objdump: build
	$(OBJDUMP) -d $(OBJS_REGPARM)

# Amiga runtime binary (optional)
TestRegParm: generate $(OBJDIR)/probes_c.o $(OBJDIR)/probes_cxx.o $(OBJDIR)/runtime_main.o
	@$(MKDIR) $(BINDIR)
	$(CXX) $(CXXFLAGS) $(OBJDIR)/probes_c.o $(OBJDIR)/probes_cxx.o $(OBJDIR)/runtime_main.o \
		-noixemul -o $(BINDIR)/TestRegParm
	@echo "Built $(BINDIR)/TestRegParm"

$(OBJDIR)/runtime_main.o: runtime_main.c regparm_types.h
	@$(MKDIR) $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

squirt: TestRegParm
	SQUIRT_HOST=$${SQUIRT_HOST:-192.168.0.110} \
		$(ROOT)/../../squirt.sh $(BINDIR)/TestRegParm SYS:c/

clean:
	rm -rf $(GEN) $(OBJDIR) $(BINDIR)
