name := koharu

boot.o:
	clang -target i386-unknown-none-elf -m16 -ffreestanding -c boot/boot.S -o boot/boot.o

boot.elf: boot.o
	ld.lld -T boot/linker.ld boot/boot.o -o boot/boot.elf

boot.bin: boot.elf
	llvm-objcopy -O binary boot/boot.elf boot.bin

stage2.o:
	clang -target x86_64-unknown-none-elf -c boot/stage2.S -o boot/stage2.o -Os

SRCS = boot/main.c boot/string.c boot/vmm.c
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

disk.img: boot.bin bootloader.bin
	cat boot.bin bootloader.bin > temp.img
	dd if=/dev/zero bs=512 count=64 >> temp.img 2>/dev/null
	dd if=temp.img of=disk.img bs=512 count=40
	rm temp.img

all: disk.img

run: all
	qemu-system-x86_64 disk.img -m 8G