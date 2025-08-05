#pragma once

#include "Arduino.h"
#include <memory>
#include <cstring>
#include <cstdio>
#include <utility>
#include <algorithm>
#include <type_traits>

#ifndef PS_PTR_CLASS
#define PS_PTR_CLASS 1

/** Auxiliary functions for using Unique Pointers in ESP32 PSRAM **/

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// PSRAM Deleter (ps_malloc → free)
struct PsramDeleter {
    void operator()(void* ptr) const {
        if (ptr) {
            free(ptr);
        }
    }
};

// Auxiliary function: Comparison of two strings case inensitive, only up to n characters
inline int strncasecmp_local(const char* s1, const char* s2, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        unsigned char c1 = static_cast<unsigned char>(s1[i]);
        unsigned char c2 = static_cast<unsigned char>(s2[i]);
        if (tolower(c1) != tolower(c2)) return tolower(c1) - tolower(c2);
        if (c1 == '\0') break;
    }
    return 0;
}

template <typename T>

class ps_ptr {
private:
    std::unique_ptr<T[], PsramDeleter> mem;
    size_t allocated_size = 0;
    static inline T dummy{}; // For invalid accesses

public:
    ps_ptr() = default;
//    ~ps_ptr() = default;
    ~ps_ptr() {
        if (mem) {
            // log_w("Destructor called: Freeing %d bytes at %p", allocated_size * sizeof(T), mem.get());
        } else {
            // log_w("Destructor called: No memory to free.");
        }
    }

    explicit operator bool() const {
        return mem.get() != nullptr; // Or just 'return (bool) mem;'if unique_ptr :: operator bool () is used
    }

    ps_ptr(ps_ptr&& other) noexcept { // move-constructor
        mem = std::move(other.mem);
        allocated_size = other.allocated_size;
        other.allocated_size = 0;
    }

    // NEU: Move-Assignment-Operator
    ps_ptr& operator=(ps_ptr&& other) noexcept {
        if (this != &other) { // Sicherstellen, dass keine Selbstzuweisung stattfindet
            mem = std::move(other.mem);
            allocated_size = other.allocated_size;
            other.allocated_size = 0;
            // log_w("Move Assignment called: Moved %d bytes from %p to %p", allocated_size * sizeof(T), other.mem.get(), mem.get());
        }
        return *this;
    }

    // Optional: Explicitly prohibit copy constructor (helpful in troubleshooting)
    ps_ptr(const ps_ptr&) = delete;
    ps_ptr& operator=(const ps_ptr&) = delete;

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// Prototypes:



// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  A L L O C  📌📌📌

    // ps_ptr <char>test;
    // test.alloc(100, "test"); // ps_malloc, It is not necessary to specify a name, only for debugging OOM etc.
    // if(test.valid()) {
    //     printf("Größe: %u\n", test.size());
    //     test.clear(); // memset "0"
    // }

    void alloc(std::size_t size, const char* name = nullptr, bool usePSRAM = true) {
        size = (size + 15) & ~15; // Align to 16 bytes
        if (psramFound() && usePSRAM) { // Check at the runtime whether PSRAM is available
            mem.reset(static_cast<T*>(ps_malloc(size)));  // <--- Important!
        }
        else{
            mem.reset(static_cast<T*>(malloc(size)));  // <--- Important!
        }
        allocated_size = size;
        if (!mem) {
            printf("OOM: failed to allocate %zu bytes for %s\n", size, name ? name : "unnamed");
        }
    }

