kernel_inc = include_directories('../include', 'arch/x86_64/include')

kernel_c_args = [
  '-target', 'x86_64-unknown-none-elf',
  '-mcmodel=kernel',
  '-ffreestanding',
  '-fno-pie',
  '-fno-pic',
  '-O2', '-Wall', '-Wextra',
  '-mno-red-zone', '-mno-mmx', '-mno-sse', '-mno-sse2',
  '-fno-builtin', '-fno-stack-protector'
]

kernel_sources = files(
  'generic.c',
  'mm/kmem.c',
  'mm/pmm.c',
  'mm/vmm.c',
  'misc/kpanic.c',
  'misc/kprintf.c',
  'dev/acpi.c',
  'arch/x86_64/cpu.c',
  'arch/x86_64/idt.c',
  'arch/x86_64/pmap.c',
  'arch/x86_64/gdt.c',
  'arch/x86_64/uart.c',
  'arch/x86_64/io.c',
  'arch/x86_64/irq.S',
  'arch/x86_64/lapic.c',
  'arch/x86_64/thread.c',
  'lib/tlsf.c',
  'lib/string.c'
)

kernel_elf = executable('kernel', kernel_sources,
  include_directories: kernel_inc,
  c_args: kernel_c_args,
  link_args: [
    '-fuse-ld=lld',
    '-nostdlib',
    '-no-pie',
    '-Wl,-m,elf_x86_64',
    '-Wl,-T,' + meson.current_source_dir() / 'arch/x86_64/linker.ld'
  ],
  name_suffix: 'elf'
)