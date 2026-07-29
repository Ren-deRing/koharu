objcopy = find_program('llvm-objcopy')

boot_o = custom_target('boot.o',
  input: 'boot.S',
  output: 'boot.o',
  command: ['clang', '-target', 'i386-unknown-none-elf', '-m16', '-ffreestanding', '-c', '@INPUT@', '-o', '@OUTPUT@']
)

boot_elf = custom_target('boot.elf',
  input: boot_o,
  output: 'boot.elf',
  command: ['ld.lld', '-T', meson.current_source_dir() / 'linker.ld', '@INPUT@', '-o', '@OUTPUT@']
)

boot_bin = custom_target('boot.bin',
  input: boot_elf,
  output: 'boot.bin',
  command: [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@']
)

boot_c_args = [
  '-target', 'x86_64-unknown-none-elf',
  '-ffreestanding',
  '-Os', '-Wall', '-Wextra',
  '-mno-red-zone', '-mno-mmx', '-mno-sse', '-mno-sse2',
  '-fno-builtin', '-fno-stack-protector',
  '-fno-pie', '-fno-pic'
]

bootloader_sources = files(
  'stage2.S',
  'main.c',
  'string.c',
  'vmm.c',
  'ata.c',
  'cpio.c'
)

bootloader_elf = executable('bootloader', bootloader_sources,
  c_args: boot_c_args,
  link_args: [
    '-fuse-ld=lld',
    '-nostdlib',
    '-no-pie',
    '-Wl,-T,' + meson.current_source_dir() / 'linker2.ld'
  ],
  name_suffix: 'elf'
)

bootloader_bin = custom_target('bootloader.bin',
  input: bootloader_elf,
  output: 'bootloader.bin',
  command: [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@']
)