BUILD_DIR = ../build

SRCS = $(shell find . -type f -name '*.c')
OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))

CC = clang
CFLAGS = -target x86_64-unknown-none-elf -c -g \
         -I ../include \
         -mcmodel=kernel \
         -ffreestanding \
         -O2 \
         -Wall \
         -Wextra \
         -mno-red-zone \
         -mno-mmx \
         -mno-sse \
         -mno-sse2 \
         -fno-builtin \
         -fno-stack-protector

LDFLAGS = -m elf_x86_64 -T arch/x86_64/linker.ld

all: ../kernel.bin

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/kernel.elf: $(OBJS)
	ld.lld $(LDFLAGS) $(OBJS) -o $@

../kernel.bin: $(BUILD_DIR)/kernel.elf
	llvm-objcopy -O binary $< $@