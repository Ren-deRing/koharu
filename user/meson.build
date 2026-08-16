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

user_inc = include_directories('include')

init_elf = executable('init',
  'main.c', 'entry.S', 'string.c', 'console.c', 'cpio.c', 'elf.c', 'pager.c', 'vma.c',
  include_directories: user_inc,
  c_args: c_args,
  link_args: ['-fuse-ld=lld', '-nostdlib', '-no-pie', '-Wl,-m,elf_x86_64', '-Wl,-T,' + meson.current_source_dir() / 'linker.ld'],
  name_suffix: 'elf')

mlibc_sysroot = meson.current_source_dir() / '../sysroot/usr/local'
mlibc_include = mlibc_sysroot / 'include'
mlibc_lib = mlibc_sysroot / 'lib'
mlibc_sysdep_include = meson.current_source_dir() / '../mlibc/sysdeps/koharu/include'

child_args = [
  '-ffreestanding',
  '-fno-pie', '-fno-pic',
  '-fno-stack-protector',
  '-fno-builtin',
  '-mno-red-zone',
  '-fno-asynchronous-unwind-tables',
  '-O2', '-Wall', '-Wextra',
  '-DMLIBC_STATIC_BUILD',
  '-nostdlibinc',
  '-isystem', mlibc_include,
  '-isystem', mlibc_sysdep_include,
]

child_elf = executable('child',
  'child.c',
  c_args: child_args,
  link_args: [
    '-fuse-ld=lld', '-nostdlib', '-no-pie', '-Wl,-m,elf_x86_64',
    '-Wl,-T,' + meson.current_source_dir() / 'child.ld',
    mlibc_lib / 'crt0.o',
    mlibc_lib / 'libc.a',
  ],
  name_suffix: 'elf')