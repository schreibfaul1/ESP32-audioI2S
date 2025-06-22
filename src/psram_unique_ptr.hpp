#pragma once

#include "Arduino.h"
#include <memory>
#include <cstring>
#include <cstdio>
#include <utility>
#include <algorithm>
#include <type_traits>

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
    // std::unique_ptr<void, PsramDeleter> mem;
    std::unique_ptr<T[], PsramDeleter> mem;
    size_t allocated_size = 0;
    static inline T dummy{}; // For invalid accesses

public:
    ps_ptr() = default;

    ps_ptr(ps_ptr&& other) noexcept { // move-constructor
        mem = std::move(other.mem);
        allocated_size = other.allocated_size;
        other.allocated_size = 0;
    }

    // Optional: Explicitly prohibit copy constructor (helpful in troubleshooting)
    ps_ptr(const ps_ptr&) = delete;
    ps_ptr& operator=(const ps_ptr&) = delete;


    //~ps_ptr() {
    //    if(mem) {
    //        log_v("Freeing %zu bytes", allocated_size);
    //    }
    //}
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

    void alloc(std::size_t size, const char* name = nullptr) {
        size = (size + 15) & ~15; // Align to 16 bytes
        // mem.reset(ps_malloc(size));
        mem.reset(static_cast<T*>(ps_malloc(size)));  // <--- Wichtig!
        allocated_size = size;
        if (!mem) {
            printf("OOM: failed to allocate %zu bytes for %s\n", size, name ? name : "unnamed");
        }
    }
    // Within the class ps_ptr<T>
    void alloc() {
        alloc(sizeof(T));
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  A L L O C _ A R R A Y   📌📌📌

    void alloc_array(std::size_t count, const char* name = nullptr) {
        alloc(sizeof(T) * count, name);
        clear();
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  R E A L L O C  📌📌📌

    // test.realloc(200, "test"); // It is not necessary to specify a name, only for debugging OOM etc.
    // printf("new size:  %i\n", test.size());
    // printf("%s\n", test.get());

    void realloc(size_t new_size, const char* name = nullptr) {
        void* new_mem = ps_malloc(new_size);
        if (!new_mem) {
            printf("OOM: failed to realloc %zu bytes for %s\n", new_size, name ? name : "unnamed");
            return;
        }

        if (mem && allocated_size > 0) {
            std::memcpy(new_mem, mem.get(), std::min(allocated_size, new_size));
        }

        mem.reset(new_mem);
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
        std::size_t bytes = count * sizeof(T);
        alloc(bytes, name);
        if (mem && src) {
            std::memcpy(mem.get(), src, bytes);
        }
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  C L O N E _ F R O M  📌📌📌

    // ps_ptr<char> source;
    // source.assign("Hello World");
    // ps_ptr<char> copy;
    // copy.clone_from(source);
    // printf("%s\n", copy.get());  // → Hello World

    void clone_from(const ps_ptr<T>& other, const char* name = nullptr) {
        if (!other.valid()) {
            reset();
            return;
        }

        std::size_t sz = other.size();
        alloc(sz, name);
        if (mem) {
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

        // Neu allokieren
        // mem.reset(ps_malloc(new_len));
        mem.reset(static_cast<T*>(ps_malloc(new_len)));  // <--- Wichtig!
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
    // 📌📌📌  F O R M A T   📌📌📌

    // ps_ptr<char> greeting;
    // greeting.format("Value: %d, Text: %s", 42, "Hello"); // sprintf
    // printf("%s\n", greeting.get());  // → Wert: 42, Text: Hello

    template <typename U = T>
    requires std::is_same_v<U, char>
    void format(const char* fmt, ...) {
        if (!fmt) return;

        va_list args;
        va_start(args, fmt);

        // Erstmal herausfinden, wie groß der Formatstring ist
        va_list args_copy;
        va_copy(args_copy, args);
        int len = vsnprintf(nullptr, 0, fmt, args_copy);
        va_end(args_copy);

        if (len < 0) {
            va_end(args);
            return;
        }

        // +1 für null-Terminator
        std::size_t buf_size = static_cast<std::size_t>(len) + 1;
        alloc(buf_size, "formatted_string");

        if (mem) {
            vsnprintf(static_cast<char*>(mem.get()), buf_size, fmt, args);
        }

        va_end(args);
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
        mem.reset(ps_malloc(new_len));
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
    // 📌📌📌  G E T   📌📌📌
    T* get() const { return static_cast<T*>(mem.get()); }
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
    // 📌📌📌  R E L E A S E   📌📌📌
    T* release() {
        T* ptr = get();        // aktuellen Zeiger sichern
        mem.release();         // unique_ptr gibt den Zeiger frei und setzt sich auf nullptr
        allocated_size = 0;    // optional: Größe zurücksetzen
        return ptr;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌   M O V E    📌📌📌

    // ps_ptr<int32_t> a;
    // ps_ptr<int32_t> b;
    //
    // b.alloc(128);// allocate mem
    //
    // // Move semantics (no copy of the content!)
    // a = std::move(b);  // 👉 Call your Move Assignment operator
    ps_ptr& operator=(ps_ptr&& other) noexcept {
        if (this != &other) {
            mem = std::move(other.mem);
            allocated_size = other.allocated_size;
            other.allocated_size = 0;
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

    // ps_ptr<char> mediaSeqStr = "227213779";
    // uint64_t mediaSeq = mediaSeqStr.to_uint64();
    //
    // ps_ptr<char> addr = "0x1A3B";
    // uint64_t val = addr.to_uint64(16);

    template <typename U = T>
    requires std::is_same_v<U, char>
    uint64_t to_uint64(int base = 10) const {
        if (!mem || !get()) return 0;

        char* end = nullptr;
        uint64_t result = std::strtoull(get(), &end, base);
        if (end == get()) {
            log_e("no numerical value found in %s", get());
            return 0;
        }
        return result;
    }
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  R E P L A C E   📌📌📌

    // ps_ptr<char> path;
    // path.assign("/user/temp/file.tmp");
    // path.replace("temp", "music");
    // result: "/user/music/file.tmp"

    bool replace(const char* from, const char* to) {
        if (!valid() || !from || !to || !*from) return false;

        const char* src = get();
        std::size_t fromLen = std::strlen(from);
        std::size_t toLen = std::strlen(to);

        // Count how often `from' occurs
        std::size_t count = 0;
        const char* p = src;
        while ((p = std::strstr(p, from)) != nullptr) {
            count++;
            p += fromLen;
        }

        if (count == 0) return false;

        // Calculate length
        std::size_t newLen = std::strlen(src) + (toLen - fromLen) * count + 1;
        ps_ptr<char> result;
        result.alloc(newLen);
        if (!result.valid()) return false;

        // Substitute
        const char* read = src;
        char* write = result.get();
        while (*read) {
            const char* pos = std::strstr(read, from);
            if (pos) {
                std::size_t bytes = pos - read;
                std::memcpy(write, read, bytes);
                write += bytes;
                std::memcpy(write, to, toLen);
                write += toLen;
                read = pos + fromLen;
            } else {
                std::strcpy(write, read);
                break;
            }
        }

        this->copy_from(result.get(), result.strlen());
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
};
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
    // 📌📌📌  S T R U C T U R E S    📌📌📌
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

    // This declaration must be in the header or in the global area
    // typedef struct _s {
    //     int a[100];
    //     char* b;
    //     char* c;
    // } myS;
    // // Important macro to release the strings
    // PS_STRUCT_FREE_MEMBERS(myS, ptr->b, ptr->c)


    // ps_struct_ptr<myS> mySt;
    // mySt.alloc("myS");
    // mySt->a[5] = 123;
    // mySt.set_ptr_field(&mySt->b, "Hallo");
    // mySt.set_ptr_field(&mySt->c, "Welt");

    // printf("a[5]=%d, b=%s, c=%s\n", mySt->a[5], mySt->b, mySt->c);

    // // Automatisch: b und c werden bei reset()/Destruktor freigegeben
    // mySt.reset();

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
            char* p = static_cast<char*>(ps_malloc(len));
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

protected:
    // Wird über Makro PS_STRUCT_FREE_MEMBERS spezialisiert
    inline void free_all_ptr_members() {}
};



// ——————————————————————————————————————————————————————————————
// Macro for the declaration of releasing fields for a structure

#define PS_STRUCT_FREE_MEMBERS(TYPE, ...) \
    template <> \
    inline void ps_struct_ptr<TYPE>::free_all_ptr_members() { \
        auto ptr = get(); \
        free_fields(__VA_ARGS__); \
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
