#include <bootinfo.h>
#include <uefi.h>

static uint32_t convert_mmap_type(uint32_t uefi_type) {
    switch (uefi_type) {
        case EfiConventionalMemory:
            return MMAP_TYPE_USABLE;
            
        case EfiLoaderCode:
        case EfiLoaderData:
        case EfiBootServicesCode:
        case EfiBootServicesData:
            return MMAP_TYPE_BOOTLOADER_RECLAIMABLE;
            
        case EfiACPIReclaimMemory:
            return MMAP_TYPE_ACPI_RECLAIMABLE;
            
        case EfiACPIMemoryNVS:
            return MMAP_TYPE_ACPI_NVS;
            
        case EfiUnusableMemory:
            return MMAP_TYPE_BAD_MEM;
            
        case EfiReservedMemoryType:
        case EfiRuntimeServicesCode:
        case EfiRuntimeServicesData:
        case EfiMemoryMappedIO:
        case EfiMemoryMappedIOPortSpace:
        case EfiPalCode:
        default:
            return MMAP_TYPE_RESERVED;
    }
}

efi_status_t get_me(uint64_t *out_base, size_t *out_size) {
    efi_guid_t loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    efi_loaded_image_protocol_t *loaded_image = NULL;
    efi_status_t status;

    status = BS->HandleProtocol(IM, &loaded_image_guid, (void**)&loaded_image);
    if (EFI_ERROR(status)) {
        return status;
    }

    *out_base = (uint64_t)loaded_image->ImageBase;
    *out_size = (size_t)loaded_image->ImageSize;

    return EFI_SUCCESS;
}

boot_info_t g_boot_info = {0};

efi_status_t init_gop(framebuffer_t *fb) {
    efi_gop_t *gop = NULL;
    efi_guid_t gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    efi_status_t status = BS->LocateProtocol(&gop_guid, NULL, (void**)&gop);
    if (EFI_ERROR(status) || !gop) {
        return status;
    }

    fb->address = (uint32_t*)gop->Mode->FrameBufferBase;
    fb->width   = gop->Mode->Information->HorizontalResolution;
    fb->height  = gop->Mode->Information->VerticalResolution;
    fb->pitch   = gop->Mode->Information->PixelsPerScanLine * sizeof(uint32_t);

    return EFI_SUCCESS;
}

efi_status_t init_mmap(mmap_info_t *mmap_info, uintn_t *out_map_key) {
    uintn_t map_size = 0, map_key = 0, desc_size = 0;
    uint32_t desc_ver = 0;
    efi_status_t status;

    status = BS->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_ver);
    if (status != EFI_BUFFER_TOO_SMALL) return status;

    map_size += 16 * desc_size;
    uintn_t total_entries = map_size / desc_size;

    efi_memory_descriptor_t *map = NULL;
    status = BS->AllocatePool(EfiLoaderData, map_size, (void**)&map);
    if (EFI_ERROR(status)) return status;

    mmap_entry_t *entries = NULL;
    status = BS->AllocatePool(EfiLoaderData, sizeof(mmap_entry_t) * total_entries, (void**)&entries);
    if (EFI_ERROR(status)) {
        BS->FreePool(map);
        return status;
    }

    status = ST->BootServices->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_ver);
    if (status != EFI_SUCCESS) {
        BS->FreePool(map);
        return status;
    }

    total_entries = map_size / desc_size;

    uint64_t max_phys = 0;
    uint64_t total_usable = 0;

    for (uintn_t i = 0; i < total_entries; i++) {
        efi_memory_descriptor_t *desc = (efi_memory_descriptor_t *)((uint8_t *)map + (i * desc_size));

        uint64_t size_bytes = desc->NumberOfPages * 4096;

        entries[i].phys_start = desc->PhysicalStart;
        entries[i].length     = size_bytes;
        entries[i].type       = convert_mmap_type(desc->Type);
        entries[i].flags      = desc->Attribute;

        uint64_t end_addr = desc->PhysicalStart + size_bytes;

        if (end_addr > max_phys) {
            max_phys = end_addr;
        }

        if (desc->Type == EfiConventionalMemory) {
            total_usable += size_bytes;
        }
    }

    mmap_info->entries       = entries;
    mmap_info->count         = total_entries;
    mmap_info->max_phys_addr = max_phys;
    mmap_info->total_usable  = total_usable;

    *out_map_key = map_key;

    return EFI_SUCCESS;
}

