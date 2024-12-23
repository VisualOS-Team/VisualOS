#include <efi.h>
#include <efilib.h>

// Constants
#define KERNEL_BASE_ADDRESS 0x100000 // 1MB for the kernel
#define RESERVED_MEMORY (128 * 1024 * 1024) // 128MB reserved

typedef void (*KernelMain)();

// Function to initialize CPU
void initialize_cpu() {
    __asm__ __volatile__(
        "mov %%cr0, %%rax\n\t"
        "or $0x1, %%rax\n\t"   // Enable protected mode
        "mov %%rax, %%cr0\n\t" // Write back to CR0
        :
        :
        : "rax"
    );
}

// Function to configure memory management
EFI_STATUS configure_memory(EFI_MEMORY_DESCRIPTOR **MemoryMap, UINTN *MemoryMapSize, UINTN *MapKey, UINTN *DescriptorSize, UINT32 *DescriptorVersion) {
    EFI_STATUS Status;

    // Get memory map size
    Status = uefi_call_wrapper(gBS->GetMemoryMap, 5, MemoryMapSize, *MemoryMap, MapKey, DescriptorSize, DescriptorVersion);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        Print(L"Failed to get memory map size: %r\n", Status);
        return Status;
    }

    // Allocate pool for memory map
    Status = uefi_call_wrapper(gBS->AllocatePool, 3, EfiLoaderData, *MemoryMapSize, (VOID **)MemoryMap);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate pool for memory map: %r\n", Status);
        return Status;
    }

    // Retrieve memory map
    Status = uefi_call_wrapper(gBS->GetMemoryMap, 5, MemoryMapSize, *MemoryMap, MapKey, DescriptorSize, DescriptorVersion);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to retrieve memory map: %r\n", Status);
        return Status;
    }

    return EFI_SUCCESS;
}

// Function to load and prepare the kernel
EFI_STATUS load_kernel(EFI_FILE_PROTOCOL *Root, EFI_PHYSICAL_ADDRESS *KernelAddress) {
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *KernelFile;
    VOID *Buffer;
    UINTN FileSize = 0;

    // Open the kernel file
    Status = uefi_call_wrapper(Root->Open, 5, Root, &KernelFile, L"\\EFI\\BOOT\\kernel.bin", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open kernel file: %r\n", Status);
        return Status;
    }

    // Get kernel file size
    EFI_FILE_INFO *FileInfo;
    UINTN InfoSize = sizeof(EFI_FILE_INFO) + 1024;
    Status = uefi_call_wrapper(gBS->AllocatePool, 3, EfiLoaderData, InfoSize, (VOID **)&FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory for file info: %r\n", Status);
        KernelFile->Close(KernelFile);
        return Status;
    }

    Status = uefi_call_wrapper(KernelFile->GetInfo, 4, KernelFile, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to retrieve kernel file info: %r\n", Status);
        gBS->FreePool(FileInfo);
        KernelFile->Close(KernelFile);
        return Status;
    }

    FileSize = FileInfo->FileSize;
    gBS->FreePool(FileInfo);

    // Allocate memory for the kernel
    Status = uefi_call_wrapper(gBS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, EFI_SIZE_TO_PAGES(FileSize), KernelAddress);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory for kernel: %r\n", Status);
        KernelFile->Close(KernelFile);
        return Status;
    }

    // Read the kernel into memory
    Buffer = (VOID *)*KernelAddress;
    Status = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &FileSize, Buffer);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to read kernel into memory: %r\n", Status);
        KernelFile->Close(KernelFile);
        return Status;
    }

    Print(L"Kernel loaded at address: 0x%lx\n", *KernelAddress);

    // Close the kernel file
    KernelFile->Close(KernelFile);
    return EFI_SUCCESS;
}

// Main entry point for the UEFI bootloader
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
    UINTN MemoryMapSize = 0, MapKey, DescriptorSize;
    UINT32 DescriptorVersion;
    EFI_PHYSICAL_ADDRESS KernelAddress = KERNEL_BASE_ADDRESS;

    InitializeLib(ImageHandle, SystemTable);
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    Print(L"UEFI Bootloader Starting...\n");

    // Initialize CPU
    Print(L"Initializing CPU...\n");
    initialize_cpu();

    // Configure memory management
    Print(L"Configuring memory management...\n");
    Status = configure_memory(&MemoryMap, &MemoryMapSize, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to configure memory: %r\n", Status);
        return Status;
    }

    // Open the root filesystem
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    Status = uefi_call_wrapper(gBS->HandleProtocol, 3, ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to retrieve loaded image protocol: %r\n", Status);
        return Status;
    }

    Status = uefi_call_wrapper(gBS->HandleProtocol, 3, LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&FileSystem);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to retrieve filesystem protocol: %r\n", Status);
        return Status;
    }

    EFI_FILE_PROTOCOL *Root;
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open filesystem volume: %r\n", Status);
        return Status;
    }

    // Load the kernel
    Print(L"Loading kernel...\n");
    Status = load_kernel(Root, &KernelAddress);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to load kernel: %r\n", Status);
        return Status;
    }

    // Exit boot services
    Print(L"Exiting boot services...\n");
    Status = uefi_call_wrapper(gBS->ExitBootServices, 2, ImageHandle, MapKey);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to exit boot services: %r\n", Status);
        return Status;
    }

    // Jump to the kernel's main function
    Print(L"Transferring control to kernel at address: 0x%lx\n", KernelAddress);
    KernelMain kernel_main = (KernelMain)KernelAddress;
    kernel_main();

    // This should not execute
    Print(L"ERROR: Kernel returned control to bootloader.\n");
    while (1) {
        __asm__("hlt");
    }

    return EFI_SUCCESS;
}
