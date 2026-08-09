#include <bootinfo.h>
#include <uefi.h>

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

efi_status_t init_mmap(mmap_info_t *mmap_info) {
    uintn_t map_size = 0, map_key = 0, desc_size = 0;
    uint32_t desc_ver = 0;
    efi_status_t status;

    status = BS->GetMemoryMap(&map_size, NULL, &map_key, &desc_size, &desc_ver);
    if (status != EFI_BUFFER_TOO_SMALL) return status;

    map_size += 8 * desc_size;

    efi_memory_descriptor_t *map = NULL;
    status = BS->AllocatePool(EfiLoaderData, map_size, (void**)&map);
    if (EFI_ERROR(status)) return status;

    status = ST->BootServices->GetMemoryMap(&map_size, map, &map_key, &desc_size, &desc_ver);
    if (status != EFI_SUCCESS) {
        BS->FreePool(map);
        return status;
    }

    uintn_t total_entries = map_size / desc_size;
    mmap_entry_t *entries = NULL;
    
    status = BS->AllocatePool(EfiLoaderData, sizeof(mmap_entry_t) * total_entries, (void**)&entries);
    if (EFI_ERROR(status)) {
        BS->FreePool(map);
        return status;
    }

    uint64_t max_phys = 0;
    uint64_t total_usable = 0;

    for (uintn_t i = 0; i < total_entries; i++) {
        efi_memory_descriptor_t *desc = (efi_memory_descriptor_t *)((uint8_t *)map + (i * desc_size));

        entries[i].phys_start = desc->PhysicalStart;
        entries[i].page_count = desc->NumberOfPages;
        entries[i].type       = desc->Type;
        entries[i].flags      = desc->Attribute;

        uint64_t size_bytes = desc->NumberOfPages * 4096;
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

    BS->FreePool(map);

    return EFI_SUCCESS;
}

int main(int argc, char **argv) {
    printf("kinit148\n\n");

    if (EFI_ERROR(init_gop(&g_boot_info.framebuffer))) {
        printf("GOP failed!\n");
        return 1;
    }
    if (EFI_ERROR(init_mmap(&g_boot_info.memory))) {
        printf("mmap failed!\n");
        return 1;
    }

    return EFI_SUCCESS;
}