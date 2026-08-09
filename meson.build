project('koharu', 'c', version: '0.1', meson_version: '>=1.4.0')

subdir('boot')
subdir('kernel')

build_disk_script = find_program('python3')

disk_img = custom_target('disk.img',
  input: [bootloader_efi, kernel_elf],
  output: 'disk.img',
  console: true,
  command: [
    build_disk_script,
    meson.current_source_dir() / 'make_disk.py',
    meson.current_build_dir(),
    meson.current_source_dir()
  ]
)

qemu = find_program('qemu-system-x86_64')
run_target('run',
  command: [
    qemu,
    '-drive', 'file=' + disk_img.full_path() + ',format=raw',
    '-m', '4G',
    '-cpu', 'host,migratable=off,invtsc=on,tsc-freq=2500000000',
    '-accel', 'kvm',
    '-machine', 'q35',
    '-smp', '4',
    '-drive', 'if=pflash,format=raw,readonly=on,file=/usr/share/ovmf/OVMF.fd',
    '-serial', 'stdio'
  ],
  depends: disk_img
)