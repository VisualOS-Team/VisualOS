#!/bin/bash

# Constants
DISK_IMG="boot.img"
EFI_DIR="efi/boot"
DISK_SIZE_MB=512
PARTITION_START="1MiB"
PARTITION_END="100%"

# Cleanup
cleanup() {
    echo "Cleaning up previous build files..."
    rm -f kernel.bin kernel.elf kernel.o qemu.log debug.log 
    rm -rf "$EFI_DIR"
}
cleanup

# Prepare directories
mkdir -p "$EFI_DIR"

# Assemble bootloader
echo "Assembling bootloader..."
nasm -f elf64 bootloader.asm -o bootloader.obj
if [ $? -ne 0 ]; then
    echo "Error: Failed to assemble bootloader"
    exit 1
fi

# Link bootloader
echo "Linking bootloader..."
ld \
    -nostdlib \
    -T /usr/lib/elf_x86_64_efi.lds \
    -shared \
    -Bsymbolic \
    -L/usr/lib/x86_64-linux-gnu \
    -l:crt0-efi-x86_64.o \
    -l:libefi.a \
    -l:libgnuefi.a \
    -o BOOTX64.EFI

if [ $? -ne 0 ]; then
    echo "Error: Failed to link bootloader"
    exit 1
fi

# Compile the kernel
echo "Compiling the C kernel..."
gcc -ffreestanding -fPIC -c kernel.c -o kernel.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile kernel"
    exit 1
fi

# Link the kernel
echo "Linking the kernel..."
ld -Ttext=0x100000 --oformat=elf64-x86-64 -nostdlib -o kernel.elf kernel.o
if [ $? -ne 0 ]; then
    echo "Error: Failed to link kernel"
    exit 1
fi

# Convert kernel to binary
echo "Converting the kernel to binary format..."
objcopy -O binary kernel.elf kernel.bin
if [ $? -ne 0 ]; then
    echo "Error: Failed to convert kernel to binary"
    exit 1
fi

# Create the disk image
echo "Creating disk image..."
dd if=/dev/zero of="$DISK_IMG" bs=1M count=$DISK_SIZE_MB status=progress
if [ $? -ne 0 ]; then
    echo "Error: Failed to create disk image"
    exit 1
fi

# Create partition table
echo "Setting up GPT partition table..."
parted -s "$DISK_IMG" mklabel gpt
if [ $? -ne 0 ]; then
    echo "Error: Failed to create GPT label"
    exit 1
fi

echo "Creating EFI System Partition..."
parted -s "$DISK_IMG" mkpart primary fat32 $PARTITION_START $PARTITION_END
if [ $? -ne 0 ]; then
    echo "Error: Failed to create partition"
    exit 1
fi

# Set flags
parted -s "$DISK_IMG" set 1 boot on
parted -s "$DISK_IMG" set 1 esp on
if [ $? -ne 0 ]; then
    echo "Error: Failed to set partition flags"
    exit 1
fi

# Loop device setup
echo "Setting up loop device..."
LOOP_DEV=$(sudo losetup -f --show -P "$DISK_IMG")
if [ $? -ne 0 ]; then
    echo "Error: Failed to setup loop device"
    exit 1
fi

# Format the partition
echo "Formatting EFI System Partition as FAT32..."
sudo mkfs.fat -F 32 "${LOOP_DEV}p1"
if [ $? -ne 0 ]; then
    echo "Error: Failed to format partition"
    sudo losetup -d "$LOOP_DEV"
    exit 1
fi

# Mount and copy files
MOUNT_POINT=$(mktemp -d)
sudo mount "${LOOP_DEV}p1" "$MOUNT_POINT"

echo "Copying bootloader and kernel to EFI partition..."
sudo mkdir -p "$MOUNT_POINT/EFI/BOOT"
sudo cp BOOTX64.EFI "$MOUNT_POINT/EFI/BOOT/"
sudo cp kernel.bin "$MOUNT_POINT/EFI/BOOT/"

# Cleanup
sync
sudo umount "$MOUNT_POINT"
rmdir "$MOUNT_POINT"
sudo losetup -d "$LOOP_DEV"

# Run the disk image
echo "Starting QEMU..."
qemu-system-x86_64 \
    -bios /usr/share/OVMF/OVMF_CODE.fd \
    -drive file="$DISK_IMG",format=raw,media=disk \
    -net none \
    -m 512M \
    -machine q35 \
    -monitor stdio \
    -serial file:serial.log \
    -debugcon file:debug.log \
    -display gtk

echo "Done!"
cleanup