    void alloc(const char* name = nullptr) { // alloc for single objects/structures
        reset();  // Freigabe des zuvor gehaltenen Speichers
        void* raw_mem = nullptr;
        if (psramFound()) { // Check at the runtime whether PSRAM is available
            raw_mem = ps_malloc(sizeof(T)); // allocated im PSRAM
        }
        else{
            raw_mem = malloc(sizeof(T)); // allocated im RAM
        }
        if (raw_mem) {
            mem.reset(new (raw_mem) T()); // Platziertes New: Konstruktor von T wird im PSRAM aufgerufen
            allocated_size = sizeof(T);
        } else {
            printf("OOM: failed to allocate %zu bytes for %s\n", sizeof(T), name ? name : "unnamed");
            allocated_size = 0; // Sicherstellen, dass allocated_size 0 ist, wenn Allokation fehlschlägt
        }
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C A L L O C    📌📌📌
    /**
     * @brief Allocates and zeroes out memory for an array of elements.
     * Chooses between PSRAM (if available) and DRAM.
     * @param num_elements The number of elements to allocate space for.
     * @param name Optional name for OOM messages.
     */
    void calloc(std::size_t num_elements, const char* name = nullptr, bool usePSRAM = true) {
        size_t total_size = num_elements * sizeof(T);
        total_size = (total_size + 15) & ~15; // Align to 16 bytes, consistent with your alloc()

        reset(); // Release of the previously held memory

        void* raw_mem = nullptr;

        if (psramFound() && usePSRAM) { // Check at the runtime whether PSRAM is available
            raw_mem = ps_malloc(total_size);
        }
        else {
            raw_mem = malloc(total_size);
        }

        if (raw_mem) {
            // Initialize memory with zeros how Calloc () does it
            memset(raw_mem, 0, total_size);

            // Connect the allocated memory to the Unique_PTR
            mem.reset(static_cast<T*>(raw_mem));
            allocated_size = total_size;
        } else {
            // Error treatment for storage allocation
            printf("OOM: failed to calloc %zu bytes for %s\n", total_size, name ? name : "unnamed");
            allocated_size = 0; // Sicherstellen, dass allocated_size 0 ist, wenn Allokation fehlschlägt
        }
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  A L L O C _ A R R A Y   📌📌📌

    void alloc_array(std::size_t count, const char* name = nullptr) {
        alloc(sizeof(T) * count, name);
        clear();
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  Z E R O _ M E M  📌📌📌
    /**
     * @brief Sets the allocated memory block to all zeroes.
     * Only operates if memory is currently allocated.
     */
    void zero_mem() {
        if (mem) { // Check whether memory is assigned (use the new operator bool ())
            // Use allocated_aize to zero the actual size of the assigned block.
            // This is important because alloc() can also be called up with a specific size.
            memset(mem.get(), 0, allocated_size);
        }
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  R E A L L O C  📌📌📌

    // test.realloc(200, "test"); // It is not necessary to specify a name, only for debugging OOM etc.
    // printf("new size:  %i\n", test.size());
    // printf("%s\n", test.get());

    void realloc(size_t new_size, const char* name = nullptr) {
        void* new_mem = nullptr;
        if (psramFound()) { // Check at the runtime whether PSRAM is available
            new_mem = ps_malloc(new_size);
        }
        else{
            new_mem = malloc(new_size);
        }
        if (!new_mem) {
            printf("OOM: failed to realloc %zu bytes for %s\n", new_size, name ? name : "unnamed");
            return;
        }

        if (mem && allocated_size > 0) {
            std::memcpy(new_mem, mem.get(), std::min(allocated_size, new_size));
        }

        mem.reset(static_cast<T*>(new_mem));
        allocated_size = new_size;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  A S S I G N  📌📌📌

    // ps_ptr <char> my_str1;
    // my_str1.assign("Hallo", "my_str1"); // ps_strdup()
    // printf("%s\n", my_str1.get());
    // my_str1.get()[3] = 'x';
    // printf("%s\n", my_str1.get());


    void assign(const char* src, const char* name = nullptr) { // Only for T = char: Is similar to strdup (full copy)
        static_assert(std::is_same_v<T, char>, "assign(const char*) is only valid for ps_ptr<char>");
        if (!src) {
            reset();
            return;
        }
        std::size_t len = std::strlen(src) + 1;
        alloc(len, name);
        if (mem) {
            std::memcpy(mem.get(), src, len);
        }
    }

    // ps_ptr <char> my_str2;
    // my_str2.assign("Hallo", 5, "my_str1"); // ps_strndup()
    // printf("%s\n", my_str2.get());


    // Only for T T = char: similar to strndup (max. n chars)
    void assign(const char* src, std::size_t max_len, const char* name = nullptr) {
        static_assert(std::is_same_v<T, char>, "assign(const char*, size_t) is only valid for ps_ptr<char>");
        if (!src) {
            reset();
            return;
        }
        std::size_t actual_len = strnlen(src, max_len);
        std::size_t total = actual_len + 1;
        alloc(total, name);
        if (mem) {
            std::memcpy(mem.get(), src, actual_len);
            static_cast<char*>(mem.get())[actual_len] = '\0';
        }
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C O P Y _ F R O M   📌📌📌
    // Counted Count elements from the external pointer to the PSRAM, similar to memcpy

    // Strings
    // const char* msg = "Hallo";
    // ps_ptr<char> text;
    // text.copy_from(msg, strlen(msg) + 1, "text");

    // Integer-Array
    // int32_t values[] = {10, 20, 30};
    // ps_ptr<int32_t> data;
    // data.copy_from(values, 3, "data");

    // Float-Array
    // float samples[] = {0.1f, 0.2f, 0.3f};
    // ps_ptr<float> buf;
    // buf.copy_from(samples, 3, "buffer");
    // printf("%f\n", buf.get()[2]);
    // buf.get()[2] = 4.5;
    // printf("%f\n", buf.get()[2]);

    void copy_from(const T* src, std::size_t count, const char* name = nullptr) {
        std::size_t bytes = (count + 1) * sizeof(T);  // +1 For zero terminator
        alloc(bytes, name);
        if (mem && src) {
            std::memcpy(mem.get(), src, count * sizeof(T));
            mem.get()[count] = '\0';  // Zero
        }
    }

    size_t copy_from(const T* src, const char* name = nullptr) {  // for strings
        if(src == nullptr){log_e("arg. is null"); return 0;}
        std::size_t count = std::strlen(src) + 1;
        std::size_t bytes = count * sizeof(T);
        alloc(bytes, name);
        if (mem && src) {
            std::memcpy(mem.get(), src, bytes);
        }
        return bytes;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C O P Y _ F R O M _ U T F 1 6   📌📌📌
    // copies all characters up to the terminator "\0\0" and converted into UTF-8
    // source: 0x00, 0x4C, 0x00, 0x69, 0x00, 0x74, 0x00, 0x74, 0x00, 0x6C, 0x00, 0x65, 0x00, 0x20, 0x00, 0x4C, 0x00, 0x6F, 0x00, 0x6E, 0x00, 0x64, 0x00, 0x6F, 0x00, 0x6E, 0x00, 0x20, 0x00, 0x47, 0x00,
    //         0x69, 0x00, 0x72, 0x00, 0x6C, 0x00, 0x00
    // is UTF-16LE and will converted to: "Little London Girl"
    // UTF-16LE and UTF-16BE is often found in ID3 header

#include <vector>
#include <cstdint>
#include <cstring>
#include <stdexcept>

// convert UTF-16 to UTF-8 and stop at zero terminator
size_t copy_from_utf16(const uint8_t* src, bool is_big_endian = false, const char* name = nullptr) {
    if (!src) {
        log_e("arg. is null");
        return 0;
    }
    std::vector<char> out;
    size_t i = 0;

    // BOM-Handling
    if (src[i] == 0xFF && src[i + 1] == 0xFE) {
        is_big_endian = false; // UTF-16LE
        i += 2;
    } else if (src[i] == 0xFE && src[i + 1] == 0xFF) {
        is_big_endian = true;  // UTF-16BE
        i += 2;
    }

    while (true) {
        // Prüfe, ob genug Bytes für ein UTF-16-Zeichen vorhanden sind
        if (i + 1 >= std::numeric_limits<size_t>::max() || (src[i] == 0x00 && src[i + 1] == 0x00)) {
            break; // Nullterminator oder Ende des Puffers
        }

        uint16_t ch;
        if (is_big_endian) {
            ch = (src[i] << 8) | src[i + 1];
        } else {
            ch = (src[i + 1] << 8) | src[i];
        }
        i += 2;

        uint32_t codepoint = ch;

        // Prüfe auf Surrogatenpaare
        if (ch >= 0xD800 && ch <= 0xDBFF) { // High surrogate
            if ((i + 1 >= std::numeric_limits<size_t>::max()) || (src[i] == 0x00 && src[i + 1] == 0x00)) {
                log_e("Invalid surrogate pair: missing low surrogate");
                break;
            }
            uint16_t ch2;
            if (is_big_endian) {
                ch2 = (src[i] << 8) | src[i + 1];
            } else {
                ch2 = (src[i + 1] << 8) | src[i];
            }
            if (ch2 < 0xDC00 || ch2 > 0xDFFF) {
                log_e("Invalid surrogate pair: invalid low surrogate");
                break;
            }
            i += 2;
            codepoint = 0x10000 + ((ch - 0xD800) << 10) + (ch2 - 0xDC00);
        } else if (ch >= 0xDC00 && ch <= 0xDFFF) {
            log_e("Invalid surrogate pair: unexpected low surrogate");
            break;
        }

        // UTF-16 → UTF-8
        if (codepoint < 0x80) {
            out.push_back(static_cast<char>(codepoint));
        } else if (codepoint < 0x800) {
            out.push_back(0xC0 | (codepoint >> 6));
            out.push_back(0x80 | (codepoint & 0x3F));
        } else if (codepoint < 0x10000) {
            out.push_back(0xE0 | (codepoint >> 12));
            out.push_back(0x80 | ((codepoint >> 6) & 0x3F));
            out.push_back(0x80 | (codepoint & 0x3F));
        } else if (codepoint < 0x110000) {
            out.push_back(0xF0 | (codepoint >> 18));
            out.push_back(0x80 | ((codepoint >> 12) & 0x3F));
            out.push_back(0x80 | ((codepoint >> 6) & 0x3F));
            out.push_back(0x80 | (codepoint & 0x3F));
        } else {
            log_e("Invalid codepoint");
            break;
        }
    }

    // Nullterminator hinzufügen
    out.push_back('\0');

    // Speicher allozieren und kopieren
    std::size_t bytes = out.size();
    alloc(bytes, name);
    std::memcpy(mem.get(), out.data(), bytes);
    return i;
}

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C O P Y _ F R O M _ I S O _ 8 8 5 9 - 1   📌📌📌
    // convert ISO 8859-1 to UTF-8 and stop at zero terminator
    // 0x48 0x65 0x6C 0x6C 0x6F 0x20 0xC3 0xA4 0x62 0x63 0x00  -> "Hello äbc"

    size_t copy_from_iso8859_1(const uint8_t* src, const char* name = nullptr) {
        if (!src) {
            log_e("arg. is null");
            return 0;
        }
        std::vector<char> out;
        size_t i = 0;

        while (true) {
            uint8_t ch = src[i];
            if (ch == 0x00) {
                break; // 'Nullterminator'
            }

            // ISO-8859-1 → UTF-8
            if (ch < 0x80) {
                out.push_back(static_cast<char>(ch)); // Ascii area remains unchanged
            } else {
                // chars from 0x80 to 0xff are coded as 2-byte sequences in UTF-8
                out.push_back(0xC0 | (ch >> 6));
                out.push_back(0x80 | (ch & 0x3F));
            }
            i++;
        }

        // add zero terminator
        out.push_back('\0');

        // allocate and copy memory
        std::size_t bytes = out.size();
        alloc(bytes, name);
        std::memcpy(mem.get(), out.data(), bytes);
        return i;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C L O N E _ F R O M  📌📌📌

    // ps_ptr<char> source;
    // source.assign("Hello World");
    // ps_ptr<char> copy;
    // copy.clone_from(source);
    // printf("%s\n", copy.get());  // → Hello World

    void clone_from(const ps_ptr<T>& other, const char* name = nullptr) {
        if (!other.valid() || other.size() == 0) {
            reset();
            return;
        }

        std::size_t sz = other.size();
        alloc(sz, name);
        if (mem && sz > 0){
            std::memcpy(mem.get(), other.get(), sz);
        }
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  S W A P   📌📌📌
    // A.swap(B);
    void swap(ps_ptr<T>& other) noexcept {
        std::swap(this->mem, other.mem);
        std::swap(this->allocated_size, other.allocated_size);
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  S W A P   W I T H   R A W   P O I N T E R   📌📌📌
    void swap_with_pointer(T*& raw_ptr) noexcept {
        T* temp = get();
        mem.release(); // Gib Besitz auf, ohne zu löschen
        mem = std::unique_ptr<T, PsramDeleter>(raw_ptr); // Übernehme neuen Zeiger
        raw_ptr = temp;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  A P P E N D  📌📌📌

    // ps_ptr<char> text1; // like Strcat with automatic new allocation
    // text1.assign("Hallo", "text");
    // text1.append(", Welt!");
    // printf("%s\n", text1.get());  // → "Hallo, Welt!"

    template <typename U = T>
    requires std::is_same_v<U, char>
    void append(const char* suffix) {
        if (!suffix || !*suffix) return;

        std::size_t old_len = mem ? std::strlen(static_cast<char*>(mem.get())) : 0;
        std::size_t add_len = std::strlen(suffix);
        std::size_t new_len = old_len + add_len + 1; // +1 für null-Terminator

        // Alten Speicher übernehmen
        char* old_data = static_cast<char*>(mem.release());

        if (psramFound()) { // Check at the runtime whether PSRAM is available
            mem.reset(static_cast<T*>(ps_malloc(new_len)));  // <--- Important!
        }
        else{
            mem.reset(static_cast<T*>(malloc(new_len)));
        }
        if (!mem) {
            printf("OOM: append() failed for %zu bytes\n", new_len);
            return;
        }

        // Copy existing content
        if (old_data) {
            std::memcpy(mem.get(), old_data, old_len);
            free(old_data);
        }

        // Suffix anhängen
        std::memcpy(static_cast<char*>(mem.get()) + old_len, suffix, add_len + 1);
        allocated_size = new_len;
        static_cast<char*>(mem.get())[old_len + add_len] = '\0';
    }

    // ps_ptr<char> text1; // like Strcat with automatic new allocation
    // text1.assign("Hallo", "text");
    // text1.append(", Welt!", 4);
    // printf("%s\n", text1.get());  // → "Hallo, We"


    template <typename U = T>
    requires std::is_same_v<U, char>
    void append(const char* suffix, std::size_t len) {
        if (!suffix || len == 0) return;

        std::size_t old_len = mem ? std::strlen(static_cast<char*>(mem.get())) : 0;
        std::size_t new_len = old_len + len + 1; // +1 für null-Terminator

        char* old_data = static_cast<char*>(mem.release());

        if (psramFound()) { // Check at the runtime whether PSRAM is available
            mem.reset(static_cast<T*>(ps_malloc(new_len)));
        }
        else{
            mem.reset(static_cast<T*>(malloc(new_len)));
        }
        if (!mem) {
            printf("OOM: append(len) failed for %zu bytes\n", new_len);
            return;
        }

        if (old_data) {
            std::memcpy(mem.get(), old_data, old_len);
            free(old_data);
        }

        std::memcpy(static_cast<char*>(mem.get()) + old_len, suffix, len);
        static_cast<char*>(mem.get())[old_len + len] = '\0';
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  S T A R T S _ W I T H   📌📌📌

    // ps_ptr<char> s;
    // s.assign("https://example.com/file.mp3");
    //
    // if (s.starts_with("https://")) {
        // printf("https-link recognized\n");
    // }

    // Only for T = Char: Check whether the string begins with the given prefix
    template <typename U = T>
    requires std::is_same_v<U, char>
    bool starts_with(const char* prefix) const {
        if (!mem || !prefix) return false;

        const char* str = static_cast<const char*>(mem.get());
        std::string_view sv(str);  // C++17, sicherer als strlen
        return sv.starts_with(prefix);
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  E N D S _ W I T H   📌📌📌

    // ps_ptr<char> s;
    // s.assign("https://example.com/file.mp3");

    // if (s.ends_with(".mp3")) {
    //     printf("mp3-file recognized\n");
    // }


    // Only for T = Char: Check whether the string ends with the given suffix
    template <typename U = T>
    requires std::is_same_v<U, char>
    bool ends_with(const char* prefix) const {
        if (!mem || !prefix) return false;

        const char* str = static_cast<const char*>(mem.get());
        std::string_view sv(str);  // C++17, sicherer als strlen
        return sv.ends_with(prefix);
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  S T A R T S _ W I T H _ I C A S E   📌📌📌

    // ps_ptr<char> url;
    // url.assign("https://example.com/TRACK.MP3");
    //
    // if (url.starts_with_icase("HTtp")) {
    //     printf("http recognized (case-insensitive)\n");
    // }


    // case non-sensitive: starts with Prefix?
    template <typename U = T>
    requires std::is_same_v<U, char>
    bool starts_with_icase(const char* prefix) const {
        if (!mem || !prefix) return false;

        const char* str = static_cast<const char*>(mem.get());
        std::size_t prefix_len = std::strlen(prefix);
        std::size_t str_len = std::strlen(str);
        if (prefix_len > str_len) return false;

        return strncasecmp_local(str, prefix, prefix_len) == 0;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  E N D S _ W I T H _ I C A S E   📌📌📌

    // ps_ptr<char> url;
    // url.assign("https://example.com/TRACK.MP3");
    //
    // if (url.ends_with_icase(".mp3")) {
    //     printf("MP3 recognized (case-insensitive)\n");
    // }

    // case non-sensitive: ends with suffix?
    template <typename U = T>
    requires std::is_same_v<U, char>
    bool ends_with_icase(const char* suffix) const {
        if (!mem || !suffix) return false;

        const char* str = static_cast<const char*>(mem.get());
        std::size_t suffix_len = std::strlen(suffix);
        std::size_t str_len = std::strlen(str);
        if (suffix_len > str_len) return false;

        return strncasecmp_local(str + str_len - suffix_len, suffix, suffix_len) == 0;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  E Q U A L S   📌📌📌
    // my_ps_ptr t1, t2;
    // t1.assign("Hallo");
    // t2.assign("Hallo");
    //
    // if (t1.equals(t2)) {
    //    👍 is equal
    // }
    bool equals(const ps_ptr<T>& other) const {
        if(!this->valid() || !other.valid()) return false;
        if (!this->get() || !other.get()) return false;
        return strcmp(this->get(), other.get()) == 0;
    }

    bool equals(const char* other) const {
        if (!this->valid()) return false;
        const char* myStr = this->get();
        if (!myStr || !other) return false;
        return strcmp(myStr, other) == 0;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  A S S I G N F   📌📌📌

    // ps_ptr<char> message;
    // message.assignf("Code %d, Modul %s", 404, "Network");
    // printf("%s\n", message.get());  // → Error: Code 404, Modul Network

    // onli activate if T = char
    template <typename U = T>
    requires std::is_same_v<U, char>
    void assignf(const char* fmt, ...) {
        if (!fmt) return;
        // Formatierte Länge berechnen
        va_list args;
        va_start(args, fmt);
        va_list args_copy;
        va_copy(args_copy, args);
        int fmt_len = vsnprintf(nullptr, 0, fmt, args_copy);
        va_end(args_copy);

        if (fmt_len < 0) {
            va_end(args);
            return;
        }

        std::size_t new_len = static_cast<std::size_t>(fmt_len) + 1;

        // share previous memory and new allocates
        reset();
        alloc(new_len);
        if (!mem) {
            printf("OOM: assignf() failed for %zu bytes\n", new_len);
            va_end(args);
            return;
        }

        // write formatted text
        vsnprintf(static_cast<char*>(mem.get()), new_len, fmt, args);
        va_end(args);
        allocated_size = fmt_len;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  A P P E N D F   📌📌📌

    // ps_ptr<char> message;
    // message.assign("Error: ");
    // message.appendf("Code %d, Modul %s", 404, "Network");
    // printf("%s\n", message.get());  // → Error: Code 404, Modul Network

    // Nur aktivieren, wenn T = char
    template <typename U = T>
    requires std::is_same_v<U, char>
    void appendf(const char* fmt, ...) {
        if (!fmt) return;

        // Formatstring vorbereiten
        va_list args;
        va_start(args, fmt);
        va_list args_copy;
        va_copy(args_copy, args);
        int add_len = vsnprintf(nullptr, 0, fmt, args_copy);
        va_end(args_copy);

        if (add_len < 0) {
            va_end(args);
            return;
        }

        std::size_t old_len = mem ? std::strlen(static_cast<char*>(mem.get())) : 0;
        std::size_t new_len = old_len + static_cast<std::size_t>(add_len) + 1;

        // Speicher reservieren
        char* old_data = static_cast<char*>(mem.release());
        reset(); alloc(new_len);
        if (!mem) {
            printf("OOM: appendf() failed for %zu bytes\n", new_len);
            if (old_data) free(old_data);
            va_end(args);
            return;
        }

        // Vorherigen Inhalt übernehmen
        if (old_data) {
            std::memcpy(mem.get(), old_data, old_len);
            free(old_data);
        }

        // Formatierter Teil wird angehängt
        vsnprintf(static_cast<char*>(mem.get()) + old_len, new_len - old_len, fmt, args);
        va_end(args);
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  A P P E N D F _ V A  📌📌📌

    // void vPrint(const char* fmt, ...) {
    //     ps_ptr<char> myLog;
    //     va_list args;
    //     va_start(args, fmt);
    //     myLog.appendf_va(fmt, args);  // <-
    //     va_end(args);
    //     printf(myLog.c_get());
    // }
    // vPrint("Hallo %i", 19);

    template <typename U = T>
    requires std::is_same_v<U, char>
    void appendf_va(const char* fmt, va_list args) {
        if (!fmt) return;
        // Formatlänge bestimmen (benötigt Kopie von args!)
        va_list args_copy;
        va_copy(args_copy, args);
        int add_len = vsnprintf(nullptr, 0, fmt, args_copy);
        va_end(args_copy);
        if (add_len < 0) return;
        std::size_t old_len = mem ? std::strlen(static_cast<char*>(mem.get())) : 0;
        std::size_t new_len = old_len + static_cast<std::size_t>(add_len) + 1;
        // Speicher neu reservieren
        char* old_data = static_cast<char*>(mem.release());
        reset(); alloc(new_len);
        if (!mem) {
            printf("OOM: appendf_va() failed for %zu bytes\n", new_len);
            if (old_data) free(old_data);
            return;
        }
        // Vorherigen Inhalt kopieren
        if (old_data) {
            std::memcpy(mem.get(), old_data, old_len);
            free(old_data);
        }
        // Formatierung schreiben
        vsnprintf(static_cast<char*>(mem.get()) + old_len, new_len - old_len, fmt, args);
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌 I N D E X _ O F   📌📌📌

    // char
    // ps_ptr<char> text3;
    // text3.assign("Hello, World!");
    // int pos = text3.index_of('l');     // → 2
    // printf("found  %i\n", pos);
    // int not_found = text3.index_of('x'); // → -1
    // printf("not_found  %i\n", not_found);
    // pos = text3.index_of('l', 5);
    // printf("found  %i\n", pos);

    // const char*
    // ps_ptr<char> path;
    // path.assign("audio/music/song.mp3");
    // int i1 = path.index_of('s');           // → z.B. 6
    // int i2 = path.index_of("song");        // → 12
    // int i3 = path.index_of("audio");       // → 0
    // int i4 = path.index_of("test");        // → -1
    // int i5 = path.index_of("music", 7);    // → -1

    // Array
    // ps_ptr<int> numbers;
    // numbers.copy_from((int[]){10, 20, 30, 40, 50}, 5);
    // int idx = numbers.index_of(30);  // → 2
    // printf("pos  %i\n", idx);

    // General version: for any T (e.g. Int, Float, Structs)
    int index_of(const T& value, std::size_t start = 0) const {
        if (!mem || allocated_size < sizeof(T)) return -1;

        std::size_t count = allocated_size / sizeof(T);
        if (start >= count) return -1;

        T* data = get();
        for (std::size_t i = start; i < count; ++i) {
            if (data[i] == value) return static_cast<int>(i);
        }
        return -1;
    }

    // Specialized version: only for T = char (search for individual characters)
    template <typename U = T>
    requires std::is_same_v<U, char>
    int index_of(char ch, std::size_t start = 0) const {
        if (!mem) return -1;

        const char* str = static_cast<const char*>(mem.get());
        std::size_t len = std::strlen(str);
        if (start >= len) return -1;

        for (std::size_t i = start; i < len; ++i) {
            if (str[i] == ch) return static_cast<int>(i);
        }
        return -1;
    }
    // Overload for const char* (substring search)
    template <typename U = T>
    requires std::is_same_v<U, char>
    int index_of(const char* substr, std::size_t start = 0) const {
        if (!mem || !substr || !*substr) return -1;

        const char* str = static_cast<const char*>(mem.get());
        std::size_t len = std::strlen(str);
        if (start >= len) return -1;

        const char* found = std::strstr(str + start, substr);
        if (!found) return -1;

        return static_cast<int>(found - str);
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  S P E C I A L _ I N D E X _ O F  📌📌📌
    // Searches for the sequence needle within the buffer managed by ps_ptr<char> (haystack), up to max_length bytes.
    // Returns the offset of the first occurrence of needle relative to the buffer start, or -1 if not found.
    // Ignores null bytes in both haystack and needle, making it suitable for binary data searches.
    // Example:
    //   ps_ptr<char> haystack; // Contains data, e.g., stsd atom content
    //   int32_t idx = haystack.index_of("mp4a", 1024); // Search for "mp4a"
    //   int32_t idx2 = haystack.index_of("\x00\x01\xFF", 3, 1024); // Search for byte sequence
    template <typename U = T>
    requires std::is_same_v<U, char>
    int32_t special_index_of(const char* needle, uint32_t needle_length, uint32_t max_length) const {
        static_assert(std::is_same_v<T, char>, "index_of is only valid for ps_ptr<char>");
        if (!mem || !get()) {
            log_e("index_of: No valid buffer data");
            return -1;
        }
        if (!needle || needle_length == 0) {
            log_e("index_of: Invalid needle (null or empty)");
            return -1;
        }
        if (max_length < needle_length) {
            log_e("index_of: max_length (%u) too short to find needle of length %u", max_length, needle_length);
            return -1;
        }
        const char* data = get();
        for (uint32_t i = 0; i <= max_length - needle_length; ++i) {
            if (std::memcmp(data + i, needle, needle_length) == 0) {
                // log_i("index_of: Found needle at offset %u", i);
                return static_cast<int32_t>(i);
            }
        }
        log_d("index_of: Needle not found within %u bytes", max_length);
        return -1;
    }

    // Overload for C-string needle (automatically determines needle length, excluding null terminator)
    template <typename U = T>
    requires std::is_same_v<U, char>
    int32_t special_index_of(const char* needle, uint32_t max_length) const {
        return special_index_of(needle, needle ? std::strlen(needle) : 0, max_length);
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌 I N D E X _ O F _ I C A S E  📌📌📌

    // ps_ptr<char> s;
    // s.assign("Content-Type: audio/mp3");
    //
    // int idx1 = s.index_of_icase("CONTENT");     // 0
    // int idx2 = s.index_of_icase("audio");       // 14
    // int idx3 = s.index_of_icase("MP3");         // 20
    // int idx4 = s.index_of_icase("notfound");    // -1

    // Case-insensitive substring search
    template <typename U = T>
    requires std::is_same_v<U, char>
    int index_of_icase(const char* substr, std::size_t start = 0) const {
        if (!mem || !substr || !*substr) return -1;

        const char* str = static_cast<const char*>(mem.get());
        std::size_t len_str = std::strlen(str);
        std::size_t len_sub = std::strlen(substr);
        if (start >= len_str || len_sub == 0 || len_sub > len_str) return -1;

        for (std::size_t i = start; i <= len_str - len_sub; ++i) {
            bool match = true;
            for (std::size_t j = 0; j < len_sub; ++j) {
                if (std::tolower(str[i + j]) != std::tolower(substr[j])) {
                    match = false;
                    break;
                }
            }
            if (match) return static_cast<int>(i);
        }
        return -1;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌 L A S T _ I N D E X _ O F   📌📌📌

    template <typename U = T>
    requires std::is_same_v<U, char>
    int last_index_of(char ch) const {
        if (!mem) return -1;

        const char* str = static_cast<const char*>(mem.get());
        int len = static_cast<int>(std::strlen(str));
        for (int i = len - 1; i >= 0; --i) {
            if (str[i] == ch) return i;
        }
        return -1;
    }

    // ps_ptr<char> str1;
    // str1.assign("OpenAI API Test");
    // int last_i = str1.last_index_of('i');  // → -1 (no 'i', but 'I' ≠ 'i')
    // int last_A = str1.last_index_of('A');  // → 5
    // printf("last_i  %i\n", last_i);
    // printf("last_A  %i\n", last_A);

    int last_index_of(const T& value) const {
        if (!mem || allocated_size < sizeof(T)) return -1;

        std::size_t count = allocated_size / sizeof(T);
        T* data = get();
        for (int i = static_cast<int>(count) - 1; i >= 0; --i) {
            if (data[i] == value) return i;
        }
        return -1;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌 I N D E X _ O F _ S U B S T R   📌📌📌

    // source must not be null terminated
    // ps_ptr<char> buffer;
    // buffer.assign("abc123ID3TAGthisisjustatest...", "mp3scan");
    // int pos1 = buffer.index_of_substr("ID3", 20); // → finds "ID3" at index 6
    // printf("ID3 found at: %d\n", pos1);

    template <typename U = T>
    requires std::is_same_v<U, char>
    int index_of_substr(const char* needle, std::size_t max_pos = SIZE_MAX) const {
        if (!mem || !needle || !*needle) return -1;

        const char* haystack = static_cast<const char*>(mem.get());
        std::size_t hay_len = std::min(std::strlen(haystack), max_pos);
        std::size_t needle_len = std::strlen(needle);

        if (needle_len > hay_len) return -1;

        for (std::size_t i = 0; i <= hay_len - needle_len; ++i) {
            if (std::memcmp(haystack + i, needle, needle_len) == 0) {
                return static_cast<int>(i);
            }
        }

        return -1;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  S T R L E N  📌📌📌

    // ps_ptr<char> text;
    // text.assign("Hello");
    // text.strlen();  // --> 5

    template <typename U = T>
    requires std::is_same_v<U, char>
    size_t strlen() const {
        if (!valid()) return 0;
        return std::strlen(get());
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  U T F 8 _ S T R L E N   📌📌📌

    // ps_ptr<char> colored; // ignoring ANSI
    // colored.assign("\033[31mПривет\033[0m", "russian red");
    // size_t visibleLetters = colored.utf8_strlen;  // --> 6

    size_t utf8_strlen() const {
        if (!get()) return 0;

        size_t count = 0;
        const unsigned char* s = reinterpret_cast<const unsigned char*>(get());

        while (*s) {
            // ANSI-Escape-Sequenz starts with ESC [
            if (*s == 0x1B && s[1] == '[') {
                s += 2;
                while (*s && !(*s >= '@' && *s <= '~')) {
                    ++s; // Parameter sign (numbers, semicola etc.)
                }
                if (*s) ++s; // Final letter z.B. 'M'
                continue;
            }

            // UTF-8-Count characters
            if ((*s & 0x80) == 0) s += 1;           // ASCII
            else if ((*s & 0xE0) == 0xC0) s += 2;   // 2 Byte
            else if ((*s & 0xF0) == 0xE0) s += 3;   // 3 Byte
            else if ((*s & 0xF8) == 0xF0) s += 4;   // 4 Byte
            else s += 1; // invalid ? carefully next

            ++count;
        }

        return count;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  U T F 8 _ V A L I D   📌📌📌

    // if (utf8_valid("Beyoncé – Déjà Vu")) {
    //     printf("Gültiges UTF-8\n");
    // } else {
    //     printf("Ungültige UTF-8-Sequenz!\n");
    // }

    bool utf8_valid(const char* s) {
        if (!s) return false;

        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(s);
        while (*bytes) {
            if (*bytes <= 0x7F) {
                // 1-byte ASCII
                bytes++;
            } else if ((*bytes >> 5) == 0x6) {
                // 2-byte sequence: 110xxxxx 10xxxxxx
                if ((bytes[1] & 0xC0) != 0x80) return false;
                if (*bytes < 0xC2) return false; // overlong encoding
                bytes += 2;
            } else if ((*bytes >> 4) == 0xE) {
                // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
                if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80) return false;

                unsigned char first = *bytes;
                if (first == 0xE0 && bytes[1] < 0xA0) return false; // overlong
                if (first == 0xED && bytes[1] >= 0xA0) return false; // UTF-16 surrogate

                bytes += 3;
            } else if ((*bytes >> 3) == 0x1E) {
                // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
                if ((bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80 || (bytes[3] & 0xC0) != 0x80) return false;

                unsigned char first = *bytes;
                if (first > 0xF4) return false; // > U+10FFFF
                if (first == 0xF0 && bytes[1] < 0x90) return false; // overlong
                if (first == 0xF4 && bytes[1] >= 0x90) return false; // out of range

                bytes += 4;
            } else {
                // Invalid first byte
                return false;
            }
        }

        return true;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  I S _ J S O N    📌📌📌

    // ps_ptr<char>jsonIn;
    // jsonIn.assign("{\"status\":1,\"message\":\"Ok\",\"result\":\"Ok\",\"errorCode\":0}");

    bool isJson() const {
        if (!mem || allocated_size < 2) return false;

        const char* p = static_cast<const char*>(mem.get());

        // Skip leading whitespace
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

        // Check if it starts with '{'
        if (*p != '{') return false;

        // Skip to end, ignoring trailing whitespace
        const char* end = mem.get() + std::strlen(mem.get()) - 1;
        while (end > p && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;

        // Check if it ends with '}'
        if (*end != '}') return false;

        // Basic JSON structure validation
        bool in_string = false;
        int brace_count = 0;
        const char* curr = p;

        while (curr <= end) {
            if (!in_string) {
                if (*curr == '{') brace_count++;
                else if (*curr == '}') brace_count--;
                else if (*curr == '"') in_string = true;
                else if (*curr == ':' || *curr == ',') {
                    // Allow colons and commas outside strings
                } else if (*curr != ' ' && *curr != '\t' && *curr != '\n' && *curr != '\r') {
                    // Allow digits, true, false, null, etc., but for simplicity, we just check for valid chars
                    if (!(*curr >= '0' && *curr <= '9') && *curr != '-' && *curr != '.' &&
                        *curr != 't' && *curr != 'f' && *curr != 'n' && *curr != '[' && *curr != ']') {
                        return false; // Invalid character outside string
                    }
                }
            } else {
                if (*curr == '"') in_string = false;
                else if (*curr == '\\') curr++; // Skip escaped character
            }
            curr++;

            if (brace_count < 0) return false; // Unmatched closing brace
        }

        return brace_count == 0; // Ensure all braces are matched
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  U N I C O D E _ T O _ U T F 8   📌📌📌
    // ps_ptr<char> jsonIn, decoded;
    // jsonIn.assign("\\u041f\\u0440\\u0438\\u0432\\u0435\\u0442"); // Привет
    // decoded.unicodeToUTF8(jsonIn.get());
    // log_i("%s", decoded.get()); // shows: Привет

void unicodeToUTF8(const char* src) {
    this->clear();
    if (!src) return;

    auto encodeCodepointToUTF8 = [](uint32_t cp, char* out) -> int {
        if (cp <= 0x7F) {
            out[0] = cp;
            return 1;
        } else if (cp <= 0x7FF) {
            out[0] = 0xC0 | (cp >> 6);
            out[1] = 0x80 | (cp & 0x3F);
            return 2;
        } else if (cp <= 0xFFFF) {
            out[0] = 0xE0 | (cp >> 12);
            out[1] = 0x80 | ((cp >> 6) & 0x3F);
            out[2] = 0x80 | (cp & 0x3F);
            return 3;
        } else if (cp <= 0x10FFFF) {
            out[0] = 0xF0 | (cp >> 18);
            out[1] = 0x80 | ((cp >> 12) & 0x3F);
            out[2] = 0x80 | ((cp >> 6) & 0x3F);
            out[3] = 0x80 | (cp & 0x3F);
            return 4;
        }
        return 0;
    };

    const char* ptr = src;
    char utf8[5];

    while (*ptr) {
        if (ptr[0] == '\\' && ptr[1] == 'u') {
            uint32_t codepoint = 0;
            if (sscanf(ptr + 2, "%4lx", &codepoint) == 1) {
                int len = encodeCodepointToUTF8(codepoint, utf8);
                utf8[len] = 0;
                this->append(utf8);
                ptr += 6; // überspringe \uXXXX
            } else {
                this->append(ptr, 1);
                ptr++;
            }
        } else {
            this->append(ptr, 1);
            ptr++;
        }
    }
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  G E T   📌📌📌
    T* get() const { return static_cast<T*>(mem.get()); }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C_G E T  (safe)   📌📌📌
    const char* c_get(const char* fallback = "NA") const {
        if constexpr (std::is_same_v<T, char>) {
            return mem ? mem.get() : fallback;
        } else {
            return fallback;
        }
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  S E T   📌📌📌

    // ps_ptr<uint32_t> p;
    // uint32_t* raw = (uint32_t*)malloc(100 * sizeof(uint32_t)); // create memory manually

    // for (int i = 0; i < 100; ++i) {// write data
    //     raw[i] = i * i;
    // }
    // p.set(raw, 100 * sizeof(uint32_t)); // ps_ptr takes over the pointer and remembers the size
    // for (int i = 0; i < 5; ++i) {// access with ps_ptr
    //     printf("%u ", p[i]);
    // } // later at p.reset () or automatically in the destructor `free(raw)`
    void set(T* ptr, std::size_t size = 0) {
        if (mem.get() != ptr) {
            mem.reset(ptr);
            allocated_size = size;
        }
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  O P E R A T O R   📌📌📌

    // struct MyStruct {
    //     int x;
    //     void hello() const {log_w("Hello, x = %i", x);}
    // };
    // ps_ptr<MyStruct> ptr;
    // ptr.alloc(sizeof(MyStruct));
    // ptr.clear();  // optional, setzt x auf 0
    // ptr->x = 123;         // 👈 uses operator->()
    // ptr->hello();         // 👈 uses operator->()
    // (*ptr).x = 456;       // 👈 uses operator*()
    // std::cout << (*ptr).x << "\n";
    T* operator->() const { return get(); }
    T& operator*() const { return *get(); }

    // //ps_ptr<int32_t> p;
    // //int32_t x = p[5];
    // T& operator[](size_t index) {return get()[index];}

    // // ps_ptr<ps_ptr<int32_t>> array_of_ptrs;
    // // array_of_ptrs[0].alloc(...);
    // template <typename U = T>
    // typename std::enable_if<
    //     std::is_class<U>::value &&
    //     std::is_same<decltype(std::declval<U>().alloc(0)), void>::value,
    //     U&>::type
    // operator[](std::size_t index) const {
    //     return get()[index];
    // }

    // Sicherer operator[] mit Logging
    T& operator[](std::size_t index) {
        if (index >= allocated_size) {
            log_e("ps_ptr[]: Index %zu out of bounds (size = %zu)", index, allocated_size);
            return dummy; // Access allowed, but ineffective
        }
        return mem[index];
    }

    const T& operator[](std::size_t index) const {
        if (index >= allocated_size) {
            log_e("ps_ptr[] (const): Index %zu out of bounds (size = %zu)", index, allocated_size);
            return dummy;
        }
        return mem[index];
    }


    // struct MyStruct {
    //     int value;
    // };
    // ps_ptr<MyStruct> smart_ptr;
    //
    // MyStruct* raw = (MyStruct*)malloc(sizeof(MyStruct));// Manually allocated raw memory
    // raw->value = 123;
    // smart_ptr = raw; // Smart_PTR is now taking over the property
    // std::cout << smart_ptr->value << std::endl;  // access as usual gives: 123
    ps_ptr<T>& operator=(T* raw_ptr) {
        if (mem.get() != raw_ptr) {
            mem.reset(raw_ptr);
            allocated_size = 0; // (raw_ptr != nullptr) ? /* Berechne Größe hier */ : 0;
        }
        return *this;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌   A T    📌📌📌

    // Special method for ps_ptr<ps_ptr<T>>
    template <typename U = T>
    auto at(size_t index) -> typename std::enable_if<
        std::is_same<U, ps_ptr<typename U::element_type>>::value,
        ps_ptr<typename U::element_type>&
    >::type {
        return static_cast<ps_ptr<typename U::element_type>*>(get())[index];
    }
    using element_type = T;
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  A S  📌📌📌

    // ps_ptr<void> generic;
    // generic.alloc(64);
    // uint32_t* p = generic.as<uint32_t>();

    template<typename U>
    U* as() const {
        return reinterpret_cast<U*>(get());
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  I N S E R T  📌📌📌

    // ps_ptr<char> audioPath;
    // audioPath.assign("mp3files/test.mp3", "audioPath");
    // audioPath.insert("/", 0);  // result: "/mp3files/test.mp3"
    //
    // audioPath.insert("local/", 1);  // result: "/local/mp3files/test.mp3"

    bool insert(const char* insertStr, std::size_t pos) {
        if (!insertStr || !valid()) return false;

        std::size_t originalLen = std::strlen(get());
        std::size_t insertLen = std::strlen(insertStr);

        // Position outside the valid area?Then add to the end
        if (pos > originalLen) pos = originalLen;

        // New total length + 1 for '\ 0'
        std::size_t newLen = originalLen + insertLen + 1;
        ps_ptr<char> temp;
        temp.alloc(newLen);
        if (!temp.valid()) return false;

        char* dst = temp.get();

        // Copy up to the insertion position
        std::memcpy(dst, get(), pos);
        // Add new content
        std::memcpy(dst + pos, insertStr, insertLen);
        // Copy the rest of the original
        std::memcpy(dst + pos + insertLen, get() + pos, originalLen - pos + 1); // +1 für \0

        // Take over new content
        this->assign(temp.get());
        return true;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  S H R I N K _ T O _ F I T  📌📌📌

    // Only for Char: Put the buffer on the actual length +1 (for \ 0)
    template <typename U = T>
    requires std::is_same_v<U, char>
    void shrink_to_fit() {
        if (!mem) return;
        std::size_t len = std::strlen(get());
        if (len == 0) return;
        ps_ptr<char> temp;
        temp.alloc(len + 1);
        if (!temp.valid()) return;
        std::memcpy(temp.get(), get(), len + 1); // inklusive \0
        this->assign(temp.get());
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  T O _ U I N T 6 4  📌📌📌
    // Retrieves the numeric value (uint64_t) from the stored string, parsing it as a number in the specified base (default: 16 for hexadecimal).
    // Example: If the stored string is "0x12345678", returns 0x12345678.
    // If the string is empty, null, or invalid, returns 0 and logs an error.

    // ps_ptr<char> mediaSeqStr = "227213779";
    // uint64_t mediaSeq = mediaSeqStr.to_uint64(10);
    //
    // ps_ptr<char> addr = "0x1A3B";
    // uint64_t val = addr.to_uint64(16);

    template <typename U = T>
    requires std::is_same_v<U, char>
    uint64_t to_uint64(int base = 10) const {
        static_assert(std::is_same_v<T, char>, "to_uint64 is only valid for ps_ptr<char>");
        if (!mem || !get()) {
            log_e("to_uint64: No valid string data");
            return 0;
        }
        const char* str = get();
        char* end = nullptr;
        uint64_t result = std::strtoull(str, &end, base);
        if (end == str || *end != '\0') {
            log_e("to_uint64: Invalid numeric value in '%s' for base %d", str, base);
            return 0;
        }
        return result;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  T O _ U I N T 3 2  📌📌📌
    // Retrieves the numeric value (uint32_t) from the stored string, parsing it as a number in the specified base (default: 10 for decimal).
    // Example: If the stored string is "0x1A3B", returns 0x1A3B (6715) for base 16.
    // If the string is empty, null, invalid, or exceeds UINT32_MAX (4294967295), returns 0 and logs an error.
    // Usage:
    //   ps_ptr<char> size = "227213779"; uint32_t val = size.to_uint32(10); // Returns 227213779
    //   ps_ptr<char> addr = "0x1A3B"; uint32_t val = addr.to_uint32(16); // Returns 6715
    template <typename U = T>
    requires std::is_same_v<U, char>
    uint32_t to_uint32(int base = 10) const {
        static_assert(std::is_same_v<T, char>, "to_uint32 is only valid for ps_ptr<char>");
        if (!mem || !get()) {
            log_e("to_uint32: No valid string data");
            return 0;
        }
        const char* str = get();
        char* end = nullptr;
        unsigned long result = std::strtoul(str, &end, base);
        if (end == str || *end != '\0') {
            log_e("to_uint32: Invalid numeric value in '%s' for base %d", str, base);
            return 0;
        }
        if (result > UINT32_MAX) {
            log_e("to_uint32: Value in '%s' exceeds UINT32_MAX (%u) for base %d", str, UINT32_MAX, base);
            return 0;
        }
        return static_cast<uint32_t>(result);
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————







// 📌📌📌  B I G _ E N D I A N  📌📌📌
    // Reads up to 8 bytes from a uint8_t array in big-endian order and stores the value as a hexadecimal string (e.g., "0x12345678").
    // Example: uint8_t data[] = {0x12, 0x34, 0x56, 0x78}; → stores "0x12345678"
    // If size > 8, only the first 8 bytes are processed. If size = 0 or data = nullptr, stores "0x0".
    template <typename U = T>
    requires std::is_same_v<U, char>
    void big_endian(const uint8_t* data, uint8_t size) {
        static_assert(std::is_same_v<T, char>, "big_endian is only valid for ps_ptr<char>");
        if (!data || size == 0) {
            log_e("big_endian: Invalid input (data is null or size is 0)");
            assign("0x0");
            return;
        }
        if (size > 8) {
            log_e("big_endian: Size %u exceeds 8 bytes, truncating to 8", size);
            size = 8;
        }
        uint64_t result = 0;
        for (uint8_t i = 0; i < size; ++i) {
            result = (result << 8) | data[i];
        }
        char buffer[19]; // Max: "0x" + 16 chars for uint64_t + null terminator
        snprintf(buffer, sizeof(buffer), "0x%llx", result);
        assign(buffer);
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  L I T T L E _ E N D I A N  📌📌📌
    // Reads up to 8 bytes from a uint8_t array in little-endian order and stores the value as a hexadecimal string (e.g., "0x12345678").
    // Example: uint8_t data[] = {0x78, 0x56, 0x34, 0x12}; → stores "0x12345678"
    // If size > 8, only the first 8 bytes are processed. If size = 0 or data = nullptr, stores "0x0".
    template <typename U = T>
    requires std::is_same_v<U, char>
    void little_endian(const uint8_t* data, uint8_t size) {
        static_assert(std::is_same_v<T, char>, "little_endian is only valid for ps_ptr<char>");
        if (!data || size == 0) {
            log_e("little_endian: Invalid input (data is null or size is 0)");
            assign("0x0");
            return;
        }
        if (size > 8) {
            log_e("little_endian: Size %u exceeds 8 bytes, truncating to 8", size);
            size = 8;
        }
        uint64_t result = 0;
        for (int i = size - 1; i >= 0; --i) {
            result = (result << 8) | data[i];
        }
        char buffer[19]; // Max: "0x" + 16 chars for uint64_t + null terminator
        snprintf(buffer, sizeof(buffer), "0x%llx", result);
        assign(buffer);
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  R E M O V E _ C H A R S   📌📌📌

    // ps_ptr<char> txt;
    // txt.copy_from("[00:12.45]", "time");
    // txt.remove_chars("[]:.");   // → 001245

    void remove_chars(const char* chars) {
        if (!valid() || !chars) return;
        char* dst = get();
        char* src = get();

        while (*src) {
            if (!std::strchr(chars, *src)) {
                *dst++ = *src;
            }
            ++src;
        }
        *dst = '\0';
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  R E P L A C E   📌📌📌

    // ps_ptr<char> path;
    // path.assign("/user/temp/file.tmp");
    // path.replace("temp", "music");
    // result: "/user/music/file.tmp"

    bool replace(const char* from, const char* to) {
        if (!valid() || !from || !*from || !to) return false;

        const char* src = get();
        std::size_t fromLen = std::strlen(from);
        std::size_t toLen = std::strlen(to);

        if (fromLen == 0) return false;  // Nichts zu ersetzen

        std::vector<char> result;
        const char* read = src;

        while (*read) {
            if (std::strncmp(read, from, fromLen) == 0) {
                // Match gefunden
                result.insert(result.end(), to, to + toLen);
                read += fromLen;
            } else {
                result.push_back(*read++);
            }
        }

        result.push_back('\0');

        // Kopiere Ergebnis zurück
        this->copy_from(result.data(), result.size() - 1);
        return true;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  T R I M   📌📌📌

    // ps_ptr<char> text3;
    // text3.assign("  Hello, World!  ");
    // text3.trim();  // → "Hello, World!"

    template <typename U = T>
    requires std::is_same_v<U, char>
    void trim() {
        if (!mem) return;

        char* str = static_cast<char*>(mem.get());
        std::size_t len = std::strlen(str);
        if (len == 0) return;

        // trim on the left
        char* start = str;
        while (*start && isspace(*start)) ++start;

        // trim on the right
        char* end = str + len - 1;
        while (end >= start && isspace(*end)) --end;
        *(end + 1) = '\0';

        // If not at the beginning, copy everything forward
        if (start != str) {
            std::size_t new_len = end - start + 1;
            std::memmove(str, start, new_len + 1);
        }
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  T R U N C A T E _ A T   📌📌📌

    // ps_ptr<char>t;
    // t.assign("Hello, World");
    // t.truncate_at(',');  --> "Hello"

    // ps_ptr<char>t;
    // t.assign("Hello, World");
    // t.truncate_at('5');  --> "Hello"

    void truncate_at(char ch) {
        if (!valid()) return;
        char* str = get();
        char* pos = strchr(str, ch);
        if (pos) *pos = '\0';
    }

    void truncate_at(std::size_t pos) {
        if (!valid()) return;
        if(pos >= strlen()) return;
        char* str = get();
        std::size_t len = std::strlen(str);
        if (pos < len) str[pos] = '\0';
        else log_e("truncate pos %i out of length %i", pos, len);
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  R E M O V E _ P R E F I X   📌📌📌
    // ps_ptr<char> t;
    // t.assign("../../2093120-b/RISMI/stream01/streamPlaylist.m3u8");
    // t.remove_prefix("../../");  // Result: "2093120-b/RISMI/stream01/streamPlaylist.m3u8"

    void remove_prefix(const char* prefix) {
        if (!valid() || !prefix) return;

        std::size_t prefix_len = std::strlen(prefix);
        if (std::strncmp(get(), prefix, prefix_len) == 0) {
            char* str = get();
            std::size_t len = std::strlen(str);
            std::memmove(str, str + prefix_len, len - prefix_len + 1); // inkl. '\0'
        }
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  R E M O V E _ B E F O R E   📌📌📌
    // t.assign("../../2093120-b/RISMI/stream01/streamPlaylist.m3u8");
    // t.remove_before('/');  // removed until the first '/'
    // Result: "../2093120-b/RISMI/stream01/streamPlaylist.m3u8"

    void remove_before(char ch, bool includeChar = true) {
        if (!valid()) return;

        char* str = get();
        char* pos = strchr(str, ch);
        if (pos) {
            if (!includeChar) ++pos;  // wenn nicht inklusive: das Zeichen behalten
            std::size_t remaining = std::strlen(pos);
            std::memmove(str, pos, remaining + 1); // inkl. '\0'
        }
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  R E M O V E _ B E F O R E   ( B Y   I N D E X )  📌📌📌
    // t.assign("HelloWorld");
    // t.remove_before(5);         // entfernt "Hello"
    // t.remove_before(5, false);  // entfernt nur "Hell", das Zeichen an idx bleibt

    void remove_before(int idx, bool includeIdx = true) {
        if (!valid()) return;

        char* str = get();
        std::size_t len = std::strlen(str);

        if (idx < 0 || static_cast<std::size_t>(idx) > len) return;

        char* pos = str + idx;
        if (!includeIdx && idx < len) ++pos; // wenn das Zeichen nicht entfernt werden soll

        std::size_t remaining = std::strlen(pos);
        std::memmove(str, pos, remaining + 1); // inkl. '\0'
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  S H I F T _ L E F T   📌📌📌
    // Show the contents of the buffer around n bytes to the left and fill the rest with zeros
    // Consider UTF-16 data and the size of the allocated memory

    void shift_left(int n) {
        if (!mem || allocated_size == 0) {
            printf("Error: No allocated memory or invalid buffer\n");
            return;
        }

        if (n < 0 || static_cast<std::size_t>(n) > allocated_size) {
            printf("Error: Invalid shift amount %d, allocated_size=%zu\n", n, allocated_size);
            return;
        }

        // if (n % 2 != 0) {
        //     printf("Warning: Shift amount %d is not even, adjusting to %d for UTF-16 alignment\n", n, n + 1);
        //     n += 1; // make sure n is even for UTF-16
        // }

        char* str = mem.get();
        if (n == 0) return;

        // show the buffer around n bytes to the left
        std::size_t remaining = allocated_size - n;
        std::memmove(str, str + n, remaining);

        // fill the rest of the memory with zeros
        std::memset(str + remaining, 0, n);
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C O N T A I N S  📌📌📌

    // if (t.contains("xyz")) {
    // "xyz" occurs in t
    // }

    bool contains(const char* substr) const {
        return substr && this->valid() && std::strstr(this->get(), substr);
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C O N T A I N S _ W I T H _ I C A S E    📌📌📌
    bool contains_with_icase(const char* substr) const {
        if (!substr || !this->valid()) {
            return false;
        }

        const char* haystack = this->get();
        const char* needle = substr;

        // Wir kopieren den Text in temporäre lowercase-Strings
        std::string haystack_lower(haystack);
        std::string needle_lower(needle);

        std::transform(haystack_lower.begin(), haystack_lower.end(), haystack_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        std::transform(needle_lower.begin(), needle_lower.end(), needle_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        return haystack_lower.find(needle_lower) != std::string::npos;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C L E A R   📌📌📌

    void clear() {
        if (mem && allocated_size > 0) {
            std::memset(mem.get(), 0, allocated_size);
        }
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  S I Z E   📌📌📌

    size_t size() const { return allocated_size; }

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  V A L I D   📌📌📌

    bool valid() const { return mem != nullptr; }

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  R E S E T   📌📌📌

    void reset() {
        mem.reset();
        allocated_size = 0;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  H E X _ D U M P  📌📌📌
    // ps_ptr<char>buff;
    // buff.calloc(10);
    // buff.assign("eng\n");
    // buff.hex_dump(6);
    //
    // 0x65 0x6E 0x67 0x0A 0x00 0x00
    // e    n    g    LF   NUL  NUL

    void hex_dump(uint16_t n = UINT16_MAX) {
        if (!valid()) { printf("hex_dump: invalid buffer\n"); return; }
        if (allocated_size < n) n = allocated_size;
        if (n == 0) {
            printf("hex_dump: no data\n");
            return;
        }
        uint8_t items_per_line = 30;

        char sym[32][5] = {
            "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL", "BS ", "TAB",
            "LF ", "VT ", "FF ", "CR ", "SO ", "SI ", "DLE", "DC1", "DC2", "DC3",
            "DC4", "NAK", "SYN", "ETB", "CAN", "EM ", "SUB", "ESC", "FS ", "GS ",
            "RS ", "US "
        };
        char* buff = get();
        int i = 0;
        printf("Dumping %u bytes:\n", n);
        while (i < n) {
            int m = std::min(static_cast<int>(n), i + items_per_line); // max items_per_line, but not > n
            int s = 0;
            for (int j = i; j < m; j++) {
                printf("0x%02X ", static_cast<unsigned char>(buff[j]));
                s++;
                if(s % 10 == 0) printf("   ");
            }
            printf("\n");
            s = 0;
            for (int j = i; j < m; j++) {
                if (buff[j] >= 32) {
                    printf("%c    ", buff[j]);
                } else {
                    printf("%s  ", sym[static_cast<unsigned char>(buff[j])]);
                }
                s++;
                if(s % 10 == 0) printf("   ");
            }
            printf("\n");
            i += items_per_line;
        }
        printf("\n");
    }
};
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  S T R U C T U R E S    📌📌📌
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————


    // typedef struct _hwoe{
    //     bool ssl;
    //     ps_ptr<char> hwoe;  // host without extension
    //     uint16_t     port;
    //     ps_ptr<char>extension[100];
    //     ps_ptr<char> query_string;
    // } hwoe_t;
    // PS_STRUCT_FREE_MEMBERS(hwoe_t,
    //     ptr->hwoe.reset();
    //     for(int i = 0; i< 100; i++)ptr->extension[i].reset();
    //     ptr->query_string.reset();
    // )

    // ps_struct_ptr<hwoe_t> result;
    // result.alloc();
    // result->extension.copy_from(path
    // .....

template <typename T>
class ps_struct_ptr {
    ps_ptr<T> inner;

public:
    ps_struct_ptr() = default;

    void alloc(const char* name = nullptr) {
        inner.alloc(sizeof(T), name);
        if (inner.valid()) inner.clear();
    }

    T* get() const { return inner.get(); }
    T* operator->() const { return get(); }
    T& operator*() const { return *inner; }

    void set_ptr_field(char** field, const char* text) {
        if (!valid() || !field) return;
        if (*field) {
            free(*field);
            *field = nullptr;
        }
        if (text) {
            std::size_t len = strlen(text) + 1;

            char* p = nullptr;
            if (psramFound()) { // Check at the runtime whether PSRAM is available
                p = static_cast<char*>(ps_malloc(len));
            }
            else{
                p = static_cast<char*>(malloc(len));
            }
            if (p) {
                memcpy(p, text, len);
                *field = p;
            }
        }
    }

    bool valid() const { return inner.valid(); }
    std::size_t size() const { return inner.size(); }
    void clear() { inner.clear(); }

    void reset() {
        if (inner.valid()) {
            free_all_ptr_members();
        }
        inner.reset();
    }

    ~ps_struct_ptr() {
        reset();
    }

    // Explicitly define the Move constructor
    ps_struct_ptr(ps_struct_ptr&& other) noexcept
        : inner(std::move(other.inner)) { // move the 'inner' ps_ptr-Member
        // Optional: A logging entry could be here to pursue relocation processes
        // log_w("ps_struct_ptr Move Constructor called for type %s", typeid(T).name());
    }

    // Explicitly define the Move Assignment operator
    ps_struct_ptr& operator=(ps_struct_ptr&& other) noexcept {
        if (this != &other) { // Prevent self -allocation
            // Optional: A logging entry could be here
            // log_w("ps_struct_ptr Move Assignment called for type %s", typeid(T).name());
            inner = std::move(other.inner); // move the 'inner' ps_ptr-Member
        }
        return *this;
    }

    // Explicitly delete copy constructor and copying assignment operator
    // This is good practice to force the "Move-Only" semantics
    ps_struct_ptr(const ps_struct_ptr&) = delete;
    ps_struct_ptr& operator=(const ps_struct_ptr&) = delete;

protected:
    // Specializes via macro ps_struct_free_memembers
    inline void free_all_ptr_members() {}
};



// ——————————————————————————————————————————————————————————————
// Macro for the declaration of releasing fields for a structure

#define PS_STRUCT_FREE_MEMBERS(TYPE, ...) \
    template <> \
    inline void ps_struct_ptr<TYPE>::free_all_ptr_members() { \
        auto ptr = this->get(); /* 'ptr' ist hier ein Pointer auf die Struktur TYPE */ \
        if (ptr) { /* Sicherheitsprüfung, um sicherzustellen, dass ptr gültig ist */ \
            __VA_ARGS__ \
        } \
    }

inline void free_field(char*& field) {
    if (field) {
        free(field);
        field = nullptr;
    }
}

// Variadic Auxiliary function
template <typename... Args>
inline void free_fields(Args&... fields) {
    (free_field(fields), ...);
}


// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌   2 D  A R R A Y   📌📌📌
//
// 2D Array in PSRAM with Bounds-Check, log_e and reset()
//
// ps_array2d<int32_t> s_samples; // standard constructor
// s_samples.alloc(2, 1152);      // mem alloc for [2][1152]
//
// int ch = 0; // exanole: channel 0
//
// declaration of PCM1 as a pointer on the first element of the line (similar to samples [CH])
// int32_t* pcm1;
//
// pcm1 = s_samples.get_raw_row_ptr(ch);
//
// use 'pcm1' now, as if it were an int32_t array of size 1152
// pcm1[0] = 42;
// pcm1[1151] = 99;
//
// s_samples.reset();
//

template <typename T>
class ps_array2d
{
public:
    // standard constructor
    ps_array2d() : m_dim1(0), m_dim2(0) {}

    // original constructor
    ps_array2d(size_t dim1, size_t dim2)
        : m_dim1(dim1), m_dim2(dim2)
    {
        alloc_internal(dim1, dim2);
    }

    // alloc-method
    bool alloc(size_t dim1, size_t dim2, const char* name = nullptr, bool usePSRAM = true) {
        reset();
        m_dim1 = dim1;
        m_dim2 = dim2;
        return alloc_internal(dim1, dim2, name, usePSRAM);
    }

    // overload for `alloc` without dimensions if the dimensions are already set
    bool alloc() {
        if (m_dim1 == 0 || m_dim2 == 0) {
            log_e("ps_array2d::alloc() called without dimensions or dimensions are zero.");
            return false;
        }
        reset();
        return alloc_internal(m_dim1, m_dim2);
    }

    void clear() {
        if (!is_allocated()) {
            // nothing to do if there is no memory assigned
            return;
        }
        // calculate the total number of elements
        const size_t total_elements = m_dim1 * m_dim2;
        // set the entire memory block to zero
        std::memset(m_data.get(), 0, total_elements * sizeof(T));
    }

    // brace operator for access to individual elements (Bounds check included)
    T &operator()(size_t i, size_t j)
    {
        if (!m_data.valid()) {
            log_e("ps_array2d: Access on unallocated memory. Call alloc() first. [line %d]", __LINE__);
            return m_dummy;
        }
        if (i >= m_dim1 || j >= m_dim2) {
            log_e("ps_array2d out-of-bounds: (%u,%u) but dims=(%u,%u) [line %d]",
                  (unsigned)i, (unsigned)j, (unsigned)m_dim1, (unsigned)m_dim2, __LINE__);
            return m_dummy;
        }
        return m_data[i * m_dim2 + j];
    }

    const T &operator()(size_t i, size_t j) const
    {
        if (!m_data.valid()) {
            log_e("ps_array2d: Access on unallocated memory. Call alloc() first. [line %d]", __LINE__);
            return m_dummy;
        }
        if (i >= m_dim1 || j >= m_dim2) {
            log_e("ps_array2d out-of-bounds: (%u,%u) but dims=(%u,%u) [line %d]",
                  (unsigned)i, (unsigned)j, (unsigned)m_dim1, (unsigned)m_dim2, __LINE__);
            return m_dummy;
        }
        return m_data[i * m_dim2 + j];
    }

    // gives a raw pointer  (T*) to the beginning of a specific line (row_index) back.
    T* get_raw_row_ptr(size_t row_index) {
        if (!m_data.valid()) {
            log_e("ps_array2d::get_raw_row_ptr: Access on unallocated memory. Call alloc() first. [line %d]", __LINE__);
            return nullptr;
        }
        if (row_index >= m_dim1) {
            log_e("ps_array2d::get_raw_row_ptr: row_index %u out-of-bounds (%u). [line %d]",
                  (unsigned)row_index, (unsigned)m_dim1, __LINE__);
            return nullptr;
        }
        return m_data.get() + row_index * m_dim2;
    }

    const T* get_raw_row_ptr(size_t row_index) const {
        if (!m_data.valid()) {
            log_e("ps_array2d::get_raw_row_ptr: Access on unallocated memory. Call alloc() first. [line %d]", __LINE__);
            return nullptr;
        }
        if (row_index >= m_dim1) {
            log_e("ps_array2d::get_raw_row_ptr: row_index %u out-of-bounds (%u). [line %d]",
                  (unsigned)row_index, (unsigned)m_dim1, __LINE__);
            return nullptr;
        }
        return m_data.get() + row_index * m_dim2;
    }

    // gives back true when the memory has been successfully allocated
    bool is_allocated() const { return m_data.valid(); }

    size_t dim1() const { return m_dim1; }
    size_t dim2() const { return m_dim2; }

    // manual release of the memory
    void reset() {
        m_data.reset();
        m_dim1 = 0;
        m_dim2 = 0;
    }

private:
    size_t m_dim1, m_dim2;
    ps_ptr<T> m_data; // manages the linear memory block
    static inline T m_dummy{};  // dummy-value for errors

    // private auxiliary method for the actual allocation logic
    bool alloc_internal(size_t dim1, size_t dim2, const char* name = nullptr, bool usePSRAM = true) {
        if (dim1 == 0 || dim2 == 0) {
            log_e("ps_array2d: Cannot allocate with zero dimensions. [line %d]", __LINE__);
            return false;
        }
        size_t total_elements = dim1 * dim2;
        m_data.calloc(total_elements, name, usePSRAM);
        if (!m_data.valid()) {
            log_e("ps_array2d: Memory allocation failed for %u elements. [line %d]", (unsigned)total_elements, __LINE__);
            m_dim1 = 0; m_dim2 = 0;
            return false;
        }
        return true;
    }
};

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// psram_unique_ptr.hpp (Auszug mit ps_array3d)
//
// 📌📌📌   3 D  A R R A Y   📌📌📌
//
// 3D Array in PSRAM with Bounds-Check, log_e and reset()
//
// ps_array3d<int32_t> s_sbsample;
// s_sbsample.alloc(2, 36, 32);
//
// int ch = 0; // example of the first dimension
// int gr = 0; // example of the group index
//
// C- pointer to a 1D-Array [32]
// int32_t (*sample)[32];
//
// // allocation using the new method get_raw_row_ptr and the appropriate reinterpret_cast
// sample = reinterpret_cast<int32_t(*)[32]>(s_sbsample.get_raw_row_ptr(ch, 18 * gr));
//
// // use 'sample' now:
// (*sample)[0] = 10;
//
// s_sbsample.reset();
//


template <typename T>
class ps_array3d{
public:
    // standard constructor
    ps_array3d() : m_dim1(0), m_dim2(0), m_dim3(0) {}

    // original constructor
    ps_array3d(size_t dim1, size_t dim2, size_t dim3)
        : m_dim1(dim1), m_dim2(dim2), m_dim3(dim3)
    {
        alloc_internal(dim1, dim2, dim3);
    }

    // alloc-method
    bool alloc(size_t dim1, size_t dim2, size_t dim3, const char* name = nullptr, bool usePSRAM = true) {
        reset();
        m_dim1 = dim1;
        m_dim2 = dim2;
        m_dim3 = dim3;
        return alloc_internal(dim1, dim2, dim3, name, usePSRAM);
    }

    // overload for `alloc` without dimensions
    bool alloc() {
        if (m_dim1 == 0 || m_dim2 == 0 || m_dim3 == 0) {
            log_e("ps_array3d::alloc() called without dimensions or dimensions are zero.");
            return false;
        }
        reset();
        return alloc_internal(m_dim1, m_dim2, m_dim3);
    }

    void clear(){
        if (!is_allocated()) {
            // nothing to do if there is no memory assigned
            return;
        }
        // calculate the total number of elements
        const size_t total_elements = m_dim1 * m_dim2 * m_dim3;
        // Setze den gesamten Speicherblock auf null
        std::memset(m_data.get(), 0, total_elements * sizeof(T));
    }

    // brace operator for access to individual elements
    T &operator()(size_t i, size_t j, size_t k)
    {
        if (!m_data.valid()) {
            log_e("ps_array3d: Access on unallocated memory. Call alloc() first. [line %d]", __LINE__);
            return m_dummy;
        }
        if (i >= m_dim1 || j >= m_dim2 || k >= m_dim3) {
            log_e("ps_array3d out-of-bounds: (%u,%u,%u) but dims=(%u,%u,%u) [line %d]",
                  (unsigned)i, (unsigned)j, (unsigned)k,
                  (unsigned)m_dim1, (unsigned)m_dim2, (unsigned)m_dim3, __LINE__);
            return m_dummy;
        }
        return m_data[i * (m_dim2 * m_dim3) + j * m_dim3 + k];
    }

    const T &operator()(size_t i, size_t j, size_t k) const
    {
        if (!m_data.valid()) {
            log_e("ps_array3d: Access on unallocated memory. Call alloc() first. [line %d]", __LINE__);
            return m_dummy;
        }
        if (i >= m_dim1 || j >= m_dim2 || k >= m_dim3) {
            log_e("ps_array3d out-of-bounds: (%u,%u,%u) but dims=(%u,%u,%u) [line %d]",
                  (unsigned)i, (unsigned)j, (unsigned)k,
                  (unsigned)m_dim1, (unsigned)m_dim2, (unsigned)m_dim3, __LINE__);
            return m_dummy;
        }
        return m_data[i * (m_dim2 * m_dim3) + j * m_dim3 + k];
    }

    // method for retrieving a raw pointer at the beginning of a 2D slice.
    T* get_raw_slice_ptr(size_t i) {
        if (!m_data.valid()) {
            log_e("ps_array3d::get_raw_slice_ptr: Access on unallocated memory. Call alloc() first. [line %d]", __LINE__);
            return nullptr;
        }
        if (i >= m_dim1) {
            log_e("ps_array3d::get_raw_slice_ptr: dim1 index %u out-of-bounds (%u). [line %d]", (unsigned)i, (unsigned)m_dim1, __LINE__);
            return nullptr;
        }
        return m_data.get() + i * (m_dim2 * m_dim3);
    }

    const T* get_raw_slice_ptr(size_t i) const {
        if (!m_data.valid()) {
            log_e("ps_array3d::get_raw_slice_ptr: Access on unallocated memory. Call alloc() first. [line %d]", __LINE__);
            return nullptr;
        }
        if (i >= m_dim1) {
            log_e("ps_array3d::get_raw_slice_ptr: dim1 index %u out-of-bounds (%u). [line %d]", (unsigned)i, (unsigned)m_dim1, __LINE__);
            return nullptr;
        }
        return m_data.get() + i * (m_dim2 * m_dim3);
    }

    // gives a raw pointer (T*) to the beginning of a specific line (dim2_idx) back.
    // return within a specific slice (DIM1_IDX).
    T* get_raw_row_ptr(size_t dim1_idx, size_t dim2_idx) {
        if (!m_data.valid()) {
            log_e("ps_array3d::get_raw_row_ptr: Access on unallocated memory. Call alloc() first. [line %d]", __LINE__);
            return nullptr;
        }
        if (dim1_idx >= m_dim1 || dim2_idx >= m_dim2) {
            log_e("ps_array3d::get_raw_row_ptr: Indices (%u,%u) out-of-bounds for dims=(%u,%u) [line %d]",
                  (unsigned)dim1_idx, (unsigned)dim2_idx, (unsigned)m_dim1, (unsigned)m_dim2, __LINE__);
            return nullptr;
        }
        // calculate the offset for the specific line in the linear memory
        return m_data.get() + (dim1_idx * (m_dim2 * m_dim3)) + (dim2_idx * m_dim3);
    }

    const T* get_raw_row_ptr(size_t dim1_idx, size_t dim2_idx) const {
        if (!m_data.valid()) {
            log_e("ps_array3d::get_raw_row_ptr: Access on unallocated memory. Call alloc() first. [line %d]", __LINE__);
            return nullptr;
        }
        if (dim1_idx >= m_dim1 || dim2_idx >= m_dim2) {
            log_e("ps_array3d::get_raw_row_ptr: Indices (%u,%u) out-of-bounds for dims=(%u,%u) [line %d]",
                  (unsigned)dim1_idx, (unsigned)dim2_idx, (unsigned)m_dim1, (unsigned)m_dim2, __LINE__);
            return nullptr;
        }
        return m_data.get() + (dim1_idx * (m_dim2 * m_dim3)) + (dim2_idx * m_dim3);
    }


    // gives back true when the memory has been successfully allocated
    bool is_allocated() const { return m_data.valid(); }

    size_t dim1() const { return m_dim1; }
    size_t dim2() const { return m_dim2; }
    size_t dim3() const { return m_dim3; }

    // manual release of the memory
    void reset() {
        m_data.reset();
        m_dim1 = 0;
        m_dim2 = 0;
        m_dim3 = 0;
    }

private:
    size_t m_dim1, m_dim2, m_dim3;
    ps_ptr<T> m_data; // manages the linear memory block
    static inline T m_dummy{}; // dummy-value for errors

    // private auxiliary method for the actual allocation logic
    bool alloc_internal(size_t dim1, size_t dim2, size_t dim3, const char* name = nullptr, bool usePSRAM = true) {
        if (dim1 == 0 || dim2 == 0 || dim3 == 0) {
            log_e("ps_array3d: Cannot allocate with zero dimensions. [line %d]", __LINE__);
            return false;
        }
        size_t total_elements = dim1 * dim2 * dim3;
        m_data.calloc(total_elements, name, usePSRAM);
        if (!m_data.valid()) {
            log_e("ps_array3d: Memory allocation failed for %u elements. [line %d]", (unsigned)total_elements, __LINE__);
            m_dim1 = 0; m_dim2 = 0; m_dim3 = 0;
            return false;
        }
        return true;
    }
};

#endif