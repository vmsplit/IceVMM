# Toolchain selection (default to aarch64-none-elf- if not set)
CROSS_COMPILE ?= aarch64-none-elf-

CC      = $(CROSS_COMPILE)gcc
AS      = $(CROSS_COMPILE)as
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

# Build flags
CFLAGS  = -Wall -Wextra -std=c11 -O2 -nostdlib -ffreestanding -g -mcmodel=large -Iinclude
ASFLAGS = -g

# Targets
TARGET_ELF = hypervisor.elf
TARGET_BIN = hypervisor.bin

# Source files (explicit, not wildcard, for determinism)
SRCS_C = src/main.c src/uart.c src/mm.c src/sched.c src/timer.c
SRCS_S = src/boot.S src/exception.S src/sysregs.S src/vcpu.S

# Guest VM binary
GUEST_S     = src/guest.S
GUEST_O     = $(GUEST_S:.S=.o)
GUEST_BIN   = src/guest.bin
GUEST_BIN_O = guest_bin.o

# Object files for hypervisor
OBJS_C = $(SRCS_C:.c=.o)
OBJS_S = $(SRCS_S:.S=.o)
OBJS   = $(OBJS_C) $(OBJS_S) $(GUEST_BIN_O)

# CORRECTED: linker.ld is in the root directory, not src/
LDSCRIPT = linker.ld

.PHONY: all clean run

all: $(TARGET_BIN)

$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@

# Linker command for the hypervisor ELF file
$(TARGET_ELF): $(OBJS) $(LDSCRIPT)
	$(CC) $(CFLAGS) -T $(LDSCRIPT) -o $@ $(OBJS) -lgcc

# Standard C/ASM compilation
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

# Guest VM build pipeline
$(GUEST_O): $(GUEST_S)
	$(AS) $(ASFLAGS) $< -o $@

$(GUEST_BIN): $(GUEST_O)
	$(OBJCOPY) -O binary $< $@

$(GUEST_BIN_O): $(GUEST_BIN)
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 $< $@
	$(OBJCOPY) --redefine-sym _binary_src_guest_bin_start=_guest_bin_start $@ $@
	$(OBJCOPY) --redefine-sym _binary_src_guest_bin_end=_guest_bin_end $@ $@
	$(OBJCOPY) --redefine-sym _binary_src_guest_bin_size=_guest_bin_sz $@ $@

clean:
	rm -f $(OBJS_C) $(OBJS_S) $(GUEST_BIN_O) $(TARGET_ELF) $(TARGET_BIN) $(GUEST_O) $(GUEST_BIN)

# Your QEMU command looks good. Let's keep your ELF target for debugging.
run: all
	qemu-system-aarch64 \
		-machine virt,virtualization=on \
		-cpu cortex-a57 \
		-smp 1 \
		-m 2G \
		-nographic \
		-kernel $(TARGET_ELF)
