import os
import sys
import subprocess
import struct
import shutil

build_dir = sys.argv[1]
source_dir = sys.argv[2]

initrd_dir = os.path.join(source_dir, "initrd")
os.makedirs(initrd_dir, exist_ok=True)

subprocess.run(f"cd {source_dir}/initrd && (find . -type d && find . -type f) | cpio -o -H newc > {build_dir}/initrd.cpio", shell=True, check=True)

esp_dir = os.path.join(build_dir, "esp_root")
if os.path.exists(esp_dir):
    shutil.rmtree(esp_dir)

boot_dir = os.path.join(esp_dir, "EFI", "BOOT")
os.makedirs(boot_dir, exist_ok=True)

shutil.copy(os.path.join(build_dir, "boot", "BOOTX64.EFI"), boot_dir)
shutil.copy(os.path.join(build_dir, "kernel", "kernel.elf"), esp_dir)
shutil.copy(os.path.join(build_dir, "initrd.cpio"), esp_dir)

fat_img = os.path.join(build_dir, "fat.img")
if os.path.exists(fat_img):
    os.remove(fat_img)

with open(fat_img, "wb") as f:
    f.truncate(62 * 1024 * 1024)

subprocess.run(f"mformat -i {fat_img} -F ::", shell=True, check=True)
subprocess.run(f"mcopy -i {fat_img} -s {esp_dir}/* ::/", shell=True, check=True)

disk_img = os.path.join(build_dir, "disk.img")
if os.path.exists(disk_img):
    os.remove(disk_img)

with open(disk_img, "wb") as f:
    f.truncate(64 * 1024 * 1024)

subprocess.run(f"sfdisk --no-reread {disk_img} <<EOF\nlabel: gpt\nstart=2048, type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B\nEOF",
    shell=True, check=True, stdout=subprocess.DEVNULL)
subprocess.run(f"dd if={fat_img} of={disk_img} bs=512 seek=2048 conv=notrunc", shell=True, check=True)
os.remove(fat_img)