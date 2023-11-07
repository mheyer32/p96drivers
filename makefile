# compiler and linker
CC = m68k-amigaos-gcc
LD = m68k-amigaos-gcc
STRIP = m68k-amigaos-strip

OUTDIR = _bin/
OBJDIR = _o/

DEBUG ?= 0

OBJS_S3TRIO64_NOPREFIX = s3trio64/chip_s3trio64.o \
						 chip_library.o

OBJS_S3TRIO64 =$(addprefix $(OBJDIR), $(OBJS_S3TRIO64_NOPREFIX))

BUILDFLAGS = -flto -noixemul -nostartfiles -msmall-code -m68020-60 -Os -g -ggdb -fomit-frame-pointer

CFLAGS ?=
CFLAGS +=  $(BUILDFLAGS) -I. -IPicasso96Develop/Include -IPicasso96Develop/PrivateInclude -IPrometheus/include
LDFLAGS ?=
LDFLAGS += $(BUILDFLAGS) -ramiga-lib
LIBS = -lamiga

ifeq ($(DEBUG),1)
	CFLAGS += -DDBG
	LIBS += -ldebug
else
	LDFLAGS += -nodefaultlibs
endif


# target 'all' (default target)

all : $(OUTDIR)S3Trio64Plus.chip

directories :
	mkdir -p $(OBJDIR)
	mkdir -p $(OBJDIR)/s3trio64
	mkdir -p $(OUTDIR)

$(OBJS_S3TRIO64) : | directories

$(OBJDIR)%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUTDIR)S3Trio64Plus.chip : $(OBJS_S3TRIO64)
	$(LD) $(LDFLAGS) -o $@_unstripped $^ $(LIBS)
	$(STRIP) $@_unstripped -o $@

# target 'clean'

clean:
	rm -rf $(OUTDIR)*
	rm -rf $(OBJDIR)*.o
