// anp59 (Mrz 2021)
// Vereinfachte Einbindung verschiedener SD Libraries in bestehende Arduino Projekte

#ifndef __SD_LIBS_H__
#define __SD_LIBS_H__

// global define SD (0), SD_MMC (1), SPIFFS (2), SDFAT (3)
#define SD_IMPL 3

// for SD_MMC (1) or SPIFFS (2) code adjustments are necessary (Audio-Lib, main)
#if   (SD_IMPL == 0)        // default
    #include "SD.h"
#elif (SD_IMPL == 1) 
    // requires different GPIO for MMC
    #include "SD_MMC.h"
    #define SD SD_MMC
#elif (SD_IMPL == 2)
    #include "SPIFFS.h"
    #include "FFat.h"
    #define SD SPIFFS
#elif (SD_IMPL == 3) 
    #include "SdFat.h"
    // set SDFAT_FILE_TYPE in SdFatConfig.h
    
    #if SDFAT_FILE_TYPE == 1
        typedef File32 File;
    #elif SDFAT_FILE_TYPE == 2
        typedef ExFile File;
    #elif SDFAT_FILE_TYPE == 3
        typedef FsFile File;
    #endif

    namespace fs {
        
        class FS : public SdFat {
        public: 
            bool begin(SdCsPin_t csPin = SS, uint32_t maxSck = SD_SCK_MHZ(25)) { return SdFat::begin(csPin, maxSck); } 
        };
    
        class SDFATFS : public fs::FS {
        public:
            // sdcard_type_t cardType();
            uint64_t cardSize() {
                return totalBytes();
            }
            uint64_t usedBytes() {
                // set SdFatConfig MAINTAIN_FREE_CLUSTER_COUNT non-zero. Then only the first call will take time.
                return (uint64_t)(clusterCount() - freeClusterCount()) * (uint64_t)bytesPerCluster();
            }
            uint64_t totalBytes() {
                return (uint64_t)clusterCount() * (uint64_t)bytesPerCluster();
            }        
        };
    } 
        
    extern  fs::SDFATFS SD_SDFAT;

    using namespace fs;
    #define SDFATFS_USED
    #define SD SD_SDFAT
#endif  // SD_IMPL
#endif  // __SD_LIBS_H__

