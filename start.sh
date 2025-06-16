#!/bin/bash
set -euo pipefail

GNUEFI_PATH="${1:?Usage: $0 <GNUEFI_PATH>}"
[[ -d "$GNUEFI_PATH" ]] || { echo "Error: GNUEFI_PATH not found"; exit 1; }

readonly BUILD_DIR="build"
readonly CACHE_DIR="${BUILD_DIR}/cache"
readonly DISK_IMG="${BUILD_DIR}/boot.img"
readonly DISK_SIZE_MB=256
readonly RAM_SIZE_MB=4096
readonly KERNEL_DIR="kernel"
readonly BOOTLOADER_DIR="bootloader"
readonly EFI_LDS="${GNUEFI_PATH}/gnuefi/elf_x86_64_efi.lds"
readonly EFI_CRT_OBJ="${GNUEFI_PATH}/gnuefi/crt0-efi-x86_64.o"
readonly -a QEMU_PATHS=("/usr/share/OVMF/OVMF_CODE.fd" "/usr/share/qemu/OVMF.fd" "/usr/share/ovmf/OVMF.fd")

readonly CC="gcc"
readonly LD="ld"
readonly AS="nasm"
readonly OBJCOPY="objcopy"

readonly BOOTLOADER_CFLAGS="-I${GNUEFI_PATH}/lib -I${GNUEFI_PATH}/gnuefi -I/usr/include/efi -I${BOOTLOADER_DIR}/include \
    -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar \
    -mno-red-zone -maccumulate-outgoing-args -O2 -Wall -Wextra"

readonly KERNEL_CFLAGS="-m64 -mno-red-zone -ffreestanding -fno-stack-protector \
    -nostdlib -fno-builtin -fno-exceptions -fno-asynchronous-unwind-tables \
    -mno-mmx -mno-sse -mno-sse2 -O2 -Wall -Wextra -I${KERNEL_DIR}/include"

readonly KERNEL_LDFLAGS="-m elf_x86_64 -T linker.ld --oformat binary"

find_ovmf() {
    for path in "${QEMU_PATHS[@]}"; do
        [[ -f "$path" ]] && { echo "$path"; return 0; }
    done
    echo "Error: OVMF.fd not found in standard locations" >&2
    return 1
}

create_directories() {
    mkdir -p "${BUILD_DIR}" "${CACHE_DIR}"
}

compute_hash() {
    find "$1" -type f -exec sha256sum {} + | sha256sum | cut -d' ' -f1
}

needs_rebuild() {
    local target="$1"
    local source_dir="$2"
    local hash_file="${CACHE_DIR}/$(basename "$target").hash"
    
    [[ ! -f "$target" ]] && return 0
    [[ ! -f "$hash_file" ]] && return 0
    
    local current_hash=$(compute_hash "$source_dir")
    local stored_hash=$(cat "$hash_file" 2>/dev/null || echo "")
    
    [[ "$current_hash" != "$stored_hash" ]]
}

save_hash() {
    local target="$1"
    local source_dir="$2"
    local hash_file="${CACHE_DIR}/$(basename "$target").hash"
    
    compute_hash "$source_dir" > "$hash_file"
}

build_bootloader() {
    local target="${BUILD_DIR}/BOOTX64.efi"
    
    if ! needs_rebuild "$target" "$BOOTLOADER_DIR"; then
        echo "Bootloader up to date"
        return 0
    fi
    
    echo "Building bootloader..."
    
    "$CC" $BOOTLOADER_CFLAGS -c "${BOOTLOADER_DIR}/src/bootloader.c" \
        -o "${BUILD_DIR}/bootloader.o"
    
    "$LD" -nostdlib -znocombreloc -T "$EFI_LDS" -shared -Bsymbolic \
        -L"${GNUEFI_PATH}/gnuefi" -L"${GNUEFI_PATH}/lib" \
        "$EFI_CRT_OBJ" "${BUILD_DIR}/bootloader.o" \
        -o "${BUILD_DIR}/BOOTX64.so" -lefi -lgnuefi
    
    "$OBJCOPY" -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym \
        -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc \
        --target=efi-app-x86_64 --subsystem=10 \
        "${BUILD_DIR}/BOOTX64.so" "$target"
    
    save_hash "$target" "$BOOTLOADER_DIR"
}

build_kernel() {
    local target="${BUILD_DIR}/kernel.bin"
    
    if ! needs_rebuild "$target" "$KERNEL_DIR"; then
        echo "Kernel up to date"
        return 0
    fi
    
    echo "Building kernel..."
    
    "$AS" -f elf64 "${KERNEL_DIR}/src/kernel_entry.asm" \
        -o "${BUILD_DIR}/kernel_entry.o"
    
    local objects=("${BUILD_DIR}/kernel_entry.o")
    
    for src in "${KERNEL_DIR}/src"/*.c; do
        [[ -f "$src" ]] || continue
        local obj="${BUILD_DIR}/$(basename "${src%.c}.o")"
        "$CC" $KERNEL_CFLAGS -c "$src" -o "$obj"
        objects+=("$obj")
    done
    
    "$LD" $KERNEL_LDFLAGS -o "$target" "${objects[@]}"
    
    save_hash "$target" "$KERNEL_DIR"
}

create_disk_image() {
    local bootloader="${BUILD_DIR}/BOOTX64.efi"
    local kernel="${BUILD_DIR}/kernel.bin"
    
    [[ -f "$bootloader" ]] || { echo "Error: Bootloader not found"; return 1; }
    [[ -f "$kernel" ]] || { echo "Error: Kernel not found"; return 1; }
    
    echo "Creating disk image..."
    
    dd if=/dev/zero of="$DISK_IMG" bs=1M count=$DISK_SIZE_MB status=none
    
    local sectors_per_track=32
    local heads=64
    local cylinders=$((DISK_SIZE_MB * 1024 * 1024 / (sectors_per_track * heads * 512)))
    
    mformat -i "$DISK_IMG" -h "$heads" -t "$cylinders" -s "$sectors_per_track" ::
    
    mmd -i "$DISK_IMG" ::/EFI ::/EFI/BOOT
    mcopy -i "$DISK_IMG" "$bootloader" ::/EFI/BOOT/
    mcopy -i "$DISK_IMG" "$kernel" ::/EFI/BOOT/
}

launch_qemu() {
    local ovmf_path=$(find_ovmf)
    
    echo "Launching QEMU..."
    
    qemu-system-x86_64 \
        -bios "$ovmf_path" \
        -drive file="$DISK_IMG",format=raw,if=ide \
        -m $RAM_SIZE_MB \
        -machine q35,accel=kvm:tcg \
        -cpu qemu64,+nx \
        -display gtk \
        -monitor stdio \
        -serial file:"${BUILD_DIR}/serial.log" \
        -debugcon file:"${BUILD_DIR}/debug.log" \
        -d guest_errors \
        -no-reboot \
        -no-shutdown
}

main() {
    trap 'echo "Build failed at line $LINENO"' ERR
    
    create_directories
    build_bootloader
    build_kernel
    create_disk_image
    launch_qemu
}

main "$@"