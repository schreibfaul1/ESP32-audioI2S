#ifndef __SD_SDFAT_H__
#define __SD_SDFAT_H__

#include "SdFat.h"
// set SDFAT_FILE_TYPE in SdFatConfig.h
// set DESTRUCTOR_CLOSES_FILE in SdFat if necessary (line 257)

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

#endif  // __SD_SDFAT_H__