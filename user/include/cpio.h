#pragma once

#include <stddef.h>
#include <stdint.h>

int cpio_extract(void *cpio_base, char *target_filename, uint64_t *out_base, size_t *out_size);
