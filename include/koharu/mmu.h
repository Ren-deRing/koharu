#pragma once

void* pmm_alloc_pages(int order);
void pmm_free_pages(void* addr, int order);