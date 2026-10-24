#pragma once

#include "Allocations.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <strings.h>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef __unused
#define __unused __attribute__((unused))
#endif
#define log_e(...) ((void)0)
#define log_w(...) ((void)0)
#define log_i(...) ((void)0)
#define log_d(...) ((void)0)
#define log_v(...) ((void)0)
using std::max;
using std::min;

inline bool psramFound() { return host_psram_available(); }
inline void* ps_malloc(size_t bytes) { return host_malloc(bytes); }
inline void* ps_calloc(size_t count, size_t bytes) { return host_calloc(count, bytes); }

// Only decoder translation units include this platform adapter. The ledger's
// implementation includes standard headers directly, without these redirects.
#define malloc host_malloc
#define calloc host_calloc
#define free host_free
