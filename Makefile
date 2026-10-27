# QAR-OS build. CROSS defaults to empty (host gcc in freestanding mode works on x86_64 Linux);
# use `make CROSS=x86_64-elf-` if you have a real cross toolchain.
CROSS ?=
CC     = $(CROSS)gcc
LD     = $(CROSS)ld
NASM   = nasm
BUILD  = build

CFLAGS = -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
         -mno-red-zone -mgeneral-regs-only -mno-mmx -mno-sse -mno-sse2 \
         -Wall -Wextra -Werror -O2 -g -Ikernel
LDFLAGS = -nostdlib -z noexecstack -z max-page-size=0x1000 -T linker.ld

KSRC := $(wildcard kernel/*.c)
KOBJ := $(patsubst kernel/%.c,$(BUILD)/%.o,$(KSRC)) $(BUILD)/boot.o

.PHONY: all iso clean run test
all: iso

$(BUILD)/boot.o: boot/boot.asm | $(BUILD)
	$(NASM) -f elf64 $< -o $@
$(BUILD)/%.o: kernel/%.c kernel/kernel.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/qaros.elf: $(KOBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KOBJ)
$(BUILD):
	mkdir -p $(BUILD)

iso: $(BUILD)/qaros.elf
	rm -rf $(BUILD)/isoroot && cp -r iso $(BUILD)/isoroot
	cp $(BUILD)/qaros.elf $(BUILD)/isoroot/boot/qaros.elf
	grub-mkrescue -o $(BUILD)/qaros.iso $(BUILD)/isoroot 2>/dev/null

run: iso
	qemu-system-x86_64 -cdrom $(BUILD)/qaros.iso -m 256M -serial stdio

test: iso
	scripts/test_qemu.sh $(BUILD)/qaros.iso

clean:
	rm -rf $(BUILD)
