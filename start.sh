#!/bin/bash

# Constants
DISK_IMG="boot.img"
EFI_DIR="efi/boot"
DISK_SIZE_MB=64
PARTITION_START="1MiB"
PARTITION_END="100%"
GNUEFI_PATH="/home/drap/caly-talk/gnu-efi/x86_64"
EFI_LDS="$GNUEFI_PATH/gnuefi/elf_x86_64_efi.lds"
EFI_CRT_OBJ="$GNUEFI_PATH/gnuefi/crt0-efi-x86_64.o"

# Cleanup function
cleanup() {
    echo "Cleaning up previous build files..."
    rm -f bootloader.o BOOTX64.so BOOTX64.efi qemu.log serial.log debug.log boot.img
    rm -rf "$EFI_DIR"
}

cleanup

# Prepare directories
mkdir -p "$EFI_DIR"

# Compile the UEFI bootloader
echo "Compiling the UEFI bootloader..."
gcc \
    -I"$GNUEFI_PATH/lib" \
    -I"$GNUEFI_PATH/gnuefi" \
    -I/usr/include/efi \
    -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar \
    -mno-red-zone -maccumulate-outgoing-args \
    -c bootloader.c -o bootloader.o

if [ $? -ne 0 ]; then
    echo "Error: Failed to compile the UEFI bootloader"
    exit 1
fi

# Link the bootloader
echo "Linking the UEFI bootloader..."
ld \
    -nostdlib -znocombreloc -T "$EFI_LDS" -shared -Bsymbolic \
    -L"$GNUEFI_PATH/gnuefi" -L"$GNUEFI_PATH/lib" \
    "$EFI_CRT_OBJ" bootloader.o -o BOOTX64.so -lefi -lgnuefi

if [ $? -ne 0 ]; then
    echo "Error: Failed to link the UEFI bootloader"
    exit 1
fi

# Convert the bootloader to EFI format
echo "Converting the UEFI bootloader to EFI format..."
objcopy \
    -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym \
    -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc \
    --target=efi-app-x86_64 --subsystem=10 BOOTX64.so BOOTX64.efi

if [ $? -ne 0 ]; then
    echo "Error: Failed to convert the UEFI bootloader to EFI format"
    exit 1
fi

# Create the disk image
echo "Creating the disk image..."
dd if=/dev/zero of="$DISK_IMG" bs=1M count=$DISK_SIZE_MB status=progress

if [ $? -ne 0 ]; then
    echo "Error: Failed to create the disk image"
    exit 1
fi

# Create partition table
echo "Setting up GPT partition table..."
parted -s "$DISK_IMG" mklabel gpt
parted -s "$DISK_IMG" mkpart primary fat32 $PARTITION_START $PARTITION_END
parted -s "$DISK_IMG" set 1 boot on
parted -s "$DISK_IMG" set 1 esp on

# Setup loop device
echo "Setting up loop device..."
LOOP_DEV=$(sudo losetup -f --show -P "$DISK_IMG")
if [ $? -ne 0 ]; then
    echo "Error: Failed to setup loop device"
    exit 1
fi

# Format the partition
echo "Formatting the EFI System Partition as FAT32..."
sudo mkfs.fat -F 32 "${LOOP_DEV}p1"

if [ $? -ne 0 ]; then
    echo "Error: Failed to format partition"
    sudo losetup -d "$LOOP_DEV"
    exit 1
fi

# Mount and copy files
MOUNT_POINT=$(mktemp -d)
sudo mount "${LOOP_DEV}p1" "$MOUNT_POINT"

echo "Copying UEFI bootloader to EFI partition..."
sudo mkdir -p "$MOUNT_POINT/EFI/BOOT"
sudo cp BOOTX64.efi "$MOUNT_POINT/EFI/BOOT/"

# Cleanup mount and loop device
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
