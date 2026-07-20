name := koharu

all: mkdirs disk.img

boot.o:
	clang -target i386-unknown-none-elf -m16 -ffreestanding -c boot/boot.S -o boot/boot.o

boot.elf: boot.o
	ld.lld -T boot/linker.ld boot/boot.o -o boot/boot.elf

boot.bin: boot.elf
	llvm-objcopy -O binary boot/boot.elf boot.bin

stage2.o:
	clang -target x86_64-unknown-none-elf -c boot/stage2.S -o boot/stage2.o -Os

SRCS = boot/main.c boot/string.c boot/vmm.c boot/ata.c
OBJS = $(SRCS:.c=.o)

%.o: %.c
	clang -target x86_64-unknown-none-elf -c $< -o $@ \
		-ffreestanding \
		-Os \
		-Wall \
		-Wextra \
		-mno-red-zone \
		-mno-mmx \
		-mno-sse \
		-mno-sse2 \
		-fno-builtin \
		-fno-stack-protector \

bootloader.elf: $(OBJS) stage2.o
	ld.lld -T boot/linker2.ld boot/stage2.o $(OBJS) -o boot/bootloader.elf

bootloader.bin: bootloader.elf
	llvm-objcopy -O binary boot/bootloader.elf bootloader.bin

kernel.bin:
	make -C kernel

disk.img: boot.bin bootloader.bin bootbin.cpio
	cat boot.bin bootloader.bin > temp.img
	dd if=/dev/zero bs=512 count=64 >> temp.img 2>/dev/null
	dd if=temp.img of=disk.img bs=512 count=40

	SECTOR_COUNT=$$(( ($$(stat -c %s bootbin.cpio) + 511) / 512 )); \
	echo "8014fac0" | xxd -r -p > bootbin_header.bin; \
	printf "%02x%02x0000" $$((SECTOR_COUNT & 0xFF)) $$(((SECTOR_COUNT >> 8) & 0xFF)) | xxd -r -p >> bootbin_header.bin
	dd if=/dev/zero bs=1 count=504 >> bootbin_header.bin 2>/dev/null

	dd if=bootbin_header.bin of=disk.img bs=512 seek=40 conv=notrunc
	dd if=bootbin.cpio of=disk.img bs=512 seek=41 conv=notrunc
	dd if=/dev/zero bs=512 count=20480 >> disk.img 2>/dev/null
	rm temp.img bootbin_header.bin

bootbin.cpio: kernel.bin
	find initrd/ -depth | cpio -o > initrd.cpio
	cp kernel.bin bootbin/
	cp initrd.cpio bootbin/
	find bootbin/ -depth | cpio -o > bootbin.cpio

mkdirs:
	mkdir -p ./build/ ./bootbin ./initrd

run: all
	qemu-system-x86_64 \
		-m 4G \
		-cpu host \
		-accel kvm \
		-machine q35 \
		-smp 4 \
		-device piix3-ide,id=legacy_ide \
		-drive file=disk.img,format=raw,if=none,id=kodisk \
		-device ide-hd,drive=kodisk,bus=legacy_ide.0,unit=0