#!/bin/bash

DISK_IMG="boot.img"
EFI_DIR="efi/boot"
DISK_SIZE_MB=512
PARTITION_START="1MiB"
PARTITION_END="100%"

cleanup() {
    echo "Cleaning up previous build files..."
    rm -f kernel.bin kernel.elf kernel.o BOOTX64.EFI bootloader.obj qemu.log debug.log
    rm -rf "$EFI_DIR"
}
cleanup

mkdir -p "$EFI_DIR"

echo "Assembling bootloader..."
nasm -f elf64 bootloader.asm -o bootloader.obj || { echo "Error assembling bootloader"; exit 1; }

echo "Linking bootloader..."
ld -nostdlib -T /usr/lib/elf_x86_64_efi.lds -shared -Bsymbolic \
   /usr/lib/crt0-efi-x86_64.o bootloader.obj \
   -L/usr/lib -lefi -lgnuefi -o BOOTX64.EFI || { echo "Error linking bootloader"; exit 1; }

echo "Compiling kernel..."
gcc -ffreestanding -fPIC -c kernel.c -o kernel.o || { echo "Error compiling kernel"; exit 1; }

echo "Linking kernel..."
ld -Ttext=0x100000 --oformat=elf64-x86-64 -nostdlib -o kernel.elf kernel.o || { echo "Error linking kernel"; exit 1; }

echo "Converting kernel to binary..."
objcopy -O binary kernel.elf kernel.bin || { echo "Error converting kernel to binary"; exit 1; }

echo "Creating disk image..."
dd if=/dev/zero of="$DISK_IMG" bs=1M count=$DISK_SIZE_MB status=progress || { echo "Error creating disk image"; exit 1; }

echo "Setting up GPT partition table..."
parted -s "$DISK_IMG" mklabel gpt || { echo "Error creating GPT label"; exit 1; }
parted -s "$DISK_IMG" mkpart primary fat32 $PARTITION_START $PARTITION_END || { echo "Error creating partition"; exit 1; }
parted -s "$DISK_IMG" set 1 boot on || { echo "Error setting boot flag"; exit 1; }
parted -s "$DISK_IMG" set 1 esp on || { echo "Error setting esp flag"; exit 1; }

echo "Setting up loop device..."
LOOP_DEV=$(sudo losetup -f --show -P "$DISK_IMG") || { echo "Error setting up loop device"; exit 1; }

echo "Formatting EFI System Partition as FAT32..."
sudo mkfs.fat -F 32 "${LOOP_DEV}p1" || { sudo losetup -d "$LOOP_DEV"; echo "Error formatting partition"; exit 1; }

MOUNT_POINT=$(mktemp -d)
sudo mount "${LOOP_DEV}p1" "$MOUNT_POINT" || { sudo losetup -d "$LOOP_DEV"; echo "Error mounting partition"; exit 1; }

echo "Copying bootloader and kernel to EFI partition..."
sudo mkdir -p "$MOUNT_POINT/EFI/BOOT"
sudo cp BOOTX64.EFI "$MOUNT_POINT/EFI/BOOT/"
sudo cp kernel.bin "$MOUNT_POINT/EFI/BOOT/"

sync
sudo umount "$MOUNT_POINT"
rmdir "$MOUNT_POINT"
sudo losetup -d "$LOOP_DEV"

echo "Running QEMU..."
qemu-system-x86_64 \
    -bios /usr/share/edk2/x64/OVMF_CODE.4m.fd \
    -drive if=pflash,format=raw,file=/usr/share/edk2/x64/OVMF_VARS.4m.fd,readonly=on \
    -drive file="$DISK_IMG",format=raw,media=disk \
    -m 512M \
    -display gtk \
    -d int,cpu_reset

cleanup
echo "Done!"
