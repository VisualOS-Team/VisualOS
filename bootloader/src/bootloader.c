#include <efi.h>
#include <efilib.h>
#include "../include/bootloader.h"

#define RESERVED_MEMORY (512 * 1024 * 1024)
#define STATIC_BUFFER_SIZE (64 * 1024 * 1024)

static kernel_params_t g_kernel_params;

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

static UINT8 g_memory_buffer[STATIC_BUFFER_SIZE] __attribute__((aligned(4096)));

void display_logo(EFI_SIMPLE_TEXT_OUT_PROTOCOL *ConOut) {
    UINTN Columns, Rows;
    ConOut->QueryMode(ConOut, ConOut->Mode->Mode, &Columns, &Rows);
    UINTN LogoHeight = sizeof(VISUALOS_LOGO) / sizeof(VISUALOS_LOGO[0]);
    UINTN StartRow = (Rows > LogoHeight) ? (Rows - LogoHeight) / 2 : 0;
    UINTN StartCol = (Columns > StrLen(VISUALOS_LOGO[0])) ? (Columns - StrLen(VISUALOS_LOGO[0])) / 2 : 0;
    
    for (UINTN i = 0; i < LogoHeight; i++) {
        ConOut->SetCursorPosition(ConOut, StartCol, StartRow + i);
        Print(L"%s\n", VISUALOS_LOGO[i]);
    }
}

void error_freeze(EFI_SIMPLE_TEXT_OUT_PROTOCOL *ConOut, EFI_STATUS Status) {
    ConOut->SetAttribute(ConOut, EFI_LIGHTRED | EFI_BACKGROUND_BLACK);
    Print(L"\nFATAL ERROR: %r\n", Status);
    Print(L"System halted.\n");
    while (1) __asm__("hlt");
}

EFI_STATUS initialize_graphics(EFI_BOOT_SERVICES *BS) {
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *GOP;
    
    Status = BS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&GOP);
    if (EFI_ERROR(Status)) return Status;
    
    g_kernel_params.framebuffer.framebuffer_base = GOP->Mode->FrameBufferBase;
    g_kernel_params.framebuffer.framebuffer_width = GOP->Mode->Info->HorizontalResolution;
    g_kernel_params.framebuffer.framebuffer_height = GOP->Mode->Info->VerticalResolution;
    g_kernel_params.framebuffer.framebuffer_pitch = GOP->Mode->Info->PixelsPerScanLine * 4;
    g_kernel_params.framebuffer.framebuffer_bpp = 32;
    
    return EFI_SUCCESS;
}

void detect_hardware_features(void) {
    g_kernel_params.acpi_enabled = 0;
    g_kernel_params.apic_enabled = 0;
}

EFI_STATUS configure_memory(
    EFI_MEMORY_DESCRIPTOR **MemoryMap,
    UINTN *MemoryMapSize,
    UINTN *MapKey,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion
) {
    EFI_STATUS Status;
    
    SetMem(g_memory_buffer, sizeof(g_memory_buffer), 0);
    
    *MemoryMapSize = 0;
    Status = gBS->GetMemoryMap(MemoryMapSize, NULL, MapKey, DescriptorSize, DescriptorVersion);
    if (Status != EFI_BUFFER_TOO_SMALL) return Status;
    
    *MemoryMapSize += 8192;
    
    if (*DescriptorSize > 0) {
        *MemoryMapSize = (*MemoryMapSize + *DescriptorSize - 1) & ~(*DescriptorSize - 1);
    }
    
    if (*MemoryMapSize > sizeof(g_memory_buffer)) {
        return EFI_BUFFER_TOO_SMALL;
    }
    
    *MemoryMap = (EFI_MEMORY_DESCRIPTOR *)g_memory_buffer;
    
    Status = gBS->GetMemoryMap(MemoryMapSize, *MemoryMap, MapKey, DescriptorSize, DescriptorVersion);
    if (EFI_ERROR(Status)) {
        *MemoryMap = NULL;
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
    UINTN RetryCount = 0;
    
    do {
        Status = configure_memory(MemoryMap, MemoryMapSize, MapKey, DescriptorSize, DescriptorVersion);
        if (EFI_ERROR(Status)) return Status;
        
        g_kernel_params.memory_info.map_size = *MemoryMapSize;
        g_kernel_params.memory_info.map_key = *MapKey;
        g_kernel_params.memory_info.descriptor_size = *DescriptorSize;
        g_kernel_params.memory_info.descriptor_version = *DescriptorVersion;
        g_kernel_params.memory_info.memory_map = *MemoryMap;
        
        Status = gBS->ExitBootServices(ImageHandle, *MapKey);
        if (!EFI_ERROR(Status)) return EFI_SUCCESS;
        
        RetryCount++;
    } while (RetryCount < 5);
    
    return Status;
}

EFI_STATUS load_kernel(EFI_FILE_PROTOCOL *Root, EFI_PHYSICAL_ADDRESS *KernelAddress) {
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *KernelFile;
    EFI_FILE_INFO *FileInfo;
    UINTN InfoSize = sizeof(EFI_FILE_INFO) + 1024;
    UINTN FileSize;
    
    Status = Root->Open(Root, &KernelFile, L"\\EFI\\BOOT\\kernel.bin", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) return Status;
    
    Status = gBS->AllocatePool(EfiLoaderData, InfoSize, (VOID **)&FileInfo);
    if (EFI_ERROR(Status)) {
        KernelFile->Close(KernelFile);
        return Status;
    }
    
    Status = KernelFile->GetInfo(KernelFile, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        gBS->FreePool(FileInfo);
        KernelFile->Close(KernelFile);
        return Status;
    }
    
    FileSize = FileInfo->FileSize;
    gBS->FreePool(FileInfo);
    
    EFI_PHYSICAL_ADDRESS desiredAddress = 0x00100000;
    
    Status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, 
                              EFI_SIZE_TO_PAGES(FileSize + 0x10000), &desiredAddress);
    if (EFI_ERROR(Status)) {
        KernelFile->Close(KernelFile);
        return Status;
    }
    
    *KernelAddress = desiredAddress;
    Status = KernelFile->Read(KernelFile, &FileSize, (VOID *)(*KernelAddress));
    KernelFile->Close(KernelFile);
    
    return Status;
}

