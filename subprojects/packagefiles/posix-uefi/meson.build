project('posix-uefi', 'c')

fs = import('fs')

posix_uefi_sources = files(
  'uefi/crt_x86_64.c',
  'uefi/dirent.c',
  'uefi/qsort.c',
  'uefi/stat.c',
  'uefi/stdio.c',
  'uefi/stdlib.c',
  'uefi/string.c',
  'uefi/time.c',
  'uefi/unistd.c'
)

posix_uefi_inc = include_directories('uefi')

posix_uefi_dep = declare_dependency(
  sources: posix_uefi_sources,
  include_directories: posix_uefi_inc
)

meson.override_dependency('posix-uefi', posix_uefi_dep)