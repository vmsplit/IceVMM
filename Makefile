CROSS_PREFIX=aarch64-none-elf-

CC=$(CROSS_PREFIX)gcc
AS=$(CROSS_PREFIX)as
LD=$(CROSS_PREFIX)ld
OBJCOPY=$(CROSS_PREFIX)objcopy

CFLAGS=-Wall -Wextra -std=c11 -O2 -nostdlib -ffreestanding -g
ASFLAGS=-g

TARGET_ELF=hypervisor.elf
TARGET_BIN=hypervisor.bin

SRCS_C=$(wildcard src/*.c)
SRCS_S=$(wildcard src/*.S)
OBJS=$(SRCS_C:.c=.o) $(SRCS_S:.S=.o)

.PHONY: all clean run

all: $(TARGET_ELF) $(TARGET_BIN)

$(TARGET_ELF): $(OBJS) linker.ld
	$(LD) -T linker.ld -o $@ $(OBJS)

$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -f $(TARGET_ELF) $(TARGET_BIN) $(OBJS)

run: all
	qemu-system-aarch64 \
	    -machine virt \
	    -cpu cortex-a57 \
	    -m 1024 \
	    -nographic \
	    -kernel $(TARGET_ELF)