efi_status_t init_acpi(boot_info_t *boot_info) {
    efi_guid_t acpi_guid = ACPI_20_TABLE_GUID;
    void *rsdp = NULL;

    for (uintn_t i = 0; i < ST->NumberOfTableEntries; i++) {
        efi_guid_t *table_guid = &ST->ConfigurationTable[i].VendorGuid;
        
        if (table_guid->Data1 == acpi_guid.Data1 &&
            table_guid->Data2 == acpi_guid.Data2 &&
            table_guid->Data3 == acpi_guid.Data3 &&
            *(uint64_t*)table_guid->Data4 == *(uint64_t*)acpi_guid.Data4) {
            rsdp = ST->ConfigurationTable[i].VendorTable;
            break;
        }
    }

    if (!rsdp) {
        return EFI_NOT_FOUND;
    }

    boot_info->rsdp_addr = (uint64_t)rsdp;
    return EFI_SUCCESS;
}

int main(int argc, char **argv) {
    uintn_t map_key = 0;
    efi_status_t status;

    printf("kinit148\n\n");

    uint64_t image_base = 0;
    size_t   image_size = 0;
    if (EFI_ERROR(get_me(&image_base, &image_size))) {
        printf("loaded image failed!\n");
        return 1;
    }

    // bootinfo
    if (EFI_ERROR(init_gop(&g_boot_info.framebuffer))) {
        printf("GOP failed!\n");
        return 1;
    }
    if (EFI_ERROR(init_acpi(&g_boot_info))) {
        printf("acpi failed!\n");
        return 1;
    }
    if (EFI_ERROR(init_mmap(&g_boot_info.memory, &map_key))) {
        printf("mmap failed!\n");
        return 1;
    }


    extern efi_status_t load_bootbin(boot_info_t *boot_info);
    if (EFI_ERROR(load_bootbin(&g_boot_info))) {
        printf("bootbin failed!\n");
        return 1;
    }

    uint64_t out_entry = 0;
    extern efi_status_t load_elf_kernel_uefi(uint64_t kernel_src, uint64_t *out_entry_point);
    if (EFI_ERROR(load_elf_kernel_uefi(g_boot_info.kernel.kernel_src, &out_entry))) {
        printf("load kernel failed!\n");
        return 1;
    }

    uint64_t out_root_phys;
    extern efi_status_t init_hhdm(uint64_t max_phys_addr, uint64_t image_base, size_t image_size, uint64_t *out_root_phys);
    
    if (EFI_ERROR(init_hhdm(g_boot_info.memory.max_phys_addr, image_base, image_size, &out_root_phys))) {
        printf("HHDM failed!\n");
        return 1;
    }

    if (EFI_ERROR(init_mmap(&g_boot_info.memory, &map_key))) {
        printf("mmap failed!\n");
        return 1;
    }
    status = BS->ExitBootServices(IM, map_key);

    if (EFI_ERROR(status)) {
        init_mmap(&g_boot_info.memory, &map_key);

        status = BS->ExitBootServices(IM, map_key);
        if (EFI_ERROR(status)) {
            return 1; 
        }
    }

    g_boot_info.memory.entries      = (mmap_entry_t*)(0xFFFF800000000000ULL + (uint64_t)g_boot_info.memory.entries);
    g_boot_info.kernel.kernel_src   = 0xFFFF800000000000ULL + g_boot_info.kernel.kernel_src;
    g_boot_info.init.addr           = 0xFFFF800000000000ULL + g_boot_info.init.addr;
    g_boot_info.initrd.addr         = 0xFFFF800000000000ULL + g_boot_info.initrd.addr;
    g_boot_info.rsdp_addr           = 0xFFFF800000000000ULL + g_boot_info.rsdp_addr;
    g_boot_info.framebuffer.address = (uint32_t*)((uint64_t)g_boot_info.framebuffer.address + 0xFFFF800000000000ULL);

    extern void jump_to_kernel(uint64_t kernel_entry_virt, uint64_t root_phys, boot_info_t *boot_info);
    jump_to_kernel(out_entry, out_root_phys, (boot_info_t*)(0xFFFF800000000000ULL + (uint64_t)&g_boot_info));

    return EFI_SUCCESS;
}