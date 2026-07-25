BUILD_DIR = ../build/boot

boot.o:
	@mkdir -p $(BUILD_DIR)
	clang -target i386-unknown-none-elf -m16 -ffreestanding -c boot.S -o $(BUILD_DIR)/boot.o

boot.elf: boot.o
	ld.lld -T linker.ld $(BUILD_DIR)/boot.o -o $(BUILD_DIR)/boot.elf

boot.bin: boot.elf
	llvm-objcopy -O binary $(BUILD_DIR)/boot.elf ../boot.bin

stage2.o:
	clang -target x86_64-unknown-none-elf -c stage2.S -o $(BUILD_DIR)/stage2.o -Os

SRCS = main.c string.c vmm.c ata.c cpio.c
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRCS))

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
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
	ld.lld -T linker2.ld $(BUILD_DIR)/stage2.o $(OBJS) -o $(BUILD_DIR)/bootloader.elf

bootloader.bin: bootloader.elf
	@mkdir -p $(BUILD_DIR)
	llvm-objcopy -O binary $(BUILD_DIR)/bootloader.elf ../bootloader.bin