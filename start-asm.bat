@echo off
setlocal enabledelayedexpansion

rem Define paths
set DISK_IMG=boot.img
set EFI_DIR=efi\boot

rem Clean up previous build files
if exist %DISK_IMG% del %DISK_IMG%
if exist log.txt del log.txt
if exist %EFI_DIR%\bootx64.efi del %EFI_DIR%\bootx64.efi
if exist %EFI_DIR%\kernel.bin del %EFI_DIR%\kernel.bin

rem Assemble the bootloader
echo Assembling the bootloader...
wsl nasm -f bin bootloader.asm -o bootloader.efi
if errorlevel 1 (
    echo Error: Failed to assemble bootloader.
    exit /b 1
)

rem Assemble the kernel
echo Assembling the kernel...
wsl nasm -f bin kernel.asm -o kernel.bin
if errorlevel 1 (
    echo Error: Failed to assemble kernel.
    exit /b 1
)

rem Create a blank disk image
echo Creating the disk image...
wsl dd if=/dev/zero of=%DISK_IMG% bs=512 count=524288
if errorlevel 1 (
    echo Error: Failed to create disk image.
    exit /b 1
)

rem Create GPT partition table and add an EFI System Partition
echo Setting up GPT partition table...
wsl parted %DISK_IMG% --script mklabel gpt
if errorlevel 1 (
    echo Error: Failed to create GPT label.
    exit /b 1
)
wsl parted %DISK_IMG% --script mkpart ESP fat32 1MiB 64MiB
if errorlevel 1 (
    echo Error: Failed to create partition.
    exit /b 1
)
wsl parted %DISK_IMG% --script set 1 boot on
wsl parted %DISK_IMG% --script set 1 esp on
if errorlevel 1 (
    echo Error: Failed to set partition flags.
    exit /b 1
)

rem Format the partition as FAT32
echo Formatting the EFI partition as FAT32...
wsl mkfs.fat -F 32 -n "UEFI_BOOT" %DISK_IMG%
if errorlevel 1 (
    echo Error: Failed to format EFI partition.
    exit /b 1
)

rem Prepare the EFI directory structure
echo Preparing the EFI directory structure...
if not exist %EFI_DIR% mkdir %EFI_DIR%
copy bootloader.efi %EFI_DIR%\bootx64.efi
copy kernel.bin %EFI_DIR%\kernel.bin

rem Copy files to the disk image
echo Adding files to the FAT32 image...
wsl mcopy -i %DISK_IMG% -s efi ::
if errorlevel 1 (
    echo Error: Failed to copy files to the disk image.
    exit /b 1
)

rem Run the disk image in QEMU
echo Running the UEFI bootloader in QEMU...
wsl qemu-system-x86_64 -drive file=%DISK_IMG%,format=raw -bios /usr/share/OVMF/OVMF_CODE.fd -serial file:log.txt

@echo on
