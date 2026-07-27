BUILD_DIR = ../build/kernel

C_SRCS = $(shell find . -type f -name '*.c')
C_OBJS = $(addprefix $(BUILD_DIR)/, $(C_SRCS:.c=.o))

ASM_SRCS = $(shell find . -type f \( -name '*.S' \))
ASM_OBJS = $(addprefix $(BUILD_DIR)/, $(ASM_SRCS:.S=.o))

OBJS = $(C_OBJS) $(ASM_OBJS)
DEPS = $(OBJS:.o=.d)

CC = clang
CFLAGS = -target x86_64-unknown-none-elf -c -g \
         -MMD -MP \
         -I ../include \
         -I arch/x86_64/include/ \
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

ASMFLAGS = $(CFLAGS)

LDFLAGS = -m elf_x86_64 -T arch/x86_64/linker.ld

all: ../kernel.bin

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/kernel.elf: $(OBJS)
	ld.lld $(LDFLAGS) $(OBJS) -o $@

../kernel.bin: $(BUILD_DIR)/kernel.elf
	llvm-objcopy -O binary $< $@

-include $(DEPS)