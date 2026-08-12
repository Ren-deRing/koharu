#include <elf.h>
#include <bootinfo.h>
#include <uefi.h>

typedef struct {
    char magic[6]; // 070701
    char ino[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];
    char filesize[8];
    char devmajor[8];
    char devminor[8];
    char rdevmajor[8];
    char rdevminor[8];
    char namesize[8];
    char check[8];
} __attribute__((packed)) cpio_header_t;

static inline uint32_t parse_hex8(const char *hex) {
    uint32_t val = 0;
    for (int i = 0; i < 8; i++) {
        val <<= 4;
        if (hex[i] >= '0' && hex[i] <= '9') val |= (hex[i] - '0');
        else if (hex[i] >= 'a' && hex[i] <= 'f') val |= (hex[i] - 'a' + 10);
        else if (hex[i] >= 'A' && hex[i] <= 'F') val |= (hex[i] - 'A' + 10);
    }
    return val;
}

efi_status_t cpio_extract(void *cpio_base, char *target_filename, uint64_t *out_base,size_t *out_size) {
    uintptr_t base_addr = (uintptr_t)cpio_base;
    uintptr_t current_offset = 0;

    while (1) {
        cpio_header_t *header = (cpio_header_t *)(base_addr + current_offset);

        // check magic number (070701)
        if (strncmp(header->magic, "070701", 6) != 0) {
            return EFI_LOAD_ERROR; // how?
        }

        uint32_t filesize = parse_hex8(header->filesize);
        uint32_t namesize = parse_hex8(header->namesize);

        char *filename = (char *)(base_addr + current_offset + sizeof(cpio_header_t));

        // TRAILER!!!
        if (strncmp(filename, "TRAILER!!!", 10) == 0) {
            break;
        } // i like that

        // find file data start
        // hader + namesize + padding (4B)
        size_t data_offset = sizeof(cpio_header_t) + namesize;
        data_offset = (data_offset + 3) & ~3; // 4B padding


        if (strcmp(filename, target_filename) == 0) { // is this....?
            *out_base = (base_addr + current_offset + data_offset);
            *out_size = filesize;
            return EFI_SUCCESS; // oh good let's go
        }

        // next file
        size_t next_record_offset = data_offset + filesize;
        next_record_offset = (next_record_offset + 3) & ~3;

        current_offset += next_record_offset;
    }

    return EFI_NOT_FOUND; // no target_filename here....
}

efi_status_t load_bootbin(boot_info_t *boot_info) {
    if (!boot_info) return EFI_INVALID_PARAMETER;

    efi_status_t status;
    efi_guid_t loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    efi_guid_t sfsp_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

    efi_loaded_image_protocol_t *loaded_image = NULL;
    status = BS->HandleProtocol(IM, &loaded_image_guid, (void**)&loaded_image);
    if (EFI_ERROR(status)) return status;

    efi_simple_file_system_protocol_t *file_system = NULL;
    status = BS->HandleProtocol(loaded_image->DeviceHandle, &sfsp_guid, (void**)&file_system);
    if (EFI_ERROR(status)) return status;

    efi_file_handle_t *root = NULL;
    status = file_system->OpenVolume(file_system, &root);
    if (EFI_ERROR(status)) return status;

    efi_file_handle_t *file = NULL;
    status = root->Open(root, &file, L"bootbin.cpio", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        root->Close(root);
        return status;
    }

    uint64_t file_size = 0;
    file->SetPosition(file, 0xFFFFFFFFFFFFFFFFULL);
    file->GetPosition(file, &file_size);
    file->SetPosition(file, 0);

    if (file_size == 0) {
        file->Close(file);
        root->Close(root);
        return EFI_LOAD_ERROR;
    }

    void *bootbin_base = NULL;
    status = BS->AllocatePool(EfiLoaderData, (size_t)file_size, &bootbin_base);
    if (EFI_ERROR(status)) {
        file->Close(file);
        root->Close(root);
        return status;
    }

    uintn_t read_bytes = (uintn_t)file_size;
    status = file->Read(file, &read_bytes, bootbin_base);

    file->Close(file);
    root->Close(root);

    if (EFI_ERROR(status) || read_bytes != file_size) {
        BS->FreePool(bootbin_base);
        return EFI_LOAD_ERROR;
    }

    uint64_t kernel_base = 0;
    size_t kernel_size = 0;
    status = cpio_extract(bootbin_base, "kernel.elf", &kernel_base, &kernel_size);
    if (EFI_ERROR(status)) {
        BS->FreePool(bootbin_base);
        return status;
    }

    uint64_t initrd_base = 0;
    size_t initrd_size = 0;
    status = cpio_extract(bootbin_base, "initrd.cpio", &initrd_base, &initrd_size);
    if (EFI_ERROR(status)) {
        BS->FreePool(bootbin_base);
        return status;
    }

    uint64_t init_base = 0;
    size_t init_size = 0;
    status = cpio_extract((void *)initrd_base, "koharu.ppm", &init_base, &init_size); // for now.
    if (EFI_ERROR(status)) {
        init_base = 0;
        init_size = 0;
    }

    boot_info->kernel.kernel_src  = kernel_base;
    boot_info->kernel.kernel_size = kernel_size;
    boot_info->init.addr          = init_base;
    boot_info->init.size          = init_size;
    boot_info->initrd_addr        = initrd_base;

    return EFI_SUCCESS;
}

efi_status_t load_elf_kernel_uefi(uint64_t kernel_src, uint64_t *out_entry_point) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)kernel_src;

    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        return EFI_INVALID_PARAMETER;
    }

    Elf64_Phdr *phdr = (Elf64_Phdr *)((uint8_t *)kernel_src + ehdr->e_phoff);

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            uintptr_t phys_start = phdr[i].p_vaddr - 0xFFFFFFFF80000000ULL;
            
            uintptr_t page_start = phys_start & ~0xFFFULL;
            uintptr_t phys_end   = phys_start + phdr[i].p_memsz;
            uintptr_t page_end   = (phys_end + 0xFFFULL) & ~0xFFFULL;
            size_t num_pages     = (page_end - page_start) / 4096;

            efi_physical_address_t alloc_addr = page_start;
            efi_status_t status = BS->AllocatePages(AllocateAddress, EfiRuntimeServicesCode, num_pages, &alloc_addr);
            
            if (EFI_ERROR(status)) return status;

            uint8_t *dest = (uint8_t *)phys_start;
            uint8_t *src  = (uint8_t *)kernel_src + phdr[i].p_offset;

            if (phdr[i].p_filesz > 0) {
                memcpy(dest, src, phdr[i].p_filesz);
            }

            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset(dest + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }
    }

    *out_entry_point = ehdr->e_entry;
    return EFI_SUCCESS;
}