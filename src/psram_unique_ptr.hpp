#pragma once

#include "Arduino.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

#ifndef PS_PTR_CLASS
    #define PS_PTR_CLASS 1

/** Auxiliary functions for using Unique Pointers in ESP32 PSRAM **/

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// PSRAM Deleter (ps_malloc → free)
// Deleter for PSRAM

// single objekt (int -> PSRAM)
// auto pint = ps_make_unique<int>(123);
// Serial.printf("Wert: %d\n", *pint);

// single objekt (std::string -> PSRAM)
// auto pstr = ps_make_unique<std::string>("Hallo PSRAM");
// Serial.printf("String: %s\n", pstr->c_str());

// array (char-Buffer)
// auto buf = ps_make_unique<char>(256);
// std::strcpy(buf.get(), "Text im PSRAM");

// array (int-Buffer)
//  auto iarr = ps_make_unique<int>(64);
//  iarr[0] = 42

struct PsramDeleter {
    void operator()(void* ptr) const noexcept {
        if (ptr) {
            free(ptr); // PSRAM freigeben
        }
    }
};

// create individual object in PSRAM
template <typename T, typename... Args> std::unique_ptr<T, PsramDeleter> ps_make_unique(Args&&... args) {
    // rohen Speicher im PSRAM holen
    void* raw = ps_malloc(sizeof(T));
    if (!raw) { throw std::bad_alloc(); }

    // Objekt mit placement-new konstruieren
    T* obj = new (raw) T(std::forward<Args>(args)...);

    // unique_ptr mit eigenem Deleter zurückgeben
    return std::unique_ptr<T, PsramDeleter>(obj);
}

