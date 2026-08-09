posix_uefi_dep = dependency('posix-uefi')

boot_sources = files('main.c')

boot_c_args = [
  '-target', 'x86_64-unknown-windows',
  '-ffreestanding',
  '-fno-stack-protector',
  '-fshort-wchar',
  '-mno-red-zone'
]

boot_link_args = [
  '-target', 'x86_64-unknown-windows',
  '-fuse-ld=lld',
  '-nostdlib',
  '-Wl,-subsystem:efi_application',
  '-Wl,-entry:uefi_init'
]

bootloader_efi = executable('BOOTX64.EFI', boot_sources,
  c_args: boot_c_args,
  link_args: boot_link_args,
  dependencies: [posix_uefi_dep],
  name_prefix: '',
  name_suffix: []
)