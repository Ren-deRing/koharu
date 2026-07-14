name := koharu

boot.o:
	clang -target i386-unknown-none-elf -m16 -ffreestanding -c boot/boot.S -o boot/boot.o

boot.elf: boot.o
	ld.lld -T boot/linker.ld boot/boot.o -o boot/boot.elf

boot.bin: boot.elf
	llvm-objcopy -O binary boot/boot.elf boot.bin

stage2.o:
	clang -target i386-unknown-none-elf -m16 -ffreestanding -c boot/stage2.S -o boot/stage2.o

stage2.elf: stage2.o
	ld.lld -T boot/linker2.ld boot/stage2.o -o boot/stage2.elf

stage2.bin: stage2.elf
	llvm-objcopy -O binary boot/stage2.elf stage2.bin

disk.img: boot.bin stage2.bin
	cat boot.bin stage2.bin > temp.img
	dd if=/dev/zero bs=512 count=20 >> temp.img 2>/dev/null
	dd if=temp.img of=disk.img bs=512 count=16
	rm temp.img

all: disk.img

run: all
	qemu-system-x86_64 disk.img