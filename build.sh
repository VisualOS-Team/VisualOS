#!/bin/bash

# Check if GNUEFI_PATH is provided as an argument
if [ -z "$1" ]; then
    echo "Usage: ./start.sh <GNUEFI_PATH>"
    exit 1
fi

GNUEFI_PATH="$1"

# Constants
RAM_SIZE_MB=4096
BUILD_DIR="build"
DISK_IMG="$BUILD_DIR/boot.img"
DISK_SIZE_MB=256
SECTORS_PER_TRACK=32
HEADS=64
CYLINDERS=$((DISK_SIZE_MB * 1024 * 1024 / (SECTORS_PER_TRACK * HEADS * 512)))
EFI_LDS="$GNUEFI_PATH/gnuefi/elf_x86_64_efi.lds"
EFI_CRT_OBJ="$GNUEFI_PATH/gnuefi/crt0-efi-x86_64.o"
TRACE_OUTPUT="$BUILD_DIR/cpu_trace.log"
KERNEL_DIR="kernel"
LINKER_SCRIPT="linker.ld"

# Cleanup function
cleanup() {
    echo "Cleaning up build files..."
    rm -rf "$BUILD_DIR"
}

cleanup
mkdir -p "$BUILD_DIR"

##############################################################################
# 1. Build the 64-bit UEFI Bootloader
##############################################################################
echo "Compiling bootloader.c..."
gcc -I"$GNUEFI_PATH/lib" -I"$GNUEFI_PATH/gnuefi" -I/usr/include/efi \
    -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar \
    -mno-red-zone -maccumulate-outgoing-args \
    -c "bootloader.c" -o "$BUILD_DIR/main.o"
if [ $? -ne 0 ]; then
    echo "Error: Failed to compile bootloader.c"
    exit 1
fi

echo "Linking the UEFI bootloader..."
ld -nostdlib -znocombreloc -T "$EFI_LDS" -shared -Bsymbolic \
    -L"$GNUEFI_PATH/gnuefi" -L"$GNUEFI_PATH/lib" \
    "$EFI_CRT_OBJ" "$BUILD_DIR/main.o" -o "$BUILD_DIR/BOOTX64.so" -lefi -lgnuefi
if [ $? -ne 0 ]; then
    echo "Error: Failed to link bootloader"
    exit 1
fi

echo "Converting bootloader to EFI format..."
objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym \
    -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc \
    --target=efi-app-x86_64 --subsystem=10 "$BUILD_DIR/BOOTX64.so" "$BUILD_DIR/BOOTX64.efi"
if [ $? -ne 0 ]; then
    echo "Error: Failed to convert bootloader to EFI"
    exit 1
fi

##############################################################################
# 2. Build the 64-bit Kernel (raw binary)
##############################################################################
echo "Assembling kernel_entry.asm..."
nasm -f elf64 "$KERNEL_DIR/kernel_entry.asm" -o "$BUILD_DIR/kernel_entry.o"
if [ $? -ne 0 ]; then
    echo "Error: Failed to assemble kernel_entry.asm"
    exit 1
fi

echo "Compiling kernel C files..."
KERNEL_OBJECTS=""
if [ -f "$KERNEL_DIR/main.c" ]; then
    gcc -m64 -mno-red-zone -ffreestanding -fno-stack-protector -c "$KERNEL_DIR/main.c" -o "$BUILD_DIR/main_kernel.o"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to compile kernel main.c"
        exit 1
    fi
    KERNEL_OBJECTS="$BUILD_DIR/main_kernel.o"
else
    echo "Error: $KERNEL_DIR/main.c not found."
    exit 1
fi

# Compile any additional kernel .c files
for file in "$KERNEL_DIR"/*.c; do
    if [[ "$file" == "$KERNEL_DIR/main.c" ]]; then
        continue
    fi
    obj="$BUILD_DIR/$(basename "$file" .c).o"
    gcc -m64 -mno-red-zone -ffreestanding -fno-stack-protector -c "$file" -o "$obj"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to compile $file"
        exit 1
    fi
    KERNEL_OBJECTS="$KERNEL_OBJECTS $obj"
done

echo "Linking kernel into raw binary..."
ld -m elf_x86_64 -T "$LINKER_SCRIPT" -o "$BUILD_DIR/kernel.bin" --oformat binary \
    "$BUILD_DIR/kernel_entry.o" $KERNEL_OBJECTS
if [ $? -ne 0 ]; then
    echo "Error: Failed to link kernel"
    exit 1
fi

##############################################################################
# 3. Create & Format Disk Image and Copy Files
##############################################################################
echo "Creating disk image..."
dd if=/dev/zero of="$DISK_IMG" bs=1M count=$DISK_SIZE_MB status=progress
if [ $? -ne 0 ]; then
    echo "Error: Failed to create disk image"
    exit 1
fi

echo "Formatting disk image as FAT32..."
mformat -i "$DISK_IMG" -h "$HEADS" -t "$CYLINDERS" -s "$SECTORS_PER_TRACK" ::
if [ $? -ne 0 ]; then
    echo "Error: Failed to format disk image"
    exit 1
fi

echo "Adding bootloader and kernel to disk image..."
mmd -i "$DISK_IMG" ::/EFI
mmd -i "$DISK_IMG" ::/EFI/BOOT
mcopy -i "$DISK_IMG" "$BUILD_DIR/BOOTX64.efi" ::/EFI/BOOT/
mcopy -i "$DISK_IMG" "$BUILD_DIR/kernel.bin" ::/EFI/BOOT/
if [ $? -ne 0 ]; then
    echo "Error: Failed to copy files to disk image"
    exit 1
fi

##############################################################################
# 4. Launch QEMU
##############################################################################
echo "Starting QEMU..."
qemu-system-x86_64 -bios /usr/share/OVMF/OVMF_CODE.fd \
    -drive file="$DISK_IMG",format=raw,media=disk \
    -net none -m $RAM_SIZE_MB -machine q35 \
    -monitor stdio -serial file:"$BUILD_DIR/serial.log" \
    -debugcon file:"$BUILD_DIR/debug.log" -display gtk \
    -d int,cpu_reset,guest_errors -D "$TRACE_OUTPUT"
if [ $? -ne 0 ]; then
    echo "Error: QEMU execution failed"
    exit 1
fi

echo "Done!"
cleanup
