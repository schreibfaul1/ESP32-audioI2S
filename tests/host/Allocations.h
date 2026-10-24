#pragma once

#include <cstddef>

void* host_malloc(size_t bytes);
void* host_calloc(size_t count, size_t bytes);
void host_free(void* pointer);
size_t host_live_allocation_count();
size_t host_live_allocation_bytes();
size_t host_live_allocations_of_size(size_t bytes);
void host_discard_live_allocations();
bool host_psram_available();
void host_set_psram_available(bool available);
