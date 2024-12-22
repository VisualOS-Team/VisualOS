#!/bin/bash

# Define paths and constants
DISK_IMG="boot.img"
EFI_DIR="efi/boot"
DISK_SIZE_MB=256      # Total disk size in MiB
PART_START=1          # Start of EFI partition (in MiB)
PART_END=200          # End of EFI partition (in MiB)

# Clean up previous build files
rm -f "$DISK_IMG" log.txt bootloader.efi
rm -rf "$EFI_DIR"

# Create necessary directories
mkdir -p "$EFI_DIR"

# Assemble the bootloader
echo "Assembling the bootloader..."
nasm -f win64 bootloader.asm -o bootloader.obj
if [ $? -ne 0 ]; then
    echo "Error: Failed to assemble bootloader."
    exit 1
fi

echo "Linking the bootloader to create EFI executable..."
ld -o bootloader.efi bootloader.obj --oformat pei-x86-64
if [ $? -ne 0 ]; then
    echo "Error: Failed to link bootloader."
    exit 1
fi

# Assemble the kernel
echo "Assembling the kernel..."
nasm -f bin kernel.asm -o kernel.bin
if [ $? -ne 0 ]; then
    echo "Error: Failed to assemble kernel."
    exit 1
fi

# Create a blank disk image
echo "Creating the disk image..."
dd if=/dev/zero of="$DISK_IMG" bs=1M count=$DISK_SIZE_MB
if [ $? -ne 0 ]; then
    echo "Error: Failed to create disk image."
    exit 1
fi

# Create GPT partition table
echo "Setting up GPT partition table..."
parted "$DISK_IMG" --script mklabel gpt
if [ $? -ne 0 ]; then
    echo "Error: Failed to create GPT label."
    exit 1
fi

# Create EFI System Partition (ESP)
echo "Creating EFI System Partition..."
parted "$DISK_IMG" --script mkpart primary fat32 ${PART_START}MiB ${PART_END}MiB
if [ $? -ne 0 ]; then
    echo "Error: Failed to create partition."
    exit 1
fi

# Set the ESP flag
parted "$DISK_IMG" --script set 1 esp on
if [ $? -ne 0 ]; then
    echo "Error: Failed to set ESP flag."
    exit 1
fi

# Setup loopback device
echo "Setting up loopback device..."
LOOP_DEV=$(losetup -f)
sudo losetup -P "$LOOP_DEV" "$DISK_IMG"

# Format the EFI partition as FAT32
echo "Formatting the EFI partition as FAT32..."
sudo mkfs.fat -F 32 "${LOOP_DEV}p1"
if [ $? -ne 0 ]; then
    echo "Error: Failed to format EFI partition."
    sudo losetup -d "$LOOP_DEV"
    exit 1
fi

# Mount the EFI partition
MOUNT_POINT=$(mktemp -d)
sudo mount "${LOOP_DEV}p1" "$MOUNT_POINT"

# Copy bootloader and kernel to EFI directory
echo "Copying files to EFI partition..."
sudo mkdir -p "$MOUNT_POINT/EFI/BOOT"
sudo cp bootloader.efi "$MOUNT_POINT/EFI/BOOT/BOOTX64.EFI"
sudo cp kernel.bin "$MOUNT_POINT/EFI/BOOT/"

# Cleanup
echo "Cleaning up..."
sync
sudo umount "$MOUNT_POINT"
rmdir "$MOUNT_POINT"
sudo losetup -d "$LOOP_DEV"

# Run the disk image in QEMU
echo "Running the UEFI bootloader in QEMU..."
qemu-system-x86_64 \
    -drive file="$DISK_IMG",format=raw \
    -bios /usr/share/OVMF/OVMF_CODE.fd \
    -m 512M \
    -serial file:log.txt
