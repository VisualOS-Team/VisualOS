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
    EFI_MEMORY_DESCRIPTOR **MemoryMap,
    UINTN *MemoryMapSize,
    UINTN *MapKey,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion
) {
    EFI_STATUS Status;
    #define STATIC_BUFFER_SIZE (8 * 1024 * 1024) // Increased buffer size for testing
    static CHAR8 StaticBuffer[STATIC_BUFFER_SIZE] __attribute__((aligned(8)));
    UINTN RetryCount = 0;

    *MemoryMap = (EFI_MEMORY_DESCRIPTOR *)StaticBuffer;
    *MemoryMapSize = STATIC_BUFFER_SIZE;

    Print(L"Debug: Entering reacquire_memory_map_and_exit_boot_services\n");
    Print(L"StaticBuffer Address: 0x%lx\n", (UINTN)StaticBuffer);

    do {
        // Reset MemoryMap
        *MemoryMap = (EFI_MEMORY_DESCRIPTOR *)StaticBuffer;

        Print(L"MemoryMap Address: 0x%lx, MemoryMapSize: %lu, DescriptorSize: %lu\n",
              (UINTN)*MemoryMap, *MemoryMapSize, *DescriptorSize);

        Status = gBS->GetMemoryMap(MemoryMapSize, *MemoryMap, MapKey, DescriptorSize, DescriptorVersion);

        if (Status == EFI_BUFFER_TOO_SMALL) {
            Print(L"Buffer Too Small. Adjusting size: %lu bytes\n", *MemoryMapSize);

            if (*MemoryMapSize > STATIC_BUFFER_SIZE) {
                Print(L"Error: MemoryMapSize exceeds static buffer size.\n");
                while (1) { __asm__("hlt"); }
            }

            if (*DescriptorSize != sizeof(EFI_MEMORY_DESCRIPTOR)) {
                Print(L"Warning: Forcing DescriptorSize to %lu.\n", sizeof(EFI_MEMORY_DESCRIPTOR));
                *DescriptorSize = sizeof(EFI_MEMORY_DESCRIPTOR);
            }
        } else if (EFI_ERROR(Status)) {
            Print(L"Error: GetMemoryMap failed with status: %r\n", Status);
            break;
        } else {
            Print(L"GetMemoryMap succeeded.\n");
            break;
        }

        RetryCount++;
    } while (RetryCount < 3);

    // Validate MemoryMap
    if (*MemoryMapSize < *DescriptorSize) {
        Print(L"Error: MemoryMapSize (%lu) is smaller than DescriptorSize (%lu).\n",
              *MemoryMapSize, *DescriptorSize);
        while (1) { __asm__("hlt"); }
    }

    // Parse Descriptors
    EFI_MEMORY_DESCRIPTOR *Descriptor = *MemoryMap;
    for (UINTN i = 0; i < (*MemoryMapSize / *DescriptorSize); i++) {
        Print(L"Descriptor %u: Type: %u, PhysicalStart: 0x%lx, NumberOfPages: %lu\n",
              i, Descriptor->Type, Descriptor->PhysicalStart, Descriptor->NumberOfPages);

        // Ensure descriptors are aligned
        if ((Descriptor->PhysicalStart % EFI_PAGE_SIZE) != 0) {
            Print(L"Error: Descriptor %u PhysicalStart is not page-aligned.\n", i);
            while (1) { __asm__("hlt"); }
        }

        Descriptor = (EFI_MEMORY_DESCRIPTOR *)((CHAR8 *)Descriptor + *DescriptorSize);
    }

    // Exit Boot Services
    Print(L"Exiting Boot Services...\n");
    Status = gBS->ExitBootServices(ImageHandle, *MapKey);

    if (EFI_ERROR(Status)) {
        Print(L"Error: ExitBootServices failed. MapKey: %lu, Status: %r\n", *MapKey, Status);
        while (1) { __asm__("hlt"); }
    }

    Print(L"Exited Boot Services successfully.\n");
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
