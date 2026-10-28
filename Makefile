# QAR-OS build. CROSS defaults to empty (host gcc in freestanding mode works on x86_64 Linux);
# use `make CROSS=x86_64-elf-` if you have a real cross toolchain.
CROSS ?=
CC     = $(CROSS)gcc
LD     = $(CROSS)ld
NASM   = nasm
BUILD  = build

CFLAGS = -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
         -mno-red-zone -mgeneral-regs-only -mno-mmx -mno-sse -mno-sse2 \
         -Wall -Wextra -Werror -O2 -g -Ikernel -Isdk/include -I$(BUILD)
ED     = third_party/ed25519/src
TPFLAGS = -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -mgeneral-regs-only \
          -mno-mmx -mno-sse -mno-sse2 -O2 -g -I$(ED) -Ikernel
LDFLAGS = -nostdlib -z noexecstack -z max-page-size=0x1000 -T linker.ld

KSRC := $(wildcard kernel/*.c)
KOBJ := $(patsubst kernel/%.c,$(BUILD)/%.o,$(KSRC)) $(BUILD)/boot.o $(BUILD)/isr.o \
        $(patsubst $(ED)/%.c,$(BUILD)/ed_%.o,$(ED)/verify.c $(ED)/ge.c $(ED)/fe.c $(ED)/sc.c $(ED)/sha512.c)
HDRS := $(wildcard kernel/*.h sdk/include/*.h) $(BUILD)/trusted_key.h

.PHONY: all iso clean run test hosttest sdk
all: iso

$(BUILD)/trusted_key.h: scripts/build_apps.sh tools/qar.py $(wildcard sdk/*/* sdk/*/*/*)
	CC=$(CC) scripts/build_apps.sh $(BUILD)

$(BUILD)/boot.o: boot/boot.asm | $(BUILD)
	$(NASM) -f elf64 $< -o $@
$(BUILD)/isr.o: boot/isr.asm | $(BUILD)
	$(NASM) -f elf64 $< -o $@
$(BUILD)/ed_%.o: $(ED)/%.c | $(BUILD)
	$(CC) $(TPFLAGS) -c $< -o $@
$(BUILD)/%.o: kernel/%.c $(HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/qaros.elf: $(KOBJ) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KOBJ)
$(BUILD):
	mkdir -p $(BUILD)

sdk: $(BUILD)/trusted_key.h
	@echo "SDK tarball: $(BUILD)/qar-sdk.tar.gz"

iso: $(BUILD)/qaros.elf
	cp -r iso/boot $(BUILD)/isoroot/
	cp $(BUILD)/qaros.elf $(BUILD)/isoroot/boot/qaros.elf
	grub-mkrescue -o $(BUILD)/qaros.iso $(BUILD)/isoroot 2>/dev/null

run: iso
	qemu-system-x86_64 -cdrom $(BUILD)/qaros.iso -m 256M -serial stdio -netdev user,id=n0 -device e1000,netdev=n0

test: iso
	scripts/test_qemu.sh $(BUILD)/qaros.iso

hosttest:
	tests/host_test.sh

clean:
	rm -rf $(BUILD)
