#!/bin/bash

# Check if GNUEFI_PATH is provided as an argument
if [ -z "$1" ]; then
    echo "Usage: ./start.sh <GNUEFI_PATH>"
    exit 1
fi

GNUEFI_PATH="$1"

# Constants
BUILD_DIR="build"
DISK_IMG="$BUILD_DIR/boot.img"
DISK_SIZE_MB=256
SECTORS_PER_TRACK=32
HEADS=64
CYLINDERS=$((DISK_SIZE_MB * 1024 * 1024 / (SECTORS_PER_TRACK * HEADS * 512)))
EFI_LDS="$GNUEFI_PATH/gnuefi/elf_x86_64_efi.lds"
EFI_CRT_OBJ="$GNUEFI_PATH/gnuefi/crt0-efi-x86_64.o"
KERNEL_DIR="kernel"

# Cleanup function
cleanup() {
    echo "Cleaning up build files..."
    rm -rf "$BUILD_DIR"
}

cleanup

# Create build directory
mkdir -p "$BUILD_DIR"

# Compile the UEFI bootloader
echo "Compiling the UEFI bootloader..."
gcc \
    -I"$GNUEFI_PATH/lib" \
    -I"$GNUEFI_PATH/gnuefi" \
    -I/usr/include/efi \
    -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar \
    -mno-red-zone -maccumulate-outgoing-args \
    -c bootloader.c -o "$BUILD_DIR/bootloader.o"

if [ $? -ne 0 ]; then
    echo "Error: Failed to compile the UEFI bootloader"
    exit 1
fi

# Link the bootloader
echo "Linking the UEFI bootloader..."
ld \
    -nostdlib -znocombreloc -T "$EFI_LDS" -shared -Bsymbolic \
    -L"$GNUEFI_PATH/gnuefi" -L"$GNUEFI_PATH/lib" \
    "$EFI_CRT_OBJ" "$BUILD_DIR/bootloader.o" -o "$BUILD_DIR/BOOTX64.so" -lefi -lgnuefi

if [ $? -ne 0 ]; then
    echo "Error: Failed to link the UEFI bootloader"
    exit 1
fi

# Convert the bootloader to EFI format
echo "Converting the UEFI bootloader to EFI format..."
objcopy \
    -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym \
    -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc \
    --target=efi-app-x86_64 --subsystem=10 "$BUILD_DIR/BOOTX64.so" "$BUILD_DIR/BOOTX64.efi"

if [ $? -ne 0 ]; then
    echo "Error: Failed to convert the UEFI bootloader to EFI format"
    exit 1
fi

# Compile all kernel source files
echo "Building kernel..."
KERNEL_OBJECTS=""

# Compile main.c first
if [ -f "$KERNEL_DIR/main.c" ]; then
    gcc -ffreestanding -c "$KERNEL_DIR/main.c" -o "$BUILD_DIR/main.o"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to compile main.c"
        exit 1
    fi
    KERNEL_OBJECTS="$BUILD_DIR/main.o"
else
    echo "Error: main.c not found in $KERNEL_DIR"
    exit 1
fi

# Compile the rest of the kernel files
for file in "$KERNEL_DIR"/*.c; do
    if [[ "$file" == "$KERNEL_DIR/main.c" ]]; then
        continue
    fi
    obj="$BUILD_DIR/$(basename "$file" .c).o"
    gcc -ffreestanding -c "$file" -o "$obj"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to compile $file"
        exit 1
    fi
    KERNEL_OBJECTS="$KERNEL_OBJECTS $obj"
done

# Link kernel object files into a single binary
ld -Ttext 0x100000 --oformat binary -nostdlib $KERNEL_OBJECTS -o "$BUILD_DIR/kernel.bin"
if [ $? -ne 0 ]; then
    echo "Error: Failed to link kernel"
    exit 1
fi

# Create the disk image
echo "Creating the disk image..."
dd if=/dev/zero of="$DISK_IMG" bs=1M count=$DISK_SIZE_MB status=progress

if [ $? -ne 0 ]; then
    echo "Error: Failed to create the disk image"
    exit 1
fi

# Format the disk image as FAT32 using mtools
echo "Formatting disk image with FAT32 file system..."
mformat -i "$DISK_IMG" -h "$HEADS" -t "$CYLINDERS" -s "$SECTORS_PER_TRACK" ::

if [ $? -ne 0 ]; then
    echo "Error: Failed to format disk image with FAT32"
    exit 1
fi

# Add boot files to the disk image
echo "Adding bootloader and kernel to disk image..."
mmd -i "$DISK_IMG" ::/EFI
mmd -i "$DISK_IMG" ::/EFI/BOOT
mcopy -i "$DISK_IMG" "$BUILD_DIR/BOOTX64.efi" ::/EFI/BOOT/
mcopy -i "$DISK_IMG" "$BUILD_DIR/kernel.bin" ::/EFI/BOOT/

if [ $? -ne 0 ]; then
    echo "Error: Failed to copy files to disk image"
    exit 1
fi

cp "$DISK_IMG" visualOS.img

cleanup
