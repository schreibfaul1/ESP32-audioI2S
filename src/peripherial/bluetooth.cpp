
#include "bluetooth.h"

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————

BT_Source* BT_Source::s_instance = nullptr;

// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
// 📌📌📌  B L U E T O O T H  📌📌📌
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool BT_Source::init(ps_ptr<char> devName) {
    if (bt_samplesBuffer.isInitialized()) { bt_samplesBuffer.reset(); }
    m_devName = devName;
    bt_samplesBuffer.setBufsize(m_bt_buffSize);
    size_t buffSize = bt_samplesBuffer.init();
    if (buffSize == m_bt_buffSize) {
        m_workBuff.alloc_array(1024);
        s_instance = this;
        a2dp_source.set_data_callback(bt_data_callback);
        a2dp_source.start(m_devName.c_get());
        m_isInit = true;
        return m_isInit;
    }
    return false;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
void BT_Source::reset() {
    a2dp_source.end();
    if (bt_samplesBuffer.isInitialized()) bt_samplesBuffer.reset();
    m_isInit = false;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
bool BT_Source::getChunk(int32_t* chunk, size_t size) {
    size_t written = 0;
    if (size <= bt_samplesBuffer.freeSpace()) {
        written = bt_samplesBuffer.write(chunk, size);
        if (written != size) {
            log_e("buffer write error size: %lu, written: %lu", size, written);
            return false;
        }
    } else {
        // log_e("not enough space, size %lu, free %lu", size, bt_samplesBuffer.freeSpace());
        return false;
    }
    return true;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
int32_t BT_Source::get_data(uint8_t* data, int32_t bytes) { // CallBack

    // Special case: Flush request
    if (bytes == -1) { return 0; }

    if (!data || bytes <= 0) return 0;

    size_t filled = bt_samplesBuffer.bufferFilled();
    size_t free = bt_samplesBuffer.freeSpace();
    size_t ws = bt_samplesBuffer.writeSpace();
    (void)filled;
    (void)free;
    (void)ws;
    // Two output bytes per int32_t sample
    // if (static_cast<size_t>(bytes) > available) {
    //     log_e("Not enough data: filled=%lu, requested=%li", static_cast<unsigned long>(available), static_cast<long>(bytes));
    //     return 0;
    // }
    size_t words = bt_samplesBuffer.peek_words(m_workBuff.get(), bytes / 2);
    //   log_e("words %lu, filled %lu, free %lu, ws %lu, bytes %lu", words, filled, free, ws, bytes);
    for (int i = 0; i < words; i++) {

        data[i * 2 + 1] = (m_workBuff[i] & 0xFF000000) >> 24;
        data[i * 2] = (m_workBuff[i] & 0x00FF0000) >> 16;
    }
    bt_samplesBuffer.wordsConsumed(words);
    return bytes;
}
// —————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————————
