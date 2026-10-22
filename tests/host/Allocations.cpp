#include "Allocations.h"

#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace {
std::unordered_map<void*, size_t> allocations;
bool psramAvailable = true;

void* record(void* pointer, size_t bytes) {
    if (!pointer || !allocations.emplace(pointer, bytes).second) std::abort();
    return pointer;
}
}

void* host_malloc(size_t bytes) { return record(std::malloc(bytes), bytes); }
void* host_calloc(size_t count, size_t bytes) { return record(std::calloc(count, bytes), count * bytes); }

void host_free(void* pointer) {
    if (!pointer) return;
    if (allocations.erase(pointer) != 1) {
        std::fputs("Untracked allocation or double free\n", stderr);
        std::abort();
    }
    std::free(pointer);
}

size_t host_live_allocation_count() { return allocations.size(); }
size_t host_live_allocation_bytes() {
    size_t total = 0;
    for (const auto& allocation : allocations) total += allocation.second;
    return total;
}
size_t host_live_allocations_of_size(size_t bytes) {
    size_t count = 0;
    for (const auto& allocation : allocations) count += allocation.second == bytes;
    return count;
}
void host_discard_live_allocations() {
    for (const auto& allocation : allocations) std::free(allocation.first);
    allocations.clear();
}
bool host_psram_available() { return psramAvailable; }
void host_set_psram_available(bool available) { psramAvailable = available; }
