c_args = [
  '-target', 'x86_64-unknown-none-elf',
  '-ffreestanding',
  '-fno-pie', '-fno-pic',
  '-fno-stack-protector',
  '-fno-builtin',
  '-mno-red-zone',
  '-fno-asynchronous-unwind-tables',
  '-O2', '-Wall', '-Wextra'
]

init_elf = executable('init', 'main.c',
  c_args: c_args,
  link_args: ['-fuse-ld=lld', '-nostdlib', '-no-pie', '-Wl,-m,elf_x86_64', '-Wl,-T,' + meson.current_source_dir() / 'linker.ld'],
  name_suffix: 'elf')