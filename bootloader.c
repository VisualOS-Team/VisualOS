#include <efi.h>
#include <efilib.h>

// Constants
#define KERNEL_BASE_ADDRESS 0x100000 // 1MB for the kernel
#define RESERVED_MEMORY (512 * 1024 * 1024) // 128MB reserved

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

// ASCII Art for VisualOS Logo
const CHAR16 *VISUALOS_LOGO[] = {
    L"                                                                ",
    L"                                                                ",
    L"                                                                ",
    L"                                                                ",
    L"                                                                ",
    L"                                                                ",
    L"                                                                ",
    L"                                                                ",
    L"                                                                ",
    L"                                                                ",
    L"            ********** *                         ***            ",
    L"         ******************                   ***********       ",
    L"       ******  ***************                         ****     ",
    L"       **     ** ************ ****   ***                 ***    ",
    L"      **      * * ****  ***************                  ****   ",
    L"      **          * ****  ***********                    * **   ",
    L"      **              ** * ******* *  *                    **   ",
    L"      **                 **   **** **** *                 ***   ",
    L"       **                 ***** ****** * *                **    ",
    L"        ***              *****   ***********           *****    ",
    L"          ****         ******     ************ ************     ",
    L"              *********   *         *********************       ",
    L"                                      * *************           "
};

// Function to display the logo in the center of the screen
void display_logo(EFI_SIMPLE_TEXT_OUT_PROTOCOL *ConOut) {
    UINTN Columns, Rows;
    ConOut->QueryMode(ConOut, ConOut->Mode->Mode, &Columns, &Rows);

    // Calculate starting row and column for centering the logo
    UINTN LogoHeight = sizeof(VISUALOS_LOGO) / sizeof(VISUALOS_LOGO[0]);
    UINTN StartRow = (Rows - LogoHeight) / 2;
    UINTN StartCol = (Columns - StrLen(VISUALOS_LOGO[0])) / 2;

    // Print each line of the logo
    for (UINTN i = 0; i < LogoHeight; i++) {
        ConOut->SetCursorPosition(ConOut, StartCol, StartRow + i);
        Print(L"%s\n", VISUALOS_LOGO[i]);
    }
}

