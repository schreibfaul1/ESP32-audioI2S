#pragma once

#include <Arduino.h>
#include "psram_unique_ptr.hpp"

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  A U D I O B U F F E R  📌📌📌
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

// Main buffer:
// +-----------------------------------------------------------+

// Reserve buffer:
// +-------------------------+

// The reserve buffer always contains a copy of the first
// reserveSize bytes of the main buffer.

// This guarantees that every decoder receives one contiguous
// memory block even if reading crosses the end of the main
// buffer.

// Only one memcpy() is required per wrap.
//
//  m_bufferBegin     m_readPtr                  m_writePtr
//   |                  |<------maxAvailableBytes----->|<--------------------- writeSpace ----------------------->|
//   ▼                  ▼                              ▼                                                          ▼
//   ---------------------------------------------------------------------------------------------------------------
//   |                           <--m_mainSize-->                                 |      <--m_reserveSize -->     |
//   ---------------------------------------------------------------------------------------------------------------
//   |<---freeSpace---->|<------------filled---------->|<-------freeSpace-------->|
//
//
//
//  m_bufferBegin                  m_writePtr                 m_readPtr
//   |                                 |<-------writeSpace------>|<---------------maxAvailableBytes-------------->|
//   ▼                                 ▼                         ▼                ▼                               ▼
//   ---------------------------------------------------------------------------------------------------------------
//   |                          <--m_mainSize-->                                  |      <--m_reserveSize -->     |
//   ---------------------------------------------------------------------------------------------------------------
//   |<------------filled------------->|<-------freeSpace------->|<----filled---->|
//
//

//----------------------------------------------------------------------------------------------------------------------

//  The reserve area contains a copy of the first bytes of the main buffer.
//  Therefore every decoder always receives one contiguous memory block, even if the read position crosses the end of the main buffer.
//
//  Only one memcpy() is required during wrap-around.
//
//  Writer:
//      WiFi / SD
//
//  Reader:
//      Audio decoder
//
//  Reader and writer may work simultaneously.
// ——————————————————————————————————————————————————————————————————————————————

class AudioBuffer {
#define ANSI_ESC_RED   "\033[31m"
#define ANSI_ESC_RESET "\033[0m"
  public:
    // ------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------

    static constexpr size_t DEFAULT_MAIN_BUFFER_SIZE = UINT16_MAX * 10;
    static constexpr size_t DEFAULT_RESERVE_BUFFER_SIZE = UINT16_MAX;
    // Maximum contiguous block returned by readSpace()/writeSpace().
    // All supported decoders use uint16_t (bytesLeft, bytesAvail, ...). Returning larger blocks has no benefit and would only increase decoder latency.
    static constexpr size_t MAX_TRANSFER_SIZE = UINT16_MAX;

    // ------------------------------------------------------------
    // Construction
    // ------------------------------------------------------------

    AudioBuffer() : AudioBuffer(DEFAULT_MAIN_BUFFER_SIZE, DEFAULT_RESERVE_BUFFER_SIZE) {}
    AudioBuffer(size_t mainSize, size_t reserveSize) : m_mainSize(mainSize), m_reserveSize(reserveSize), m_totalSize(mainSize + reserveSize) {}

    ~AudioBuffer() {}

    // ------------------------------------------------------------
    // Initialization
    // ------------------------------------------------------------

    size_t init() {
        if (m_initialized) return m_mainSize;
        m_buffer.alloc(m_totalSize, "AudioBuffer");
        if (!m_buffer) return 0;
        m_mutex = xSemaphoreCreateBinary();
        if (!m_mutex) {
            m_buffer.reset();
            return 0;
        }
        xSemaphoreGive(m_mutex);
        m_log.set_name("\nAudioBuffer_Log");
        m_bufferBegin = m_buffer.get();
        m_mainEnd = m_bufferBegin + m_mainSize;
        m_bufferEnd = m_bufferBegin + m_totalSize;
        reset();
        m_initialized = true;
        return m_mainSize;
    }

    bool isInitialized() const { return m_initialized; }

    void reset() {

        m_readPtr = m_bufferBegin;
        m_writePtr = m_bufferBegin;

        m_isEmpty = true;
        m_isFull = false;

        m_readSpace = 0;
        m_writeSpace = std::min(MAX_TRANSFER_SIZE, m_mainSize);
    }

