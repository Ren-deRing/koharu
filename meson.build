project('koharu', 'c', version: '0.1', meson_version: '>=1.4.0')

python3 = find_program('python3')

mlibc_sources = run_command(
    python3, meson.current_source_dir() / 'scripts/list_mlibc_files.py',
    meson.current_source_dir(),
    meson.current_source_dir() / 'mlibc' / 'options',
    meson.current_source_dir() / 'mlibc' / 'abis',
    meson.current_source_dir() / 'mlibc' / 'sysdeps' / 'koharu',
    meson.current_source_dir() / 'mlibc' / 'subprojects',
    meson.current_source_dir() / 'mlibc' / 'meson.build',
    meson.current_source_dir() / 'mlibc' / 'meson_options.txt',
    meson.current_source_dir() / 'mlibc' / 'internal-config.h.in',
    meson.current_source_dir() / 'mlibc' / 'mlibc-config.h.in',
    check: true,
).stdout().split()
mlibc_sources += [meson.current_source_dir() / 'cross_x86_64.ini']

mlibc_stamp = custom_target('mlibc-sysroot',
  input: mlibc_sources,
  output: 'mlibc-sysroot.stamp',
  build_by_default: true,
  console: true,
  command: ['bash', meson.current_source_dir() / 'scripts/build_mlibc.sh', '@OUTPUT@'],
)

subdir('boot')
subdir('kernel')
subdir('user')

build_disk_script = find_program('python3')

disk_img = custom_target('disk.img',
  input: [bootloader_efi, kernel_elf, init_elf, child_elf],
  output: 'disk.img',
  console: true,
  command: [
    build_disk_script,
    meson.current_source_dir() / 'make_disk.py',
    meson.current_build_dir(),
    meson.current_source_dir(),
    init_elf.full_path(),
    child_elf.full_path(),
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