#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include <efi.h>
#include <efilib.h>

typedef struct {
    void *memory_map;
    UINTN map_size;
    UINTN map_key;
    UINTN descriptor_size;
    UINT32 descriptor_version;
} memory_info_t;

typedef struct {
    UINT64 framebuffer_base;
    UINT32 framebuffer_width;
    UINT32 framebuffer_height;
    UINT32 framebuffer_pitch;
    UINT32 framebuffer_bpp;
} framebuffer_info_t;

typedef struct {
    memory_info_t memory_info;
    framebuffer_info_t framebuffer;
    UINT8 acpi_enabled;
    UINT8 apic_enabled;
} kernel_params_t;

typedef void (*kernel_main_t)(kernel_params_t*);

EFI_STATUS initialize_graphics(EFI_BOOT_SERVICES *BS);
void detect_hardware_features(void);
EFI_STATUS configure_memory(
    EFI_MEMORY_DESCRIPTOR **MemoryMap,
    UINTN *MemoryMapSize,
    UINTN *MapKey,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion
);
EFI_STATUS reacquire_memory_map_and_exit_boot_services(
    EFI_HANDLE ImageHandle,
    EFI_MEMORY_DESCRIPTOR **MemoryMap,
    UINTN *MemoryMapSize,
    UINTN *MapKey,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion
);
EFI_STATUS load_kernel(EFI_FILE_PROTOCOL *Root, EFI_PHYSICAL_ADDRESS *KernelAddress);
void display_logo(EFI_SIMPLE_TEXT_OUT_PROTOCOL *ConOut);
void error_freeze(EFI_SIMPLE_TEXT_OUT_PROTOCOL *ConOut, EFI_STATUS Status);

#endif