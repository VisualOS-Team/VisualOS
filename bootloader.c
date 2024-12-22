#include <efi.h>
#include <efilib.h>

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *Root, *KernelFile;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_PHYSICAL_ADDRESS KernelBase = 0x100000; // Kernel load address
    UINTN KernelSize;
    VOID *KernelBuffer;

    // Initialize UEFI library
    InitializeLib(ImageHandle, SystemTable);
    Print(L"UEFI Bootloader Starting...\n");

    // Get Loaded Image Protocol
    Status = uefi_call_wrapper(gBS->HandleProtocol, 3, ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get Loaded Image Protocol: %r\n", Status);
        return Status;
    }

    // Open Root Volume
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    Status = uefi_call_wrapper(gBS->HandleProtocol, 3, LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&FileSystem);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open file system: %r\n", Status);
        return Status;
    }
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open root volume: %r\n", Status);
        return Status;
    }

    // Open Kernel File
    Status = uefi_call_wrapper(Root->Open, 5, Root, &KernelFile, L"\\EFI\\BOOT\\kernel.bin", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open kernel file: %r\n", Status);
        return Status;
    }

    // Get Kernel File Info
    EFI_FILE_INFO *FileInfo;
    UINTN FileInfoSize = 0;
    Status = uefi_call_wrapper(KernelFile->GetInfo, 4, KernelFile, &gEfiFileInfoGuid, NULL, &FileInfoSize);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        Print(L"Failed to get kernel file info size: %r\n", Status);
        return Status;
    }
    FileInfo = AllocatePool(FileInfoSize);
    Status = uefi_call_wrapper(KernelFile->GetInfo, 4, KernelFile, &gEfiFileInfoGuid, FileInfo, &FileInfoSize);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to get kernel file info: %r\n", Status);
        FreePool(FileInfo);
        return Status;
    }

    KernelSize = FileInfo->FileSize;
    FreePool(FileInfo);

    // Allocate Memory for Kernel
    Print(L"Allocating %lu bytes for kernel at 0x%lx\n", KernelSize, KernelBase);
    Status = uefi_call_wrapper(gBS->AllocatePages, 4, AllocateAddress, EfiLoaderData, (KernelSize + 0xFFF) / 0x1000, &KernelBase);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory for kernel: %r\n", Status);
        return Status;
    }

    // Load Kernel into Memory
    KernelBuffer = (VOID *)KernelBase;
    Status = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &KernelSize, KernelBuffer);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to read kernel file: %r\n", Status);
        return Status;
    }

    uefi_call_wrapper(KernelFile->Close, 1, KernelFile);
    Print(L"Kernel loaded at address: 0x%lx\n", KernelBase);

    // Jump to Kernel Entry Point
    Print(L"Jumping to kernel entry point...\n");
    void (*KernelEntry)(void) = (void (*)(void))KernelBase;
    KernelEntry();

    return EFI_SUCCESS;
}
