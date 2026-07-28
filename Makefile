name := koharu

all: mkdirs disk.img

kernel.elf:
	make -C kernel

boot.bin:
	make -C boot boot.bin

bootloader.bin:
	make -C boot bootloader.bin

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
	rm temp.img bootbin_header.bin

bootbin.cpio: kernel.elf
	cd initrd && (find . -type d && find . -type f) | cpio -o -H newc > ../initrd.cpio
	cp kernel.elf bootbin/
	mv initrd.cpio bootbin/
	cd bootbin && (find . -type d && find . -type f) | cpio -o -H newc > ../bootbin.cpio

mkdirs:
	mkdir -p ./build/ ./bootbin ./initrd

.PHONY: all mkdirs run clean kernel.elf boot.bin bootloader.bin

run: all
	qemu-system-x86_64 \
		-m 4G \
		-cpu host,migratable=off,invtsc=on,tsc-freq=2500000000 \
		-accel kvm \
		-machine q35 \
		-smp 4 \
		-device piix3-ide,id=legacy_ide \
		-drive file=disk.img,format=raw,if=none,id=kodisk \
		-device ide-hd,drive=kodisk,bus=legacy_ide.0,unit=0 -serial stdio