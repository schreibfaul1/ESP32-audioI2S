#pragma once

#include "psram_unique_ptr.hpp"

class Audio {
  public:
    template <typename... Args> static void AUDIO_LOG_IMPL(Args&&...) {}
};
