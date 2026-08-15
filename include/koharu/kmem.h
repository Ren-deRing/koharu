#pragma once

#include <stddef.h>

void* kmalloc(size_t size);
void kfree(void *ptr);
void* krealloc(void *ptr, size_t size);

void* kmalloc_aligned(size_t size, size_t alignment);
void kfree_aligned(void* ptr);