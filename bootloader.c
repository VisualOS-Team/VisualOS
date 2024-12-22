#include <efi.h>
#include <efilib.h>

#define KERNEL_BASE_ADDRESS 0x100000 // 1MB for the kernel
#define RESERVED_MEMORY (128 * 1024 * 1024) // 128MB reserved

typedef void (*KernelMain)();

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS KernelAddress = KERNEL_BASE_ADDRESS;
    EFI_PHYSICAL_ADDRESS RamAddress;
    UINTN MemoryMapSize = 0, MapKey, DescriptorSize;
    UINT32 DescriptorVersion;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
    UINT64 TotalAvailableMemory = 0, AllocatableMemory = 0;
    UINT64 ChunkSize = 128 * 1024 * 1024; // 128MB per chunk
    UINT64 AllocatedMemory = 0;
    EFI_FILE_PROTOCOL *KernelFile;
    UINTN FileSize = 0;
    VOID *Buffer;

    InitializeLib(ImageHandle, SystemTable);
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    Print(L"UEFI Bootloader Starting...\n");

    // Get the memory map size
    Status = uefi_call_wrapper(gBS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        Print(L"Failed to get memory map size: %r\n", Status);
        return Status;
    }

    // Allocate memory for the memory map
    Status = uefi_call_wrapper(gBS->AllocatePool, 3, EfiLoaderData, MemoryMapSize, (VOID **)&MemoryMap);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory for memory map: %r\n", Status);
        return Status;
    }

    // Retrieve the memory map
    Status = uefi_call_wrapper(gBS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to retrieve memory map: %r\n", Status);
        gBS->FreePool(MemoryMap);
        return Status;
    }

    // Calculate total available memory
    EFI_MEMORY_DESCRIPTOR *Desc = MemoryMap;
    for (UINTN i = 0; i < MemoryMapSize / DescriptorSize; i++) {
        if (Desc->Type == EfiConventionalMemory) {
            UINT64 BlockSize = (UINT64)Desc->NumberOfPages * 4096; // Ensure no overflow
            TotalAvailableMemory += BlockSize;
        }
        Desc = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Desc + DescriptorSize);
    }

    gBS->FreePool(MemoryMap);

    // Calculate allocatable memory
    if (TotalAvailableMemory > RESERVED_MEMORY) {
        AllocatableMemory = TotalAvailableMemory - RESERVED_MEMORY;
    } else {
        Print(L"Not enough memory to reserve 128MB.\n");
        return EFI_OUT_OF_RESOURCES;
    }

    Print(L"Total available memory: %llu bytes\n", TotalAvailableMemory);
    Print(L"Allocatable memory: %llu bytes\n", AllocatableMemory);

    // Allocate memory in chunks
    Print(L"Allocating memory in chunks...\n");
    while (AllocatedMemory < AllocatableMemory) {
        Status = uefi_call_wrapper(gBS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, EFI_SIZE_TO_PAGES(ChunkSize), &RamAddress);
        if (EFI_ERROR(Status)) {
            Print(L"Failed to allocate chunk after %llu bytes: %r\n", AllocatedMemory, Status);
            break;
        }
        Print(L"Allocated chunk at address: 0x%lx, Size: %llu bytes\n", RamAddress, ChunkSize);
        AllocatedMemory += ChunkSize;
    }

    Print(L"Total allocated memory: %llu bytes\n", AllocatedMemory);

    if (AllocatedMemory < AllocatableMemory) {
        Print(L"Could not allocate all requested memory.\n");
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

    // Open the kernel file
    Status = uefi_call_wrapper(Root->Open, 5, Root, &KernelFile, L"\\EFI\\BOOT\\kernel.bin", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open kernel file: %r\n", Status);
        return Status;
    }

    // Get the file size
    EFI_FILE_INFO *FileInfo;
    UINTN InfoSize = sizeof(EFI_FILE_INFO) + 1024;
    Status = uefi_call_wrapper(gBS->AllocatePool, 3, EfiLoaderData, InfoSize, (VOID **)&FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory for file info: %r\n", Status);
        return Status;
    }

    Status = uefi_call_wrapper(KernelFile->GetInfo, 4, KernelFile, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to retrieve file info: %r\n", Status);
        gBS->FreePool(FileInfo);
        return Status;
    }

    FileSize = FileInfo->FileSize;
    gBS->FreePool(FileInfo);

    // Allocate memory for the kernel
    Status = uefi_call_wrapper(gBS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, EFI_SIZE_TO_PAGES(FileSize), &KernelAddress);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory for kernel: %r\n", Status);
        return Status;
    }

    // Read the kernel into memory
    Buffer = (VOID *)KernelAddress;
    Status = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &FileSize, Buffer);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to read kernel into memory: %r\n", Status);
        return Status;
    }

    Print(L"Kernel loaded at address: 0x%lx\n", KernelAddress);

    // Jump to the kernel's main function
    Print(L"Jumping to kernel main...\n");
    KernelMain main = (KernelMain)KernelAddress;
    main();

    // Should never return
    return EFI_SUCCESS;
}