    // ------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------

    void setMaxBlocksize(size_t size) { m_maxBlockSize = size; }

    size_t getMaxBlockSize() const { return m_maxBlockSize; }

    size_t getBufsize() const { return m_mainSize; }

    // ------------------------------------------------------------
    // Buffer information
    // ------------------------------------------------------------

    size_t bufferFilled() {
        if (!m_initialized) return 0;

        xSemaphoreTake(m_mutex, portMAX_DELAY);
        size_t filled = 0;

        if (m_readPtr == m_writePtr) {

            if (m_isEmpty) {
                filled = 0;
                goto exit;
            }

            if (m_isFull) {
                filled = m_mainSize;
                goto exit;
            }

            // should never happen
            m_log.assignf(ANSI_ESC_RED "[{}:{}] writePtr == readPtr, invalid state" ANSI_ESC_RESET, __FILE__, __LINE__);
            m_log.println();
            goto exit;
        }

        if (m_writePtr > m_readPtr) {
            filled = m_writePtr - m_readPtr;
        } else {
            filled = (m_mainEnd - m_readPtr) + (m_writePtr - m_bufferBegin);
        }

    exit:

        xSemaphoreGive(m_mutex);

        return filled;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    size_t freeSpace() {
        if (!m_initialized) return 0;

        xSemaphoreTake(m_mutex, portMAX_DELAY);

        size_t free = 0;

        if (m_readPtr == m_writePtr) {

            if (m_isEmpty) {
                free = m_mainSize;
                goto exit;
            }

            if (m_isFull) {
                free = 0;
                goto exit;
            }

            m_log.assignf(ANSI_ESC_RED "[{}:{}] writePtr == readPtr, invalid state" ANSI_ESC_RESET, __FILE__, __LINE__);
            m_log.println();
            goto exit;
        }

        if (m_readPtr > m_writePtr) {
            free = m_readPtr - m_writePtr;
        } else {
            free = (m_mainEnd - m_writePtr) + (m_readPtr - m_bufferBegin);
        }

    exit:
        xSemaphoreGive(m_mutex);
        return free;
    }
    size_t readSpace() {
        if (!m_initialized) return 0;

        xSemaphoreTake(m_mutex, portMAX_DELAY);

        //------------------------------------------------------------
        // Reader reached reserve area?
        // Move it back into the beginning of the main buffer.
        //------------------------------------------------------------

        if (m_readPtr >= m_mainEnd && m_writePtr <= m_mainEnd) {
            const size_t offset = m_readPtr - m_mainEnd;

            if (m_writePtr > (m_bufferBegin + offset)) { m_readPtr = m_bufferBegin + offset; }
        }

        m_readSpace = 0;

        //------------------------------------------------------------
        // Empty buffer?
        //------------------------------------------------------------

        if (m_isEmpty) goto exit;

        //------------------------------------------------------------
        // Buffer completely full.
        //------------------------------------------------------------

        if (m_isFull) {
            m_readSpace = std::min(MAX_TRANSFER_SIZE, (size_t)(m_bufferEnd - m_readPtr));
            goto exit;
        }

        //------------------------------------------------------------
        // Writer is ahead.
        //------------------------------------------------------------

        if (m_writePtr > m_readPtr) {
            m_readSpace = std::min(MAX_TRANSFER_SIZE, (size_t)(m_writePtr - m_readPtr));
            goto exit;
        }

        //------------------------------------------------------------
        // Writer wrapped around.
        //------------------------------------------------------------

        m_readSpace = std::min(MAX_TRANSFER_SIZE, (size_t)(m_bufferEnd - m_readPtr));

    exit:
        xSemaphoreGive(m_mutex);
        return m_readSpace;
    }

    size_t writeSpace() {
        if (!m_initialized) return 0;
        m_writeSpace = 0;

        xSemaphoreTake(m_mutex, portMAX_DELAY);

        //------------------------------------------------------------
        // Space until physical buffer end
        //------------------------------------------------------------

        size_t spaceToEnd = m_bufferEnd - m_writePtr;

        //------------------------------------------------------------
        // Buffer full?
        //------------------------------------------------------------

        if (m_isFull) goto exit;

        //------------------------------------------------------------
        // Empty buffer
        //------------------------------------------------------------

        if (m_isEmpty) {
            m_writeSpace = std::min(MAX_TRANSFER_SIZE, spaceToEnd);
            goto exit;
        }

        //------------------------------------------------------------
        // Reserve buffer completely written?
        //------------------------------------------------------------

        if (spaceToEnd == 0) {
            // Copy reserve area to beginning only if the reader
            // has already released these bytes.

            if (m_readPtr > (m_bufferBegin + m_reserveSize)) {
                memcpy(m_bufferBegin, m_mainEnd, m_reserveSize);
                m_writePtr = m_bufferBegin + m_reserveSize;
            }
            spaceToEnd = m_bufferEnd - m_writePtr;
        }

        //------------------------------------------------------------
        // Writer before reader
        //------------------------------------------------------------

        if (m_writePtr < m_readPtr) {

            // Reader is still inside reserve area.
            // Writer must not enter reserve.

            if (m_readPtr >= m_mainEnd) {
                m_writeSpace = std::min(MAX_TRANSFER_SIZE, (size_t)(m_mainEnd - m_writePtr));
            } else {
                m_writeSpace = std::min(MAX_TRANSFER_SIZE, (size_t)(m_readPtr - m_writePtr));
            }
            goto exit;
        }

        //------------------------------------------------------------
        // Writer behind reader
        //------------------------------------------------------------

        m_writeSpace = std::min(MAX_TRANSFER_SIZE, spaceToEnd);
    exit:
        xSemaphoreGive(m_mutex);
        return m_writeSpace;
    }

    // ------------------------------------------------------------
    // Pointer access
    // ------------------------------------------------------------

    uint8_t* getReadPtr() { return m_readPtr; }   // Pointer remains valid until the next call to bytesWasRead() or reset().
    uint8_t* getWritePtr() { return m_writePtr; } // Pointer remains valid until the next call to bytesWritten() or reset().

    // ------------------------------------------------------------
    // Update pointers
    // ------------------------------------------------------------

    void bytesWritten(size_t bytes) {
        if (!m_initialized) return;

        if (!bytes) return;
        xSemaphoreTake(m_mutex, portMAX_DELAY);

        //------------------------------------------------------------
        // The caller must never report more bytes than previously
        // returned by writeSpace().
        //------------------------------------------------------------

        if (bytes > m_writeSpace) {
            m_log.assignf(ANSI_ESC_RED "[{}:{}] writeSpace {}, bytes {}" ANSI_ESC_RESET, __FILE__, __LINE__, m_writeSpace, bytes);
            m_log.println();
            bytes = m_writeSpace;
        }

        //------------------------------------------------------------
        // Prevent writer from overtaking reader.
        //------------------------------------------------------------

        if (m_writePtr < m_readPtr) {
            if (m_writePtr + bytes > m_readPtr) {
                m_log.assignf(ANSI_ESC_RED "[{}:{}] writePtr overruns readPtr "
                                           "(write {}, read {}, bytes {})" ANSI_ESC_RESET,
                              __FILE__, __LINE__, m_writePtr - m_bufferBegin, m_readPtr - m_bufferBegin, bytes);
                m_log.println();
                bytes = m_readPtr - m_writePtr;
            }
        }

        //------------------------------------------------------------
        // Never write behind the physical buffer.
        //------------------------------------------------------------

        if (m_writePtr + bytes > m_bufferEnd) {
            m_log.assignf(ANSI_ESC_RED "[{}:{}] writePtr overruns bufferEnd "
                                       "(write {}, end {}, bytes {})" ANSI_ESC_RESET,
                          __FILE__, __LINE__, m_writePtr - m_bufferBegin, m_bufferEnd - m_bufferBegin, bytes);

            m_log.println();
            bytes = m_bufferEnd - m_writePtr;
        }

        //------------------------------------------------------------
        // Advance write pointer.
        //------------------------------------------------------------

        m_writePtr += bytes;

        //------------------------------------------------------------
        // Update state flags.
        //------------------------------------------------------------

        m_isEmpty = false;
        if (m_writePtr == m_readPtr) m_isFull = true;
        xSemaphoreGive(m_mutex);

#ifdef DEBUG
        sanityCheck();
#endif
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    void bytesWasRead(size_t bytes) {
        if (!m_initialized) return;

        if (!bytes) return;

        xSemaphoreTake(m_mutex, portMAX_DELAY);

        //------------------------------------------------------------
        // The caller must never report more bytes than previously
        // m_readSpace contains.
        //------------------------------------------------------------

        if (bytes > m_readSpace) {
            m_log.assignf(ANSI_ESC_RED "[{}:{}] readSpace {}, bytes {}" ANSI_ESC_RESET, __FILE__, __LINE__, m_readSpace, bytes);
            m_log.println();
            bytes = m_readSpace;
        }

        //------------------------------------------------------------
        // Prevent reader from overtaking writer.
        //------------------------------------------------------------

        if (m_readPtr < m_writePtr) {
            if (m_readPtr + bytes > m_writePtr) {
                m_log.assignf(ANSI_ESC_RED "[{}:{}] readPtr overruns writePtr "
                                           "(read {}, write {}, bytes {})" ANSI_ESC_RESET,
                              __FILE__, __LINE__, m_readPtr - m_bufferBegin, m_writePtr - m_bufferBegin, bytes);

                m_log.println();
                bytes = m_writePtr - m_readPtr;
            }
        }

        //------------------------------------------------------------
        // Never read behind the physical buffer.
        //------------------------------------------------------------

        if (m_readPtr + bytes > m_bufferEnd) {

            m_log.assignf(ANSI_ESC_RED "[{}:{}] readPtr overruns bufferEnd "
                                       "(read {}, end {}, bytes {})" ANSI_ESC_RESET,
                          __FILE__, __LINE__, m_readPtr - m_bufferBegin, m_bufferEnd - m_bufferBegin, bytes);

            m_log.println();
            bytes = m_bufferEnd - m_readPtr;
        }

        //------------------------------------------------------------
        // Advance read pointer.
        //------------------------------------------------------------

        m_readPtr += bytes;

        //------------------------------------------------------------
        // Update state flags.
        //------------------------------------------------------------

        m_isFull = false;
        if (m_readPtr == m_writePtr) m_isEmpty = true;
        xSemaphoreGive(m_mutex);

#ifdef DEBUG
        sanityCheck();
#endif
    }

    // ------------------------------------------------------------
    // Debug
    // ------------------------------------------------------------

    void showStatus() {
        m_log.assignf("\nfilled {}, free {}\n", bufferFilled(), freeSpace());
        m_log.appendf("writeSpace {}, readSpace {}\n", writeSpace(), readSpace());
        m_log.appendf("writePtr {}, readPtr {}\n", m_writePtr - m_bufferBegin, m_readPtr - m_bufferBegin);
        m_log.appendf("isEmpty {}, isFull {}\n\n", m_isEmpty, m_isFull);
        m_log.print();
    }

  protected:
    //-------------------------------------------------------------
    // Buffer configuration
    //-------------------------------------------------------------

    size_t m_mainSize;
    size_t m_reserveSize;
    size_t m_totalSize;
    size_t m_maxBlockSize = 0;
    size_t m_readSpace = 0;
    size_t m_writeSpace = 0;

    //-------------------------------------------------------------
    // Memory
    //-------------------------------------------------------------

    ps_ptr<uint8_t> m_buffer;

    //-------------------------------------------------------------
    // Pointer layout
    //-------------------------------------------------------------

    uint8_t* m_bufferBegin = nullptr;
    uint8_t* m_mainEnd = nullptr;
    uint8_t* m_bufferEnd = nullptr;
    uint8_t* m_readPtr = nullptr;
    uint8_t* m_writePtr = nullptr;

    //-------------------------------------------------------------
    // State
    //-------------------------------------------------------------

    bool m_initialized = false;
    bool m_isEmpty = true;
    bool m_isFull = false;

    //-------------------------------------------------------------
    // Debug
    //-------------------------------------------------------------

    ps_ptr<char> m_log;

    //-------------------------------------------------------------
    // Synchronisation
    //-------------------------------------------------------------

    SemaphoreHandle_t m_mutex = nullptr;

  private:
    //-------------------------------------------------------------
    // Internal helper
    //-------------------------------------------------------------
    void sanityCheck() {
        assert(m_bufferBegin);
        assert(m_mainEnd == m_bufferBegin + m_mainSize);
        assert(m_bufferEnd == m_mainEnd + m_reserveSize);
        assert(m_readPtr >= m_bufferBegin);
        assert(m_readPtr <= m_bufferEnd);
        assert(m_writePtr >= m_bufferBegin);
        assert(m_writePtr <= m_bufferEnd);
        assert(!(m_isEmpty && m_isFull));
        assert(m_readSpace <= MAX_TRANSFER_SIZE);
        assert(m_writeSpace <= MAX_TRANSFER_SIZE);
        if (m_readPtr == m_writePtr) { assert(m_isEmpty || m_isFull); }
    }
};

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  R I N G B U F F E R  📌📌📌
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

// RingBuffer will be allocated in PSRAM
//
//  start            m_readPtr                    m_writePtr                                                     end
//   |                  |<----------- m_used --------->|<--------------------- writeSpace ----------------------->|
//   ▼                  ▼                              ▼                                                          ▼
//   ---------------------------------------------------------------------------------------------------------------
//   |                                           <--m_bufSize-->                                                   |
//   ---------------------------------------------------------------------------------------------------------------
//   |<---freeSpace---->|<------------filled---------->|<-----------------------freeSpace------------------------->|
//
//
//----------------------------------------------------------------------------------------------------------------------------------------------------

class RingBuffer {

  private:
    ps_ptr<int32_t>      m_buffer;
    size_t               m_bufSize = 0;
    size_t               m_readIndex = 0;
    size_t               m_writeIndex = 0;
    size_t               m_used = 0;
    bool                 m_init = false;
    mutable ps_ptr<char> m_log;

  public:
    RingBuffer() {}

    ~RingBuffer() { m_buffer.reset(); }

    //----------------------------------------------------------------------------------------------------------------------------------------------------
    void setBufsize(size_t size) { m_bufSize = size; }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    size_t getBufsize() const { return m_bufSize; }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    size_t init() {
        if (!m_buffer.alloc_array(m_bufSize, "RingBuffer")) {
            m_init = false;
            return 0;
        }
        m_log.set_name("\nRingBuffer_Log");
        reset();
        m_init = true;
        return m_bufSize;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    void clear() {}
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    void reset() {
        m_readIndex = 0;
        m_writeIndex = 0;
        m_used = 0;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    bool isInitialized() { return m_init; };
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    size_t freeSpace() const {
        if (!m_init) return 0;
        return m_bufSize - m_used;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    size_t bufferFilled() const {
        if (!m_init) return 0;
        return m_used;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    size_t writeSpace() const {
        if (!m_init) {
            log_e("Buffer is not initialized");
            return 0;
        }
        if (m_used == m_bufSize) return 0;

        if (m_writeIndex >= m_readIndex) {
            size_t s = m_bufSize - m_writeIndex;
            if (m_readIndex == 0) return std::min(s, freeSpace());
            return s;
        }
        return m_readIndex - m_writeIndex;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    size_t readSpace() const {
        if (!m_init) return 0;
        if (m_used == 0) return 0;
        if (m_readIndex < m_writeIndex) return m_writeIndex - m_readIndex;
        return m_bufSize - m_readIndex;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    int32_t* getWritePtr() {
        if (!m_init) return nullptr;
        return m_buffer.get() + m_writeIndex;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    int32_t* getReadPtr() {
        if (!m_init) return nullptr;
        return m_buffer.get() + m_readIndex;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    bool bytesWritten(size_t words) {

        if (!m_init) return false;

        if (words > freeSpace()) {
            m_log.assignf("bytesWritten({}) > freeSpace({})", words, freeSpace());
            m_log.println();
            return false;
        }
        m_writeIndex = advanceIndex(m_writeIndex, words);
        m_used += words;
        return true;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    bool wordsConsumed(size_t words) {

        if (!m_init) return false;

        if (words > bufferFilled()) {
            m_log.assignf("bytesRead({}) > bufferFilled({})", words, bufferFilled());
            m_log.println();
            return false;
        }
        m_readIndex = advanceIndex(m_readIndex, words);
        m_used -= words;
        return true;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    size_t write(const int32_t* src, size_t words) {

        if (!m_init || !src || words == 0) return 0;

        words = std::min(words, freeSpace());

        // Stereo-Frames nicht zerreißen
        words &= ~static_cast<size_t>(1);

        size_t written = 0;

        while (written < words) {
            size_t chunk = writeSpace();
            if (chunk == 0) {
                m_log.assign("RingBuffer: writeSpace()==0");
                m_log.println();
                break;
            }
            chunk = std::min(chunk, words - written);
            memcpy(getWritePtr(), src + written, chunk * sizeof(int32_t));
            bytesWritten(chunk);
            written += chunk;
        }
        return written;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    size_t read(int32_t* dst, size_t words) {

        if (!m_init || !dst || words == 0) return 0;

        // Nicht mehr lesen als vorhanden
        words = std::min(words, bufferFilled());

        // Stereo-Frames nicht zerreißen
        words &= ~static_cast<size_t>(1);

        size_t read = 0;

        while (read < words) {
            size_t chunk = readSpace();
            if (chunk == 0) {
                m_log.assign("RingBuffer: readSpace()==0");
                m_log.println();
                break;
            }
            chunk = std::min(chunk, words - read);
            memcpy(dst + read, getReadPtr(), chunk * sizeof(int32_t));
            wordsConsumed(chunk);
            read += chunk;
        }
        return read;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    size_t peek_words(int32_t* dst, size_t words) {

        if (!m_init || !dst || words == 0) return 0;

        words = std::min(words, bufferFilled());
        words &= ~static_cast<size_t>(1);

        size_t read = 0;
        size_t readIndex = m_readIndex; // lokale Kopie

        while (read < words) {
            size_t chunk;
            if (readIndex < m_writeIndex) {
                chunk = m_writeIndex - readIndex;
            } else {
                chunk = m_bufSize - readIndex;
                if (chunk == 0) {
                    m_log.assignf("RingBuffer: m_bufSize=readIndex, filled: {}", bufferFilled());
                    m_log.println();
                    break;
                }
            }
            chunk = std::min(chunk, words - read);
            memcpy(dst + read, m_buffer.get() + readIndex, chunk * sizeof(int32_t));
            read += chunk;
            readIndex += chunk;
            if (readIndex == m_bufSize) readIndex = 0;
        }
        return read;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    size_t peek_bytes(uint8_t* dst, size_t bytes) {

        if (!m_init || !dst || bytes == 0) return 0;

        // Always output full 16-bit values
        bytes &= ~static_cast<size_t>(1);

        // Two output bytes per int32_t sample
        size_t samples = std::min(bytes / 2, bufferFilled());

        size_t written = 0;
        size_t readIndex = m_readIndex;

        while (written < samples * 2) {
            size_t chunk;

            if (readIndex < m_writeIndex) {
                chunk = m_writeIndex - readIndex;
            } else {
                chunk = m_bufSize - readIndex;

                if (chunk == 0) {
                    m_log.assignf("RingBuffer: m_bufSize=readIndex, filled: {}", bufferFilled());
                    m_log.println();
                    break;
                }
            }

            chunk = std::min(chunk, samples - written / 2);

            const int32_t* src = reinterpret_cast<const int32_t*>(m_buffer.get() + readIndex);

            for (size_t i = 0; i < chunk; ++i) {
                uint32_t sample = static_cast<uint32_t>(src[i]);

                dst[written++] = static_cast<uint8_t>(sample >> 24);
                dst[written++] = static_cast<uint8_t>(sample >> 16);
            }

            readIndex += chunk;
            if (readIndex == m_bufSize) readIndex = 0;
        }

        return written;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    inline size_t advanceIndex(size_t index, size_t words) const {
        index += words;
        while (index >= m_bufSize) {
            index -= m_bufSize;
            //  printf("advanceIndex, index {}, words {}\n", index, words);
        }
        return index;
    }
    //----------------------------------------------------------------------------------------------------------------------------------------------------
    void showStatus() {
        m_log.assignf("\n"
                      "Initialized  : {}\n"
                      "BufferSize   : {}\n"
                      "Used         : {}\n"
                      "Free         : {}\n"
                      "ReadSpace    : {}\n"
                      "WriteSpace   : {}\n"
                      "ReadIndex    : {}\n"
                      "WriteIndex   : {}\n\n",
                      m_init, m_bufSize, m_used, freeSpace(), readSpace(), writeSpace(), m_readIndex, m_writeIndex);
        m_log.print();
    }
};