// create an array of objects in PSRAM
template <typename T> std::unique_ptr<T[], PsramDeleter> ps_make_unique(size_t count) {
    T* raw = static_cast<T*>(ps_malloc(sizeof(T) * count));
    if (!raw) {
        printf("OOM: ps_malloc failed (%zu bytes)\n", sizeof(T) * count);
        return std::unique_ptr<T[], PsramDeleter>(nullptr); // kein throw
    }
    return std::unique_ptr<T[], PsramDeleter>(raw);
}

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
    size_t                             allocated_size = 0;
    char*                              name = nullptr; // member for object name
    static inline T                    dummy{};        // For invalid accesses
    size_t                             length_ = 0;    // actual number of characters
  public:
    // Auxiliary function for setting the name
    void set_name(const char* new_name) {
        if (name) {
            free(name);
            name = nullptr;
        }
        if (new_name) {
            std::size_t len = std::strlen(new_name) + 1;
            if (psramFound()) {
                name = static_cast<char*>(ps_malloc(len));
            } else {
                name = static_cast<char*>(malloc(len));
            }
            if (name) {
                std::memcpy(name, new_name, len);
            } else {
                printf("OOM: failed to allocate %zu bytes for name %s\n", len, new_name);
            }
        }
    }

    ps_ptr() = default; // default constructor

    ~ps_ptr() { // destructor
        if (mem) {
            // log_w("Destructor called for %s: Freeing %zu bytes at %p", name ? name : "unnamed", allocated_size * sizeof(T), mem.get());
        } else {
            // log_w("Destructor called for %s: No memory to free.", name ? name : "unnamed");
        }
        if (name) {
            free(name);
            name = nullptr;
        }
    }

    explicit operator bool() const {
        return mem.get() != nullptr; // Or just 'return (bool) mem;'if unique_ptr :: operator bool () is used
    }

    // constructor for C-strings (only active if t == char)
    ps_ptr(const char* src) {
        if constexpr (std::is_same_v<T, char>) {
            assign(src);
        } else {
            static_assert(!std::is_same_v<T, char>, "This constructor is only available for ps_ptr<char>");
        }
    }

    ps_ptr(const char* src, size_t len) {
        if (src && len > 0) {
            allocated_size = len + 1;
            mem = ps_make_unique<char>(allocated_size); // sauber!
            if (mem.get() && allocated_size > len) {    // additional bounds check
                std::memcpy(mem.get(), src, len);
    // suppress warning: We know that allocated_size = len + 1 and therefore index [len] is valid
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstringop-overflow"
                mem.get()[len] = '\0';
    #pragma GCC diagnostic pop
            } else {
                allocated_size = 0; // Reset if allocation fails
            }
        }
    }

    ps_ptr(ps_ptr&& other) noexcept { // move-constructor
        mem = std::move(other.mem);
        allocated_size = other.allocated_size;
        name = other.name;
        other.allocated_size = 0;
        other.name = nullptr;
    }

    // copy constructor (only for Char sensible, deep copy)
    ps_ptr(const ps_ptr& other) {
        if constexpr (std::is_same_v<T, char>) {
            assign(other.get());
        } else {
            // Für Nicht-Char-Typen: Kopieren verboten (wie vorher)
            static_assert(!std::is_same_v<T, T>, "Copy constructor disabled for this type");
        }
    }

    // 🆕 alloc constructor, e.g. ps_ptr<char>buff(1024)
    explicit ps_ptr(size_t n) { alloc(n); }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  A L L O C  📌📌📌

    // ps_ptr <char>test;
    // test.alloc(100");
    // if(test.valid()) {
    //     printf("Größe: %u\n", test.size());
    //     test.clear(); // memset "0"
    // }

    bool alloc(std::size_t size, const char* alloc_name = nullptr, bool usePSRAM = true) {
        size = (size + 15) & ~15;                        // Align to 16 bytes
        if (psramFound() && usePSRAM) {                  // Check at the runtime whether PSRAM is available
            mem.reset(static_cast<T*>(ps_malloc(size))); // <--- Important!
        } else {
            mem.reset(static_cast<T*>(malloc(size))); // <--- Important!
        }
        allocated_size = size;
        if (alloc_name) { set_name(alloc_name); }
        if (!mem) {
            printf("OOM: failed to allocate %zu bytes for %s\n", size, name ? name : "unnamed");
            return false;
        }
        return true;
    }

    bool alloc(const char* alloc_name = nullptr) { // alloc for single objects/structures
        reset();                                   // Freigabe des zuvor gehaltenen Speichers
        void* raw_mem = nullptr;
        if (psramFound()) {                 // Check at the runtime whether PSRAM is available
            raw_mem = ps_malloc(sizeof(T)); // allocated im PSRAM
        } else {
            raw_mem = malloc(sizeof(T)); // allocated im RAM
        }
        if (alloc_name) { set_name(alloc_name); }
        if (raw_mem) {
            mem.reset(new (raw_mem) T()); // placed new: constructor of T is called up in PSRAM
            allocated_size = sizeof(T);
        } else {
            printf("OOM: failed to allocate %zu bytes for %s\n", sizeof(T), name ? name : "unnamed");
            allocated_size = 0; // make sure that allocated_size is 0 if allocation fails
            return false;
        }
        return true;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C A L L O C    📌📌📌
    /**
     * @brief Allocates and zeroes out memory for an array of elements.
     * Chooses between PSRAM (if available) and DRAM.
     * @param num_elements The number of elements to allocate space for.
     */
    bool calloc(std::size_t num_elements, const char* alloc_name = nullptr, bool usePSRAM = true) {
        size_t total_size = num_elements * sizeof(T);
        total_size = (total_size + 15) & ~15; // Align to 16 bytes, consistent with your alloc()

        reset(); // Release of the previously held memory
        if (alloc_name) { set_name(alloc_name); }
        void* raw_mem = nullptr;

        if (psramFound() && usePSRAM) { // Check at the runtime whether PSRAM is available
            raw_mem = ps_malloc(total_size);
        } else {
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
            return false;
        }
        return true;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  A L L O C _ A R R A Y   📌📌📌

    bool alloc_array(std::size_t count, const char* alloc_name = nullptr) {
        if (alloc_name) { set_name(alloc_name); }
        bool res = alloc(sizeof(T) * count);
        //    clear();
        return res;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C A L L O C _ A R R A Y   📌📌📌

    bool calloc_array(std::size_t count, const char* alloc_name = nullptr) {
        if (alloc_name) { set_name(alloc_name); }

        // rohen Speicher holen
        bool res = alloc(sizeof(T) * count);
        if (!res) { return false; }

        // alle Elemente sauber value-initialisieren
        for (std::size_t i = 0; i < count; i++) {
            // placement-new mit {} ruft den Default-Konstruktor / value-init auf
            new (&(get()[i])) T{};
        }
        return true;
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

    // test.realloc(200);
    // printf("new size:  %i\n", test.size());
    // printf("%s\n", test.get());

    void realloc(size_t new_size) {
        void* new_mem = nullptr;
        if (psramFound()) { // Check at the runtime whether PSRAM is available
            new_mem = ps_malloc(new_size);
        } else {
            new_mem = malloc(new_size);
        }
        if (!new_mem) {
            printf("OOM: failed to realloc %zu bytes for %s\n", new_size, name ? name : "unnamed");
            return;
        }

        if (mem && allocated_size > 0) { std::memcpy(new_mem, mem.get(), std::min(allocated_size, new_size)); }

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

    void assign(const char* src) {
        if constexpr (std::is_same_v<T, char>) {
            if (!src) {
                reset();
                return;
            }
            std::size_t len = std::strlen(src) + 1;
            alloc(len);
            if (mem) { std::memcpy(mem.get(), src, len); }
        } else {
            static_assert(std::is_same_v<T, char>, "assign(const char*) is only valid for ps_ptr<char>");
        }
    }

    // ps_ptr <char> my_str2;
    // my_str2.assign("Hallo", 5, "my_str1"); // ps_strndup()
    // printf("%s\n", my_str2.get());

    // Only for T T = char: similar to strndup (max. n chars)
    void assign(const char* src, std::size_t max_len) {
        static_assert(std::is_same_v<T, char>, "assign(const char*, size_t) is only valid for ps_ptr<char>");
        if (!src) {
            reset();
            return;
        }
        std::size_t actual_len = strnlen(src, max_len);
        std::size_t total = actual_len + 1;
        alloc(total);
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

    void copy_from(const T* src, std::size_t count) {
        std::size_t bytes = (count + 1) * sizeof(T); // +1 For zero terminator
        alloc(bytes);
        if (mem && src) {
            std::memcpy(mem.get(), src, count * sizeof(T));
            mem.get()[count] = '\0'; // Zero
        }
    }

    size_t copy_from(const T* src) { // for strings
        if (src == nullptr) {
            log_e("arg. is null");
            return 0;
        }
        std::size_t count = std::strlen(src) + 1;
        std::size_t bytes = count * sizeof(T);
        alloc(bytes);
        if (mem && src) { std::memcpy(mem.get(), src, bytes); }
        return bytes;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C O P Y _ F R O M _ U T F 1 6   📌📌📌
    // copies all characters up to the terminator "\0\0" and converted into UTF-8
    // source: 0x00, 0x4C, 0x00, 0x69, 0x00, 0x74, 0x00, 0x74, 0x00, 0x6C, 0x00, 0x65, 0x00, 0x20, 0x00, 0x4C, 0x00, 0x6F, 0x00, 0x6E, 0x00, 0x64, 0x00, 0x6F, 0x00, 0x6E, 0x00, 0x20, 0x00, 0x47, 0x00,
    //         0x69, 0x00, 0x72, 0x00, 0x6C, 0x00, 0x00
    // is UTF-16LE and will converted to: "Little London Girl"
    // UTF-16LE and UTF-16BE is often found in ID3 header

    #include <cstdint>
    #include <cstring>
    #include <stdexcept>
    #include <vector>

    // convert UTF-16 to UTF-8 and stop at zero terminator
    size_t copy_from_utf16(const uint8_t* src, bool is_big_endian = false) {
        if (!src) {
            log_e("arg. is null");
            return 0;
        }
        std::vector<char> out;
        size_t            i = 0;

        // BOM-Handling
        if (src[i] == 0xFF && src[i + 1] == 0xFE) {
            is_big_endian = false; // UTF-16LE
            i += 2;
        } else if (src[i] == 0xFE && src[i + 1] == 0xFF) {
            is_big_endian = true; // UTF-16BE
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
        alloc(bytes);
        std::memcpy(mem.get(), out.data(), bytes);
        return i;
    }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C O P Y _ F R O M _ I S O _ 8 8 5 9 - 1   📌📌📌
    // convert ISO 8859-1 to UTF-8 and stop at zero terminator
    // 0x48 0x65 0x6C 0x6C 0x6F 0x20 0xC3 0xA4 0x62 0x63 0x00  -> "Hello äbc"

    size_t copy_from_iso8859_1(const uint8_t* src) {
        if (!src) {
            log_e("arg. is null");
            return 0;
        }
        std::vector<char> out;
        size_t            i = 0;

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
        alloc(bytes);
        std::memcpy(mem.get(), out.data(), bytes);
        return i;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  U R L D E C O D E  📌📌📌
    // Decodes the current string in-place from URL encoding.
    // Example:
    //   ps_ptr<char> url = "%D0%B8%D1%81%D0%BF%D1%8B%D1%82%D0%B0%D0%BD%D0%B8%D0%B5.mp3";
    //   url.urldecode(); // → "испытание.mp3"
    //   url = "Born%20On%20The%20B.mp3"; url.urldecode(); // → "Born On The B.mp3"
    //   url = "A+Test.mp3"; url.urldecode(); // → "A Test.mp3"

    void urldecode() {
        static_assert(std::is_same_v<T, char>, "urldecode() is only valid for ps_ptr<char>");
        if (!mem || !get()) {
            log_e("urldecode: No valid string data");
            return;
        }

        char*    str = get();
        uint16_t p1 = 0, p2 = 0;
        char     a, b;

        while (str[p1]) {
            if ((str[p1] == '%') && ((a = str[p1 + 1]) && (b = str[p1 + 2])) && (isxdigit(a) && isxdigit(b))) {

                // Normalize lowercase to uppercase
                if (a >= 'a') a -= ('a' - 'A');
                if (b >= 'a') b -= ('a' - 'A');

                // Convert hex digits to numeric
                a = (a >= 'A') ? (a - 'A' + 10) : (a - '0');
                b = (b >= 'A') ? (b - 'A' + 10) : (b - '0');

                str[p2++] = (a << 4) | b;
                p1 += 3;
            } else if (str[p1] == '+') {
                str[p2++] = ' ';
                p1++;
            } else {
                str[p2++] = str[p1++];
            }
        }
        str[p2] = '\0';
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C L O N E _ F R O M  📌📌📌

    // ps_ptr<char> source;
    // source.assign("Hello World");
    // ps_ptr<char> copy;
    // copy.clone_from(source);
    // printf("%s\n", copy.get());  // → Hello World

    void clone_from(const ps_ptr<T>& other) {
        if (!other.valid() || other.size() == 0) {
            reset();
            return;
        }

        std::size_t sz = other.size();
        alloc(sz);
        if (mem && sz > 0) { std::memcpy(mem.get(), other.get(), sz); }
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
        mem.release();                                   // Gib Besitz auf, ohne zu löschen
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

        if (psramFound()) {                                 // Check at the runtime whether PSRAM is available
            mem.reset(static_cast<T*>(ps_malloc(new_len))); // <--- Important!
        } else {
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

        // Append suffix
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
        } else {
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

    // example: ps_ptr<char> a="Hello "; ps_ptr<char> b="World"; a.append(b);
    template <typename U = T>
        requires std::is_same_v<U, char>
    void append(const ps_ptr<char>& other) {
        const char* suffix = other.get();
        if (!suffix || !*suffix) return; // append nothing if empty

        size_t add_len = std::strlen(suffix);
        size_t old_len = mem ? std::strlen(static_cast<char*>(mem.get())) : 0;
        size_t new_len = old_len + add_len + 1;

        char* old_data = static_cast<char*>(mem.release());

        if (psramFound()) {
            mem.reset(static_cast<T*>(ps_malloc(new_len)));
        } else {
            mem.reset(static_cast<T*>(malloc(new_len)));
        }

        if (!mem) {
            printf("OOM: append(ps_ptr) failed for %zu bytes\n", new_len);
            if (old_data) free(old_data);
            return;
        }

        if (old_data) {
            std::memcpy(mem.get(), old_data, old_len);
            free(old_data);
        }

        std::memcpy(mem.get() + old_len, suffix, add_len + 1);
        allocated_size = new_len;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  P U S H _ B A C K   📌📌📌
    // append individual characters
    // ps_ptr<char>s; s = "abc"; s.push_back('1); -> abc1

    void push_back(char c) {
        if (length + 1 >= capacity()) {
            // Wenn zu klein, Kapazität verdoppeln (wie std::string)
            size_t new_cap = (capacity() == 0) ? 16 : capacity() * 2;
            reserve(new_cap);
        }

        mem.get()[length++] = c;
        mem.get()[length] = '\0';
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  L E N G T H   📌📌📌
    size_t length() const { return length_; }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C A P A C I T Y   📌📌📌
    size_t capacity() const { return allocated_size ? allocated_size - 1 : 0; }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  R E S E R V E  📌📌📌
    void reserve(size_t new_cap) {
        if (new_cap + 1 <= allocated_size) return; // genug Platz vorhanden

        char*  old_data = mem.release();
        size_t old_len = length();

        size_t new_size = new_cap + 1; // +1 für '\0'
        if (psramFound())
            mem.reset(static_cast<char*>(ps_malloc(new_size)));
        else
            mem.reset(static_cast<char*>(malloc(new_size)));

        if (!mem) {
            printf("OOM: reserve(%zu) failed\n", new_size);
            if (old_data) free(old_data);
            return;
        }

        if (old_data) {
            if (old_len > 0)
                std::memcpy(mem.get(), old_data, old_len + 1);
            else
                mem.get()[0] = '\0';
            free(old_data);
        } else {
            mem.get()[0] = '\0';
        }

        allocated_size = new_size;
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

        const char*      str = static_cast<const char*>(mem.get());
        std::string_view sv(str); // C++17, sicherer als strlen
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

        const char*      str = static_cast<const char*>(mem.get());
        std::string_view sv(str); // C++17, sicherer als strlen
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
        if (!this->valid() || !other.valid()) return false;
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

    template <typename... Args> void appendf(const char* fmt, Args&&... args) {
        if (!fmt) return;
        int add_len = std::snprintf(nullptr, 0, fmt, std::forward<Args>(args)...);
        if (add_len < 0) return;

        std::size_t old_len = mem ? std::strlen(mem.get()) : 0;
        std::size_t new_len = old_len + add_len + 1;

        char* old_data = static_cast<char*>(mem.release());
        reset();
        alloc(new_len);

        if (!mem) {
            printf("OOM: appendf() failed for %zu bytes\n", new_len);
            if (old_data) free(old_data);
            return;
        }

        if (old_data) {
            std::memcpy(mem.get(), old_data, old_len);
            free(old_data);
        }

        std::snprintf(mem.get() + old_len, new_len - old_len, fmt, std::forward<Args>(args)...);
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

    //---------------------------------------------------------------------------------------------------
    static int safe_vsnprintf(char* buf, size_t size, const char* fmt, va_list args) {
        va_list args_copy;
        va_copy(args_copy, args);

        // Ergebnis wird hier reingeschrieben
        int total_written = 0;

        const char* p = fmt;
        while (*p) {
            if (*p == '%') {
                [[maybe_unused]] const char* start = p++;
                if (*p == '%') {
                    // Escaped "%%"
                    if (total_written < (int)size) {
                        if (buf) buf[total_written] = '%';
                    }
                    total_written++;
                    p++;
                    continue;
                }

                // collect complete format tokens now(e.g. "%02d", "%-15.8s" ...)
                char  fmt_token[64];
                char* f = fmt_token;
                *f++ = '%';
                while (*p && !strchr("diuoxXfFeEgGaAcspn%", *p)) { *f++ = *p++; }
                if (*p) *f++ = *p++;
                *f = '\0';

                if (fmt_token[std::strlen(fmt_token) - 1] == 's') {
                    // String-Argument abfangen
                    char* s = va_arg(args_copy, char*);
                    if (!s) s = (char*)"(null)";
                    int n = snprintf(buf ? buf + total_written : nullptr, size > (size_t)total_written ? size - total_written : 0, fmt_token, s);
                    if (n < 0) {
                        va_end(args_copy);
                        return n;
                    }
                    total_written += n;
                } else {
                    // alle anderen Typen → normal weitergeben
                    va_list tmp;
                    va_copy(tmp, args_copy);
                    int n = vsnprintf(buf ? buf + total_written : nullptr, size > (size_t)total_written ? size - total_written : 0, fmt_token, tmp);
                    va_end(tmp);
                    if (n < 0) {
                        va_end(args_copy);
                        return n;
                    }
                    total_written += n;

                    // verbrauchte Argumente aus args_copy ziehen
                    switch (fmt_token[std::strlen(fmt_token) - 1]) {
                        case 'd':
                        case 'i':
                        case 'u':
                        case 'o':
                        case 'x':
                        case 'X': (void)va_arg(args_copy, int); break;
                        case 'f':
                        case 'F':
                        case 'e':
                        case 'E':
                        case 'g':
                        case 'G':
                        case 'a':
                        case 'A': (void)va_arg(args_copy, double); break;
                        case 'c': (void)va_arg(args_copy, int); break;
                        case 'p': (void)va_arg(args_copy, void*); break;
                        case 'n': (void)va_arg(args_copy, int*); break;
                    }
                }
            } else {
                // normales Zeichen kopieren
                if (total_written < (int)size) {
                    if (buf) buf[total_written] = *p;
                }
                total_written++;
                p++;
            }
        }

        // Nullterminierung falls Platz
        if (buf && total_written < (int)size) {
            buf[total_written] = '\0';
        } else if (buf && size > 0) {
            buf[size - 1] = '\0';
        }

        va_end(args_copy);
        return total_written;
    }
    //----------------------------------------------------------------------------------------------------

    template <typename U = T>
        requires std::is_same_v<U, char>
    void appendf_va(const char* fmt, va_list& args) {
        if (!fmt) return;
        // determine the length of the format (requires copy of args!)
        va_list args_copy;
        va_copy(args_copy, args);
        int add_len = safe_vsnprintf(nullptr, 0, fmt, args_copy);
        va_end(args_copy);
        if (add_len < 0) return;
        std::size_t old_len = mem ? std::strlen(static_cast<char*>(mem.get())) : 0;
        std::size_t new_len = old_len + static_cast<std::size_t>(add_len) + 1;
        // Speicher neu reservieren
        char* old_data = static_cast<char*>(mem.release());
        reset();
        alloc(new_len);
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
        // check null pointer
        safe_vsnprintf(static_cast<char*>(mem.get()) + old_len, new_len - old_len, fmt, args);
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
        log_d("index_of: Needle not found within %lu bytes", max_length);
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
    int last_index_of(char ch, int start_pos = -1) const {
        if (!mem) return -1;

        const char* str = static_cast<const char*>(mem.get());
        int         len = static_cast<int>(std::strlen(str));

        // if no start position is specified, start at the end
        if (start_pos < 0 || start_pos >= len) start_pos = len - 1;

        for (int i = start_pos; i >= 0; --i) {
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

    // ps_ptr<char> str;
    // str.assign("/audiofiles/my_playlist/podcast/h.mp3");
    // int last = str.last_index_of('/');            // → 32 (the last '/')
    // int prev = str.last_index_of('/', last - 1);  // → 23 (the second to last '/')

    template <typename U = T>
        requires std::is_same_v<U, const char>
    int last_index_of(const char ch, int start_pos = -1) const {
        if (!mem) return -1;

        const char* str = static_cast<const char*>(mem.get());
        int         len = static_cast<int>(std::strlen(str));

        // if no start position is specified, start at the end
        if (start_pos < 0 || start_pos >= len) start_pos = len - 1;

        for (int i = start_pos; i >= 0; --i) {
            if (str[i] == ch) return i;
        }
        return -1;
    }

    // int last_index_of(const T& value, int start_pos = -1) const {
    //     if (!mem || allocated_size < sizeof(T)) return -1;

    //     std::size_t count = allocated_size / sizeof(T);
    //     T*          data = get();

    //     if (start_pos < 0 || start_pos >= static_cast<int>(count)) start_pos = static_cast<int>(count) - 1;

    //     for (int i = start_pos; i >= 0; --i) {
    //         if (data[i] == value) return i;
    //     }
    //     return -1;
    // }
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
            if (std::memcmp(haystack + i, needle, needle_len) == 0) { return static_cast<int>(i); }
        }

        return -1;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  S U B S T R  📌📌📌
    ps_ptr<char> substr(size_t pos, size_t count = std::string::npos) const {
        const char* src = mem.get();
        if (!src) return ps_ptr<char>{};

        size_t len = std::strlen(src);
        if (pos >= len) return ps_ptr<char>{}; // leer zurück

        size_t n = (count == std::string::npos || pos + count > len) ? (len - pos) : count;

        return ps_ptr<char>(src + pos, n);
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

        size_t               count = 0;
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
            if ((*s & 0x80) == 0)
                s += 1; // ASCII
            else if ((*s & 0xE0) == 0xC0)
                s += 2; // 2 Byte
            else if ((*s & 0xF0) == 0xE0)
                s += 3; // 3 Byte
            else if ((*s & 0xF8) == 0xF0)
                s += 4; // 4 Byte
            else
                s += 1; // invalid ? carefully next

            ++count;
        }

        return count;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  I S _ U T F 8   📌📌📌

    // ps_ptr<char> a = "Hello";    // ASCII → valid UTF-8
    // ps_ptr<char> b = u8"Привет"; // Russian → valid UTF-8
    // ps_ptr<char> c = "\xC3\x28"; // invalid (bad UTF-8 sequence)

    // if (is_utf8(a)) printf("a is UTF-8\n");
    // if (is_utf8(b)) printf("b is UTF-8\n");
    // if (!is_utf8(c)) printf("c is not UTF-8\n");

    bool is_utf8() const {
        const unsigned char* s = reinterpret_cast<const unsigned char*>(mem.get());
        if (!s) return false;

        while (*s) {
            unsigned char c = *s++;

            // 1-byte ASCII (0xxxxxxx)
            if (c < 0x80) continue;

            // 2-byte-sequence (110xxxxx 10xxxxxx)
            if ((c >> 5) == 0x6) {
                if ((s[0] & 0xC0) != 0x80) return false;
                s += 1;
                continue;
            }

            // 3-byte-sequence (1110xxxx 10xxxxxx 10xxxxxx)
            if ((c >> 4) == 0xE) {
                if ((s[0] & 0xC0) != 0x80 || (s[1] & 0xC0) != 0x80) return false;

                unsigned int cp = ((c & 0x0F) << 12) | ((s[0] & 0x3F) << 6) | (s[1] & 0x3F);
                // Excessive coding or surrogate?
                if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) return false;

                s += 2;
                continue;
            }

            // 4-byte-sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
            if ((c >> 3) == 0x1E) {
                if ((s[0] & 0xC0) != 0x80 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return false;

                unsigned int cp = ((c & 0x07) << 18) | ((s[0] & 0x3F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
                // Overlong or out of Unicode range
                if (cp < 0x10000 || cp > 0x10FFFF) return false;

                s += 3;
                continue;
            }

            // Invalid starting byte
            return false;
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
        bool        in_string = false;
        int         brace_count = 0;
        const char* curr = p;

        while (curr <= end) {
            if (!in_string) {
                if (*curr == '{')
                    brace_count++;
                else if (*curr == '}')
                    brace_count--;
                else if (*curr == '"')
                    in_string = true;
                else if (*curr == ':' || *curr == ',') {
                    // Allow colons and commas outside strings
                } else if (*curr != ' ' && *curr != '\t' && *curr != '\n' && *curr != '\r') {
                    // Allow digits, true, false, null, etc., but for simplicity, we just check for valid chars
                    if (!(*curr >= '0' && *curr <= '9') && *curr != '-' && *curr != '.' && *curr != 't' && *curr != 'f' && *curr != 'n' && *curr != '[' && *curr != ']') {
                        return false; // Invalid character outside string
                    }
                }
            } else {
                if (*curr == '"')
                    in_string = false;
                else if (*curr == '\\')
                    curr++; // Skip escaped character
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
        char        utf8[5];

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
    // 📌📌📌  H T M L T O U T F 8   📌📌📌
    // ps_ptr<char> html, utf8;
    // html.assign("Hello%20World");
    // utf8.unicodeToUTF8(html.get());
    // log_i("%s", utf8.get()); // shows: Hallo World

    void htmlToUTF8(const char* src) {
        this->clear();
        if (!src) return;

        struct EntityMap {
            const char* name;
            uint32_t    codepoint;
        };

        static const EntityMap entities[] = {
            {"amp", 0x0026},    // &
            {"lt", 0x003C},     // <
            {"gt", 0x003E},     // >
            {"quot", 0x0022},   // "
            {"apos", 0x0027},   // '
            {"nbsp", 0x00A0},   // non-breaking space
            {"euro", 0x20AC},   // €
            {"copy", 0x00A9},   // ©
            {"reg", 0x00AE},    // ®
            {"trade", 0x2122},  // ™
            {"hellip", 0x2026}, // …
            {"ndash", 0x2013},  // –
            {"mdash", 0x2014},  // —
            {"sect", 0x00A7},   // §
            {"para", 0x00B6}    // ¶
        };

        auto encodeCodepointToUTF8 = [](uint32_t cp, char* out) -> int {
            if (cp <= 0x7F) {
                out[0] = (char)cp;
                return 1;
            } else if (cp <= 0x7FF) {
                out[0] = (char)(0xC0 | (cp >> 6));
                out[1] = (char)(0x80 | (cp & 0x3F));
                return 2;
            } else if (cp <= 0xFFFF) {
                out[0] = (char)(0xE0 | (cp >> 12));
                out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                out[2] = (char)(0x80 | (cp & 0x3F));
                return 3;
            } else if (cp <= 0x10FFFF) {
                out[0] = (char)(0xF0 | (cp >> 18));
                out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
                out[3] = (char)(0x80 | (cp & 0x3F));
                return 4;
            }
            return 0;
        };

        auto find_named_entity = [&](const char* p, uint32_t* cp_out, int* entity_len) -> bool {
            for (size_t i = 0; i < sizeof(entities) / sizeof(entities[0]); ++i) {
                const char* name = entities[i].name;
                size_t      len = std::strlen(name);
                if (strncmp(p + 1, name, len) == 0 && p[1 + len] == ';') {
                    *cp_out = entities[i].codepoint;
                    *entity_len = (int)(len + 2); // &name;
                    return true;
                }
            }
            return false;
        };

        const char* p = src;
        char        utf8[5];

        while (*p) {
            if (p[0] == '&') {
                uint32_t cp = 0;
                int      ent_len = 0;

                // 1) Named entity
                if (find_named_entity(p, &cp, &ent_len)) {
                    int n = encodeCodepointToUTF8(cp, utf8);
                    if (n > 0) {
                        utf8[n] = '\0';
                        this->append(utf8);
                        p += ent_len;
                        continue;
                    }
                }

                // 2) Numeric entity
                if (p[1] == '#') {
                    const char* q = p + 2;
                    uint32_t    value = 0;

                    if (*q == 'x' || *q == 'X') {
                        // Hex &#x1F60A;
                        q++;
                        char*         endptr = nullptr;
                        unsigned long tmp = strtoul(q, &endptr, 16);
                        if (endptr && *endptr == ';' && tmp <= 0x10FFFF) {
                            value = (uint32_t)tmp;
                            int n = encodeCodepointToUTF8(value, utf8);
                            if (n > 0) {
                                utf8[n] = '\0';
                                this->append(utf8);
                                p = endptr + 1;
                                continue;
                            }
                        }
                    } else {
                        // Decimal &#1234;
                        char*         endptr = nullptr;
                        unsigned long tmp = strtoul(q, &endptr, 10);
                        if (endptr && *endptr == ';' && tmp <= 0x10FFFF) {
                            value = (uint32_t)tmp;
                            int n = encodeCodepointToUTF8(value, utf8);
                            if (n > 0) {
                                utf8[n] = '\0';
                                this->append(utf8);
                                p = endptr + 1;
                                continue;
                            }
                        }
                    }
                }

                // Unbekannte Entity → nur '&' übernehmen
                this->append(p, 1);
                p++;
                continue;
            }

            // Normales Zeichen
            this->append(p, 1);
            p++;
        }
    }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  G E T   📌📌📌
    T* get() const { return static_cast<T*>(mem.get()); }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C_G E T  (safe)   📌📌📌
    const char* c_get(const char* fallback = "") const {
        if constexpr (std::is_same_v<T, char>) {
            return mem ? mem.get() : fallback;
        } else {
            return fallback;
        }
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  P R I N T    📌📌📌
    // Prints the stored string to the standard output using printf.
    // Only valid for ps_ptr<char>. Uses c_get() to safely handle null cases.
    template <typename U = T>
        requires std::is_same_v<U, char>
    void print() const {
        printf("%s: %s", name ? name : "unnamed", c_get());
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  P R I N T L N    📌📌📌
    // Prints the stored string to the standard output using printf.
    // Only valid for ps_ptr<char>. Uses c_get() to safely handle null cases.
    template <typename U = T>
        requires std::is_same_v<U, char>
    void println() const {
        printf("%s: %s\n", name ? name : "unnamed", c_get());
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
    // 📌📌📌   A T    📌📌📌

    // Special method for ps_ptr<ps_ptr<T>>
    template <typename U = T> auto at(size_t index) -> typename std::enable_if<std::is_same<U, ps_ptr<typename U::element_type>>::value, ps_ptr<typename U::element_type>&>::type {
        return static_cast<ps_ptr<typename U::element_type>*>(get())[index];
    }
    using element_type = T;
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  A S  📌📌📌

    // ps_ptr<void> generic;
    // generic.alloc(64);
    // uint32_t* p = generic.as<uint32_t>();

    template <typename U> U* as() const { return reinterpret_cast<U*>(get()); }
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
        std::size_t  newLen = originalLen + insertLen + 1;
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
        char*       end = nullptr;
        uint64_t    result = std::strtoull(str, &end, base);
        if (end == str) {
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
    uint32_t to_uint32(int base = 10) const {
        static_assert(std::is_same_v<T, char>, "to_uint32 is only valid for ps_ptr<char>");
        if (!mem || !get()) {
            log_e("to_uint32: No valid string data");
            return 0;
        }
        const char*   str = get();
        char*         end = nullptr;
        unsigned long result = std::strtoul(str, &end, base);
        if (end == str) {
            log_e("to_uint32: Invalid numeric value in '%s' for base %i", str, base);
            return 0;
        }
        if (result > UINT32_MAX) {
            log_e("to_uint32: Value in '%s' exceeds UINT32_MAX (%u) for base %i", str, UINT32_MAX, base);
            return 0;
        }
        return static_cast<uint32_t>(result);
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  T O _ I N T 3 2  📌📌📌
    // Retrieves the numeric value (int32_t) from the stored string, parsing it as a number in the specified base (default: 10 for decimal).
    // Example:
    //   ps_ptr<char> temp = "-12345"; int32_t val = temp.to_int32(10); // Returns -12345
    //   ps_ptr<char> hex  = "0x7FFF"; int32_t val = hex.to_int32(16); // Returns 32767
    // If the string is empty, null, invalid, or exceeds INT32 range (-2147483648 ... 2147483647), returns 0 and logs an error.

    int32_t to_int32(int base = 10) const {
        static_assert(std::is_same_v<T, char>, "to_int32 is only valid for ps_ptr<char>");
        if (!mem || !get()) {
            log_e("to_int32: No valid string data");
            return 0;
        }

        const char* str = get();
        char*       end = nullptr;
        long        result = std::strtol(str, &end, base);

        if (end == str) {
            log_e("to_int32: Invalid numeric value in '%s' for base %i", str, base);
            return 0;
        }

        if (result < INT32_MIN || result > INT32_MAX) {
            log_e("to_int32: Value in '%s' exceeds INT32 range (%ld..%ld) for base %i", str, (long)INT32_MIN, (long)INT32_MAX, base);
            return 0;
        }

        return static_cast<int32_t>(result);
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  T O _ I N T 6 4  📌📌📌
    // Retrieves the numeric value (int64_t) from the stored string, parsing it as a number
    // in the specified base (default: 10 for decimal).
    //
    // Examples:
    //   ps_ptr<char> val1 = "9223372036854775807"; int64_t v1 = val1.to_int64();  // OK
    //   ps_ptr<char> val2 = "-1234567890123";     int64_t v2 = val2.to_int64();  // OK
    //   ps_ptr<char> val3 = "0x7FFFFFFFFFFFFFFF"; int64_t v3 = val3.to_int64(16); // OK
    //
    // If the string is empty, invalid, or exceeds INT64 range (-9223372036854775808 .. 9223372036854775807),
    // returns 0 and logs an error.

    int64_t to_int64(int base = 10) const {
        static_assert(std::is_same_v<T, char>, "to_int64 is only valid for ps_ptr<char>");
        if (!mem || !get()) {
            log_e("to_int64: No valid string data");
            return 0;
        }

        const char* str = get();
        char*       end = nullptr;
        long long   result = std::strtoll(str, &end, base);

        if (end == str) {
            log_e("to_int64: Invalid numeric value in '%s' for base %i", str, base);
            return 0;
        }

        if (result < INT64_MIN || result > INT64_MAX) {
            log_e("to_int64: Value in '%s' exceeds INT64 range (%lld..%lld) for base %i", str, (long long)INT64_MIN, (long long)INT64_MAX, base);
            return 0;
        }

        return static_cast<int64_t>(result);
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
        for (uint8_t i = 0; i < size; ++i) { result = (result << 8) | data[i]; }
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
        for (int i = size - 1; i >= 0; --i) { result = (result << 8) | data[i]; }
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
            if (!std::strchr(chars, *src)) { *dst++ = *src; }
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

        if (fromLen == 0) return false; // Nichts zu ersetzen

        std::vector<char> result;
        const char*       read = src;

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

        char*       str = static_cast<char*>(mem.get());
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
        if (pos >= strlen()) return;
        char*       str = get();
        std::size_t len = std::strlen(str);
        if (pos < len)
            str[pos] = '\0';
        else
            log_e("truncate pos %i out of length %i", pos, len);
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
            char*       str = get();
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
            if (!includeChar) ++pos; // wenn nicht inklusive: das Zeichen behalten
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

        char*       str = get();
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

    bool contains(const char* substr) const { return substr && this->valid() && std::strstr(this->get(), substr); }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C O N T A I N S _ W I T H _ I C A S E    📌📌📌
    bool contains_with_icase(const char* substr) const {
        if (!substr || !this->valid()) { return false; }

        const char* haystack = this->get();
        const char* needle = substr;

        // Wir kopieren den Text in temporäre lowercase-Strings
        std::string haystack_lower(haystack);
        std::string needle_lower(needle);

        std::transform(haystack_lower.begin(), haystack_lower.end(), haystack_lower.begin(), [](unsigned char c) { return std::tolower(c); });
        std::transform(needle_lower.begin(), needle_lower.end(), needle_lower.begin(), [](unsigned char c) { return std::tolower(c); });

        return haystack_lower.find(needle_lower) != std::string::npos;
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C L E A R   📌📌📌

    void clear() {
        if (mem && allocated_size > 0) { std::memset(mem.get(), 0, allocated_size); length_ = 0; }
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  S I Z E   📌📌📌

    size_t size() const { return allocated_size; }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  E M P T Y   📌📌📌

    bool empty() const noexcept { return allocated_size == 0; }

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
        if (!valid()) {
            printf("hex_dump: invalid buffer\n");
            return;
        }

        if (allocated_size < n) n = allocated_size;
        if (n == 0) {
            printf("hex_dump: no data\n");
            return;
        }

        uint8_t items_per_line = 30;

        static const char* sym[32] = {"NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL", "BS ", "TAB", "LF ", "VT ", "FF ", "CR ", "SO ", "SI ",
                                      "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB", "CAN", "EM ", "SUB", "ESC", "FS ", "GS ", "RS ", "US "};

        const uint8_t* buff = reinterpret_cast<const uint8_t*>(get());
        if (!name)
            printf("dumping %u bytes:\n", n);
        else
            printf("%s dumping %u bytes:\n", name, n);

        for (uint16_t i = 0; i < n; i += items_per_line) {
            uint16_t m = std::min<uint16_t>(n, i + items_per_line);
            uint8_t  s = 0;

            // Hex view
            for (uint16_t j = i; j < m; j++) {
                printf("0x%02X ", buff[j]);
                if (++s % 10 == 0) printf("   ");
            }
            printf("\n");

            // ASCII / symbolic view
            s = 0;
            for (uint16_t j = i; j < m; j++) {
                uint8_t c = buff[j];
                if (c >= 32)
                    printf("%c    ", c);
                else
                    printf("%s  ", sym[c]);
                if (++s % 10 == 0) printf("   ");
            }
            printf("\n");
        }
        printf("\n");
    }
    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  O P E R A T O R  📌📌📌 (within class)
    // ——————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

    // Copy-Assignment (only for char sensible)
    ps_ptr& operator=(const ps_ptr& other) {
        if (this != &other) {
            if constexpr (std::is_same_v<T, char>) {
                assign(other.get());
            } else {
                static_assert(!std::is_same_v<T, T>, "Copy assignment disabled for this type");
            }
        }
        return *this;
    }

    ps_ptr<T>& operator=(const T* raw_ptr) { // e.g.  ps_ptr<char> h; h = "123";
        if constexpr (std::is_same_v<T, char>) {
            assign(raw_ptr);
        } else {
            static_assert(!std::is_same_v<T, T>, "Assignment from const pointer disabled for this type");
        }
        return *this;
    }

    ps_ptr<T>& operator=(T* raw_ptr) {
        if (mem.get() != raw_ptr) {
            mem.reset(raw_ptr);
            allocated_size = 0; // (raw_ptr != nullptr) ? /* Calculate size here */ : 0;
        }
        return *this;
    }

    // Move-Assignment-Operator
    ps_ptr& operator=(ps_ptr&& other) noexcept {
        if (this != &other) {
            if (name) {
                free(name);
                name = nullptr;
            }
            mem = std::move(other.mem);
            allocated_size = other.allocated_size;
            name = other.name;
            other.allocated_size = 0;
            other.name = nullptr;
        }
        return *this;
    }

    T* operator->() const { return get(); }
    T& operator*() const { return *get(); }

    // Safe operator[] with logging
    T& operator[](std::size_t index) noexcept {
        if (index >= allocated_size) {
            log_e("ps_ptr[]: Index %zu out of bounds (size = %zu)", index, allocated_size);
            return dummy; // Access allowed, but ineffective
        }
        return mem[index];
    }

    const T& operator[](std::size_t index) const noexcept {
        if (index >= allocated_size) {
            log_e("ps_ptr[] (const): Index %zu out of bounds (size = %zu)", index, allocated_size);
            return dummy;
        }
        return mem[index];
    }

    // C++20: Spaceship-Operator — Automatically creates all comparison operators
    auto operator<=>(const ps_ptr& other) const noexcept {
        if constexpr (std::is_same_v<T, char>) {
            const char* s1 = get();
            const char* s2 = other.get();
            int         result = std::strcmp(s1 ? s1 : "", s2 ? s2 : "");
            return (result < 0) ? std::strong_ordering::less : (result > 0) ? std::strong_ordering::greater : std::strong_ordering::equal;
        } else {
            // for non-string types: Compare the pointer address
            return mem.get() <=> other.mem.get();
        }
    }

    // ps_ptr<char> a = "Hallo"; a += " Welt";
    template <typename U = T>
        requires std::is_same_v<U, char>
    ps_ptr<char>& operator+=(const char* rhs) {
        append(rhs);
        return *this;
    }

    // ps_ptr<char> a = "Hallo", b = " Welt"; a += b;
    template <typename U = T>
        requires std::is_same_v<U, char>
    ps_ptr<char>& operator+=(const ps_ptr<char>& rhs) {
        append(rhs);
        return *this;
    }

    // Iterator compatible accesses (make ps_ptr STL compatible)
    T*       begin() noexcept { return mem.get(); }
    const T* begin() const noexcept { return mem.get(); }

    T*       end() noexcept { return mem.get() + allocated_size; }
    const T* end() const noexcept { return mem.get() + allocated_size; }

    const T* cbegin() const noexcept { return mem.get(); }
    const T* cend() const noexcept { return mem.get() + allocated_size; }

    // 🔹 Optional: pointer arithmetic (syntactic only)
    T*       operator+(std::size_t offset) noexcept { return mem.get() + offset; }
    const T* operator+(std::size_t offset) const noexcept { return mem.get() + offset; }

    void fill(const T& value) { std::fill(begin(), end(), value); }

    // Access as std::span – secure view of the data
    std::span<T> span() noexcept { return std::span<T>(mem.get(), allocated_size); }

    std::span<const T> span() const noexcept { return std::span<const T>(mem.get(), allocated_size); }
};
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  O P E R A T O R  📌📌📌 (witout class)
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

// ps_ptr<char> a = "Hello "; ps_ptr<char> b = "World"; ps_ptr<char> c = a + b;  // results in new string
template <typename T>
    requires std::is_same_v<T, char>
ps_ptr<char> operator+(const ps_ptr<T>& lhs, const ps_ptr<T>& rhs) {
    ps_ptr<char> result(lhs);
    result.append(rhs);
    return result;
}

template <typename T>
    requires std::is_same_v<T, char>
ps_ptr<char> operator+(const ps_ptr<T>& lhs, const char* rhs) {
    ps_ptr<char> result(lhs);
    result.append(rhs);
    return result;
}

template <typename T>
    requires std::is_same_v<T, char>
ps_ptr<char> operator+(const char* lhs, const ps_ptr<T>& rhs) {
    ps_ptr<char> result(lhs);
    result.append(rhs);
    return result;
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator==(const ps_ptr<T>& lhs, const ps_ptr<T>& rhs) {
    const char* a = lhs.get();
    const char* b = rhs.get();
    if (!a || !b) return a == b; // beide nullptr → true
    return std::strcmp(a, b) == 0;
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator==(const ps_ptr<T>& lhs, const char* rhs) {
    const char* a = lhs.get();
    if (!a || !rhs) return a == rhs;
    return std::strcmp(a, rhs) == 0;
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator==(const char* lhs, const ps_ptr<T>& rhs) {
    return rhs == lhs;
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator!=(const ps_ptr<T>& lhs, const ps_ptr<T>& rhs) {
    return !(lhs == rhs);
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator!=(const ps_ptr<T>& lhs, const char* rhs) {
    return !(lhs == rhs);
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator!=(const char* lhs, const ps_ptr<T>& rhs) {
    return !(lhs == rhs);
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator<(const ps_ptr<T>& lhs, const ps_ptr<T>& rhs) {
    const char* a = lhs.get();
    const char* b = rhs.get();
    if (!a || !b) return a < b; // nullptr kleiner als nicht-null
    return std::strcmp(a, b) < 0;
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator<(const ps_ptr<T>& lhs, const char* rhs) {
    const char* a = lhs.get();
    if (!a || !rhs) return a < rhs;
    return std::strcmp(a, rhs) < 0;
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator<(const char* lhs, const ps_ptr<T>& rhs) {
    const char* b = rhs.get();
    if (!lhs || !b) return lhs < b;
    return std::strcmp(lhs, b) < 0;
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator>(const ps_ptr<T>& lhs, const ps_ptr<T>& rhs) {
    return rhs < lhs;
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator>(const ps_ptr<T>& lhs, const char* rhs) {
    return rhs && (std::strcmp(lhs.get(), rhs) > 0);
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator>(const char* lhs, const ps_ptr<T>& rhs) {
    return rhs < lhs;
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator<=(const ps_ptr<T>& lhs, const ps_ptr<T>& rhs) {
    return !(rhs < lhs);
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator<=(const ps_ptr<T>& lhs, const char* rhs) {
    return !(lhs > rhs);
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator<=(const char* lhs, const ps_ptr<T>& rhs) {
    return !(rhs < lhs);
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator>=(const ps_ptr<T>& lhs, const ps_ptr<T>& rhs) {
    return !(lhs < rhs);
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator>=(const ps_ptr<T>& lhs, const char* rhs) {
    return !(lhs < rhs);
}

template <typename T>
    requires std::is_same_v<T, char>
bool operator>=(const char* lhs, const ps_ptr<T>& rhs) {
    return !(lhs < rhs);
}

template <typename T>
    requires std::is_same_v<T, char>
std::ostream& operator<<(std::ostream& os, const ps_ptr<T>& str) {
    const char* s = str.get();
    if (s) os << s;
    return os;
}

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  S T R U C T U R E S    📌📌📌
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

//  typedef struct _hwoe{
//      bool ssl;
//      ps_ptr<char> hwoe;  // host without extension
//      uint16_t     port;
//      ps_ptr<char>extension[100];
//      ps_ptr<char> query_string;
//  } hwoe_t;
//  PS_STRUCT_FREE_MEMBERS(hwoe_t,
//      ptr->hwoe.reset();
//      for(int i = 0; i< 100; i++)ptr->extension[i].reset();
//      ptr->query_string.reset();
//  )

//  ps_struct_ptr<hwoe_t> result;
//  result.alloc();
//  result->extension.copy_from(path
//  .....

template <typename T> class ps_struct_ptr {
  private:
    std::unique_ptr<T, PsramDeleter> mem;
    char*                            name = nullptr; // name member

    // Auxiliary function for setting the name
    void set_name(const char* new_name) {
        if (name) {
            free(name);
            name = nullptr;
        }
        if (new_name) {
            std::size_t len = std::strlen(new_name) + 1;
            if (psramFound()) {
                name = static_cast<char*>(ps_malloc(len));
            } else {
                name = static_cast<char*>(malloc(len));
            }
            if (name) {
                std::memcpy(name, new_name, len);
            } else {
                printf("OOM: failed to allocate %zu bytes for name %s\n", len, new_name);
            }
        }
    }

  public:
    ps_struct_ptr() = default; // default constructor

    explicit ps_struct_ptr(const char* name_str) { // named constructor
        set_name(name_str);
    }

    ~ps_struct_ptr() { // destructor
        if (mem) {
            log_w("Destructor called for %s: Freeing %zu bytes at %p", name ? name : "unnamed", sizeof(T), mem.get());
        } else {
            log_w("Destructor called for %s: No memory to free.", name ? name : "unnamed");
        }
        if (name) {
            free(name);
            name = nullptr;
        }
    }

    ps_struct_ptr(ps_struct_ptr&& other) noexcept { // Move constructor
        mem = std::move(other.mem);
        name = other.name;
        other.name = nullptr;
    }

    ps_struct_ptr& operator=(ps_struct_ptr&& other) noexcept { // Move Assignment operator
        if (this != &other) {
            if (name) {
                free(name);
                name = nullptr;
            }
            mem = std::move(other.mem);
            name = other.name;
            other.name = nullptr;
        }
        return *this;
    }

    ps_struct_ptr(const ps_struct_ptr&) = delete;
    ps_struct_ptr& operator=(const ps_struct_ptr&) = delete;

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  A L L O C  📌📌📌
    void alloc(const char* name_str = nullptr) {
        reset();
        if (name_str) { set_name(name_str); }
        void* raw_mem = nullptr;
        if (psramFound()) {
            raw_mem = ps_malloc(sizeof(T));
        } else {
            raw_mem = malloc(sizeof(T));
        }
        if (raw_mem) {
            mem.reset(new (raw_mem) T());
        } else {
            printf("OOM: failed to allocate %zu bytes for %s\n", sizeof(T), name ? name : (name_str ? name_str : "unnamed"));
        }
    }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C A L L O C  📌📌📌
    void calloc(const char* name_str = nullptr) {
        reset();
        if (name_str) { set_name(name_str); }
        void* raw_mem = nullptr;
        if (psramFound()) {
            raw_mem = ps_calloc(1, sizeof(T));
        } else {
            raw_mem = calloc(1, sizeof(T));
        }
        if (raw_mem) {
            mem.reset(static_cast<T*>(raw_mem));
        } else {
            printf("OOM: failed to allocate %zu bytes for %s\n", sizeof(T), name ? name : (name_str ? name_str : "unnamed"));
        }
    }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  R E S E T   📌📌📌
    void reset() {
        mem.reset(); // Name is not released, remains up to the destructor
    }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  O P E R A T O R ->   📌📌📌
    T* operator->() { return mem.get(); }

    const T* operator->() const { return mem.get(); }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  O P E R A T O R *   📌📌📌
    T& operator*() { return *mem; }

    const T& operator*() const { return *mem; }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  G E T   📌📌📌
    T* get() noexcept { return mem.get(); }

    const T* get() const noexcept { return mem.get(); }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    inline void free_all_ptr_members(); // prototypes
    inline void free_field(char*& field);

    void set_ptr_field(char** field, const char* text) {
        if (!mem || !field) return;
        if (*field) {
            free(*field);
            *field = nullptr;
        }
        if (text) {
            std::size_t len = strlen(text) + 1;
            char*       p = nullptr;
            if (psramFound()) { // Check at the runtime whether PSRAM is available
                p = static_cast<char*>(ps_malloc(len));
            } else {
                p = static_cast<char*>(malloc(len));
            }
            if (p) {
                memcpy(p, text, len);
                *field = p;
            }
        }
    }

    bool        valid() const { return mem.get() != nullptr; }
    std::size_t size() const { return sizeof(T); }
    void        clear() { reset(); }
};

    // ——————————————————————————————————————————————————————————————
    // Macro for the declaration of releasing fields for a structure
    #define PS_STRUCT_FREE_MEMBERS(TYPE, ...)                                                         \
        template <> inline void ps_struct_ptr<TYPE>::free_all_ptr_members() {                         \
            auto ptr = this->get(); /* 'ptr' ist hier ein Pointer auf die Struktur TYPE */            \
            if (ptr) {              /* Sicherheitsprüfung, um sicherzustellen, dass ptr gültig ist */ \
                __VA_ARGS__                                                                           \
            }                                                                                         \
        }

inline void free_field(char*& field) {
    if (field) {
        free(field);
        field = nullptr;
    }
}

// Variadic Auxiliary function
template <typename... Args> inline void free_fields(Args&... fields) {
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
// int ch = 0; // exanple: channel 0
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

template <typename T> class ps_array2d {
  private:
    std::unique_ptr<T[], PsramDeleter> mem;
    size_t                             rows = 0;
    size_t                             cols = 0;
    char*                              name = nullptr; // Neues Mitglied für den Objektnamen

    // Hilfsfunktion zum Setzen des Namens
    void set_name(const char* new_name) {
        if (name) {
            free(name);
            name = nullptr;
        }
        if (new_name) {
            std::size_t len = std::strlen(new_name) + 1;
            if (psramFound()) {
                name = static_cast<char*>(ps_malloc(len));
            } else {
                name = static_cast<char*>(malloc(len));
            }
            if (name) {
                std::memcpy(name, new_name, len);
            } else {
                printf("OOM: failed to allocate %zu bytes for name %s\n", len, new_name);
            }
        }
    }

  public:
    // Standardkonstruktor
    ps_array2d() = default;

    // Konstruktor mit Namen
    explicit ps_array2d(const char* name_str) { set_name(name_str); }

    // Destruktor
    ~ps_array2d() {
        if (mem) {
            log_w("Destructor called for %s: Freeing %zu bytes at %p", name ? name : "unnamed", rows * cols * sizeof(T), mem.get());
        } else {
            log_w("Destructor called for %s: No memory to free.", name ? name : "unnamed");
        }
        if (name) {
            free(name);
            name = nullptr;
        }
    }

    // Move-Constructor
    ps_array2d(ps_array2d&& other) noexcept {
        mem = std::move(other.mem);
        rows = other.rows;
        cols = other.cols;
        name = other.name;
        other.rows = 0;
        other.cols = 0;
        other.name = nullptr;
    }

    // Move-Assignment-Operator
    ps_array2d& operator=(ps_array2d&& other) noexcept {
        if (this != &other) {
            if (name) {
                free(name);
                name = nullptr;
            }
            mem = std::move(other.mem);
            rows = other.rows;
            cols = other.cols;
            name = other.name;
            other.rows = 0;
            other.cols = 0;
            other.name = nullptr;
        }
        return *this;
    }

    ps_array2d(const ps_array2d&) = delete;
    ps_array2d& operator=(const ps_array2d&) = delete;

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  A L L O C  📌📌📌
    void alloc(size_t r, size_t c, const char* alloc_name = nullptr, bool usePSRAM = true) {
        reset();
        if (alloc_name) { set_name(alloc_name); }
        rows = r;
        cols = c;
        size_t total_size = rows * cols * sizeof(T);
        total_size = (total_size + 15) & ~15; // Align to 16 bytes
        if (psramFound() && usePSRAM) {
            mem.reset(static_cast<T*>(ps_malloc(total_size)));
        } else {
            mem.reset(static_cast<T*>(malloc(total_size)));
        }
        if (!mem) {
            printf("OOM: failed to allocate %zu bytes for %s\n", total_size, name ? name : (alloc_name ? alloc_name : "unnamed"));
            rows = 0;
            cols = 0;
        }
    }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C A L L O C  📌📌📌
    void calloc(size_t r, size_t c, const char* alloc_name = nullptr, bool usePSRAM = true) {
        reset();
        if (alloc_name) { set_name(alloc_name); }
        rows = r;
        cols = c;
        size_t total_size = rows * cols * sizeof(T);
        total_size = (total_size + 15) & ~15; // Align to 16 bytes
        void* raw_mem = nullptr;
        if (psramFound() && usePSRAM) {
            raw_mem = ps_calloc(1, total_size);
        } else {
            raw_mem = calloc(1, total_size);
        }
        if (raw_mem) {
            mem.reset(static_cast<T*>(raw_mem));
        } else {
            printf("OOM: failed to allocate %zu bytes for %s\n", total_size, name ? name : (alloc_name ? alloc_name : "unnamed"));
            rows = 0;
            cols = 0;
        }
    }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  R E S E T   📌📌📌
    void reset() {
        mem.reset();
        rows = 0;
        cols = 0;
        // Name is not released, remains up to the destructor
    }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  O P E R A T O R []   📌📌📌
    T* operator[](size_t row) { return mem.get() + row * cols; }

    const T* operator[](size_t row) const { return mem.get() + row * cols; }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  G E T   📌📌📌
    T* get() { return mem.get(); }

    const T* get() const { return mem.get(); }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  V A L I D / S I Z E   📌📌📌
    bool   valid() const { return mem.get() != nullptr; }
    size_t get_rows() const { return rows; }
    size_t get_cols() const { return cols; }
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

template <typename T> class ps_array3d {
  private:
    std::unique_ptr<T[], PsramDeleter> mem;
    size_t                             dim1 = 0;
    size_t                             dim2 = 0;
    size_t                             dim3 = 0;
    char*                              name = nullptr; // Neues Mitglied für den Objektnamen

    // Hilfsfunktion zum Setzen des Namens
    void set_name(const char* new_name) {
        if (name) {
            free(name);
            name = nullptr;
        }
        if (new_name) {
            std::size_t len = std::strlen(new_name) + 1;
            if (psramFound()) {
                name = static_cast<char*>(ps_malloc(len));
            } else {
                name = static_cast<char*>(malloc(len));
            }
            if (name) {
                std::memcpy(name, new_name, len);
            } else {
                printf("OOM: failed to allocate %zu bytes for name %s\n", len, new_name);
            }
        }
    }

  public:
    // Standardkonstruktor
    ps_array3d() = default;

    // Konstruktor mit Namen
    explicit ps_array3d(const char* name_str) { set_name(name_str); }

    // Destruktor
    ~ps_array3d() {
        if (mem) {
            log_w("Destructor called for %s: Freeing %zu bytes at %p", name ? name : "unnamed", dim1 * dim2 * dim3 * sizeof(T), mem.get());
        } else {
            log_w("Destructor called for %s: No memory to free.", name ? name : "unnamed");
        }
        if (name) {
            free(name);
            name = nullptr;
        }
    }

    // Move-Konstruktor
    ps_array3d(ps_array3d&& other) noexcept {
        mem = std::move(other.mem);
        dim1 = other.dim1;
        dim2 = other.dim2;
        dim3 = other.dim3;
        name = other.name;
        other.dim1 = 0;
        other.dim2 = 0;
        other.dim3 = 0;
        other.name = nullptr;
    }

    // Move-Assignment-Operator
    ps_array3d& operator=(ps_array3d&& other) noexcept {
        if (this != &other) {
            if (name) {
                free(name);
                name = nullptr;
            }
            mem = std::move(other.mem);
            dim1 = other.dim1;
            dim2 = other.dim2;
            dim3 = other.dim3;
            name = other.name;
            other.dim1 = 0;
            other.dim2 = 0;
            other.dim3 = 0;
            other.name = nullptr;
        }
        return *this;
    }

    ps_array3d(const ps_array3d&) = delete;
    ps_array3d& operator=(const ps_array3d&) = delete;

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  A L L O C  📌📌📌
    void alloc(size_t d1, size_t d2, size_t d3, const char* alloc_name = nullptr, bool usePSRAM = true) {
        reset();
        if (alloc_name) { set_name(alloc_name); }
        dim1 = d1;
        dim2 = d2;
        dim3 = d3;
        size_t total_size = dim1 * dim2 * dim3 * sizeof(T);
        total_size = (total_size + 15) & ~15; // Align to 16 bytes
        if (psramFound() && usePSRAM) {
            mem.reset(static_cast<T*>(ps_malloc(total_size)));
        } else {
            mem.reset(static_cast<T*>(malloc(total_size)));
        }
        if (!mem) {
            printf("OOM: failed to allocate %zu bytes for %s\n", total_size, name ? name : (alloc_name ? alloc_name : "unnamed"));
            dim1 = 0;
            dim2 = 0;
            dim3 = 0;
        }
    }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C A L L O C  📌📌📌
    void calloc(size_t d1, size_t d2, size_t d3, const char* alloc_name = nullptr, bool usePSRAM = true) {
        reset();
        if (alloc_name) { set_name(alloc_name); }
        dim1 = d1;
        dim2 = d2;
        dim3 = d3;
        size_t total_size = dim1 * dim2 * dim3 * sizeof(T);
        total_size = (total_size + 15) & ~15; // Align to 16 bytes
        void* raw_mem = nullptr;
        if (psramFound() && usePSRAM) {
            raw_mem = ps_calloc(1, total_size);
        } else {
            raw_mem = calloc(1, total_size);
        }
        if (raw_mem) {
            mem.reset(static_cast<T*>(raw_mem));
        } else {
            printf("OOM: failed to allocate %zu bytes for %s\n", total_size, name ? name : (alloc_name ? alloc_name : "unnamed"));
            dim1 = 0;
            dim2 = 0;
            dim3 = 0;
        }
    }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  R E S E T   📌📌📌
    void reset() {
        mem.reset();
        dim1 = 0;
        dim2 = 0;
        dim3 = 0;
        // Name wird nicht freigegeben, bleibt bis zum Destruktor erhalten
    }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  O P E R A T O R []   📌📌📌
    class Proxy {
        T*     ptr;
        size_t cols, depth;

      public:
        Proxy(T* p, size_t c, size_t d) : ptr(p), cols(c), depth(d) {}
        T*       operator[](size_t col) { return ptr + col * depth; }
        const T* operator[](size_t col) const { return ptr + col * depth; }
    };

    Proxy operator[](size_t d1) { return Proxy(mem.get() + d1 * dim2 * dim3, dim2, dim3); }

    const Proxy operator[](size_t d1) const { return Proxy(mem.get() + d1 * dim2 * dim3, dim2, dim3); }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  G E T   📌📌📌
    T* get() { return mem.get(); }

    const T* get() const { return mem.get(); }

    // —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  V A L I D / S I Z E   📌📌📌
    bool   valid() const { return mem.get() != nullptr; }
    size_t get_dim1() const { return dim1; }
    size_t get_dim2() const { return dim2; }
    size_t get_dim3() const { return dim3; }
};
#endif