EFI_STATUS
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS                      Status;
    EFI_BOOT_SERVICES              *BS = SystemTable->BootServices;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FS;
    EFI_FILE_PROTOCOL               *Root;
    EFI_MEMORY_DESCRIPTOR           *MemoryMap = NULL;
    UINTN                            MemoryMapSize    = 0;
    UINTN                            MapKey           = 0;
    UINTN                            DescriptorSize   = 0;
    UINT32                           DescriptorVersion= 0;
    EFI_PHYSICAL_ADDRESS             KernelImageAddr = 0;

    // 1) UEFI setup + splash
    InitializeLib(ImageHandle, SystemTable);
    display_logo(SystemTable->ConOut);

    // 2) Grab a stable memory map
    Status = configure_memory(
        &MemoryMap, &MemoryMapSize,
        &MapKey, &DescriptorSize,
        &DescriptorVersion
    );
    if (EFI_ERROR(Status))
        error_freeze(SystemTable->ConOut, Status);

    // 3) Populate g_kernel_params.memory_info
    g_kernel_params.memory_info.memory_map        = MemoryMap;
    g_kernel_params.memory_info.map_size          = MemoryMapSize;
    g_kernel_params.memory_info.map_key           = MapKey;
    g_kernel_params.memory_info.descriptor_size   = DescriptorSize;
    g_kernel_params.memory_info.descriptor_version= DescriptorVersion;

    // 4) Graphics & features
    Status = initialize_graphics(BS);
    if (EFI_ERROR(Status))
        error_freeze(SystemTable->ConOut, Status);
    detect_hardware_features();

    // 5) Load kernel.bin at 1 MiB
    Status = BS->HandleProtocol(
        ImageHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)&FS
    );
    if (EFI_ERROR(Status))
        error_freeze(SystemTable->ConOut, Status);

    Status = FS->OpenVolume(FS, &Root);
    if (EFI_ERROR(Status))
        error_freeze(SystemTable->ConOut, Status);

    Status = load_kernel(Root, &KernelImageAddr);
    if (EFI_ERROR(Status))
        error_freeze(SystemTable->ConOut, Status);

    // 6) Exit boot services (retry if memory map changed)
    Status = reacquire_memory_map_and_exit_boot_services(
        ImageHandle,
        &MemoryMap,
        &MemoryMapSize,
        &MapKey,
        &DescriptorSize,
        &DescriptorVersion
    );
    if (EFI_ERROR(Status))
        error_freeze(SystemTable->ConOut, Status);

    // 7) Refresh params in case map grew
    g_kernel_params.memory_info.memory_map        = MemoryMap;
    g_kernel_params.memory_info.map_size          = MemoryMapSize;
    g_kernel_params.memory_info.map_key           = MapKey;
    g_kernel_params.memory_info.descriptor_size   = DescriptorSize;
    g_kernel_params.memory_info.descriptor_version= DescriptorVersion;

    // 8) Jump into your kernel!
    kernel_main_t entry = (kernel_main_t)(UINTN)KernelImageAddr;
    entry(&g_kernel_params);

    // Should never return; if it does, halt forever
    for (;;)
        __asm__ volatile ("hlt");

    return EFI_SUCCESS; // never reached
}