void error_freeze(EFI_SIMPLE_TEXT_OUT_PROTOCOL *ConOut) {
    Print(L"Error occurred. Freezing system...\n");

    while (1) {
        __asm__("hlt");
    }
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

EFI_STATUS reacquire_memory_map_and_exit_boot_services(
    EFI_HANDLE ImageHandle,
    UINTN *MemoryMapSize,
    EFI_MEMORY_DESCRIPTOR **MemoryMap,
    UINTN *MapKey,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion
) {
    EFI_STATUS Status;

    // Reacquire memory map before exiting boot services
    Print(L"Reacquiring memory map before exiting boot services...\n");

    // Allocate initial memory map buffer
    if (*MemoryMapSize == 0) {
        *MemoryMapSize = 4096; // Start with a reasonable buffer size (4KB)
    }

    // Loop until the memory map is successfully retrieved
    do {
        // Free the current buffer if it was previously allocated
        if (*MemoryMap != NULL) {
            gBS->FreePool(*MemoryMap);
        }

        // Allocate memory for the memory map
        Status = uefi_call_wrapper(gBS->AllocatePool, 3, EfiLoaderData, *MemoryMapSize, (VOID **)MemoryMap);
        if (EFI_ERROR(Status)) {
            Print(L"Failed to allocate memory for memory map: %r\n", Status);
            return Status;
        }

        // Retrieve the memory map
        Status = uefi_call_wrapper(gBS->GetMemoryMap, 5, MemoryMapSize, *MemoryMap, MapKey, DescriptorSize, DescriptorVersion);

        // If the buffer is too small, increase the size
        if (Status == EFI_BUFFER_TOO_SMALL) {
            *MemoryMapSize += 4096; // Increment the buffer size
        }

    } while (Status == EFI_BUFFER_TOO_SMALL);

    if (EFI_ERROR(Status)) {
        Print(L"Failed to retrieve memory map: %r\n", Status);
        gBS->FreePool(*MemoryMap);
        return Status;
    }

    // Print debug information about the memory map
    Print(L"MemoryMapSize: %lu, MapKey: 0x%lx, DescriptorSize: %lu\n", *MemoryMapSize, *MapKey, *DescriptorSize);

    // Exit boot services
    Print(L"Exiting boot services...\n");
    Status = uefi_call_wrapper(gBS->ExitBootServices, 2, ImageHandle, *MapKey);

    // If ExitBootServices fails, print debug information
    if (EFI_ERROR(Status)) {
        Print(L"Failed to exit boot services: %r\n", Status);
        Print(L"MapKey: 0x%lx, MemoryMapSize: %lu, DescriptorSize: %lu\n", *MapKey, *MemoryMapSize, *DescriptorSize);

        // Free the memory map buffer if allocated
        gBS->FreePool(*MemoryMap);
        return Status;
    }

    // Free the memory map buffer, as it is no longer needed
    gBS->FreePool(*MemoryMap);

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
        KernelFile->Close(KernelFile); // Ensure the file is closed
        return Status;
    }

    Status = uefi_call_wrapper(KernelFile->GetInfo, 4, KernelFile, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to retrieve kernel file info: %r\n", Status);
        gBS->FreePool(FileInfo);
        KernelFile->Close(KernelFile); // Ensure the file is closed
        return Status;
    }

    FileSize = FileInfo->FileSize;
    gBS->FreePool(FileInfo);

    // Allocate memory for the kernel
    Status = uefi_call_wrapper(gBS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, EFI_SIZE_TO_PAGES(FileSize), KernelAddress);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate memory for kernel: %r\n", Status);
        KernelFile->Close(KernelFile); // Ensure the file is closed
        return Status;
    }

    // Read the kernel into memory
    Buffer = (VOID *)*KernelAddress;
    Status = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &FileSize, Buffer);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to read kernel into memory: %r\n", Status);
        KernelFile->Close(KernelFile); // Ensure the file is closed
        return Status;
    }

    Print(L"Kernel loaded at address: 0x%lx\n", *KernelAddress);

    // Close the kernel file
    Status = uefi_call_wrapper(KernelFile->Close, 1, KernelFile);
    if (EFI_ERROR(Status)) {
        Print(L"Warning: Failed to close kernel file: %r\n", Status);
        // Do not return failure here, as the kernel was successfully loaded
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
    UINTN MemoryMapSize = 0, MapKey, DescriptorSize;
    UINT32 DescriptorVersion;
    EFI_PHYSICAL_ADDRESS KernelAddress = KERNEL_BASE_ADDRESS;

    InitializeLib(ImageHandle, SystemTable);
    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);

    display_logo(SystemTable->ConOut);

    Print(L"UEFI Bootloader Starting...\n");

    // Initialize CPU
    Print(L"Initializing CPU...\n");
    initialize_cpu();

    // Configure memory management
    Print(L"Configuring memory management...\n");
    Status = configure_memory(&MemoryMap, &MemoryMapSize, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to configure memory: %r\n", Status);
        error_freeze(SystemTable->ConOut);
        return Status;
    }

    // Open the root filesystem
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    Status = uefi_call_wrapper(gBS->HandleProtocol, 3, ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to retrieve loaded image protocol: %r\n", Status);
        error_freeze(SystemTable->ConOut);
        return Status;
    }

    Status = uefi_call_wrapper(gBS->HandleProtocol, 3, LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&FileSystem);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to retrieve filesystem protocol: %r\n", Status);
        error_freeze(SystemTable->ConOut);
        return Status;
    }

    EFI_FILE_PROTOCOL *Root;
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to open filesystem volume: %r\n", Status);
        error_freeze(SystemTable->ConOut);
        return Status;
    }

    // Load the kernel
    Print(L"Loading kernel...\n");
    Status = load_kernel(Root, &KernelAddress);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to load kernel: %r\n", Status);
        error_freeze(SystemTable->ConOut);
        return Status;
    }

    // Reacquire memory map and exit boot services
    Status = reacquire_memory_map_and_exit_boot_services(
        ImageHandle,
        &MemoryMapSize,
        &MemoryMap,
        &MapKey,
        &DescriptorSize,
        &DescriptorVersion
    );
    if (EFI_ERROR(Status)) {
        if (MemoryMap != NULL) {
            gBS->FreePool(MemoryMap);
        }
        Print(L"Exiting boot services failed: %r\n", Status);
        error_freeze(SystemTable->ConOut);
        return Status; // Return the error instead of freezing
    }

    // Free the memory map if allocated
    if (MemoryMap != NULL) {
        gBS->FreePool(MemoryMap);
    }



    // Transfer control to the kernel
    Print(L"Transferring control to kernel at address: 0x%lx\n", KernelAddress);
    KernelMain kernel_main = (KernelMain)KernelAddress;
    kernel_main();

    // Should not return here
    Print(L"ERROR: Kernel returned control to bootloader.\n");
    while (1) {
        __asm__("hlt");
    }

    return EFI_SUCCESS;
}
