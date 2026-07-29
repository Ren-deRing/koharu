import os
import sys
import subprocess
import struct

build_dir = sys.argv[1]
source_dir = sys.argv[2]

initrd_dir = os.path.join(source_dir, "initrd")
os.makedirs(initrd_dir, exist_ok=True)

subprocess.run(f"cd {source_dir}/initrd && (find . -type d && find . -type f) | cpio -o -H newc > {build_dir}/initrd.cpio", shell=True, check=True)

bootbin_dir = f"{build_dir}/bootbin_tmp"
os.makedirs(bootbin_dir, exist_ok=True)
subprocess.run(f"cp {build_dir}/kernel/kernel.elf {bootbin_dir}/", shell=True, check=True)
subprocess.run(f"cp {build_dir}/initrd.cpio {bootbin_dir}/", shell=True, check=True)
subprocess.run(f"cd {bootbin_dir} && (find . -type d && find . -type f) | cpio -o -H newc > {build_dir}/bootbin.cpio", shell=True, check=True)

boot_bin = open(f"{build_dir}/boot/boot.bin", "rb").read()
bootloader_bin = open(f"{build_dir}/boot/bootloader.bin", "rb").read()
bootbin_cpio = open(f"{build_dir}/bootbin.cpio", "rb").read()

temp_img = (boot_bin + bootloader_bin).ljust(40 * 512, b'\x00') # 40 Sectors

sector_count = (len(bootbin_cpio) + 511) // 512
header = bytes.fromhex("8014fac0") + struct.pack("<H", sector_count) + b"\x00\x00"
header = header.ljust(512, b'\x00')

with open(f"{build_dir}/disk.img", "wb") as f:
    f.write(temp_img[:40*512]) # Sector 0~39
    f.write(header)            # Sector 40
    f.write(bootbin_cpio)      # Sector 41~