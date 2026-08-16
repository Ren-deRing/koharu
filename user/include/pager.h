#pragma once

#include <elf.h>

#include <stdint.h>

void pager_init(uint64_t self_tid, uint64_t frame_count);
int load_segment(uint64_t child, const uint8_t *elf, const struct elf64_phdr *ph);
int map_stack(uint64_t child);
int map_shared(uint64_t child);
void serve(uint64_t child);
