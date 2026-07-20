BUILD_DIR = ../build

SRCS = generic.c
OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))

CC = clang
CFLAGS = -target x86_64-unknown-none-elf -c \
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

all: ../kernel.bin

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/kernel.elf: $(OBJS)
	ld.lld -T arch/x86_64/linker.ld $(OBJS) -o $@

../kernel.bin: $(BUILD_DIR)/kernel.elf
	llvm-objcopy -O binary $< $@