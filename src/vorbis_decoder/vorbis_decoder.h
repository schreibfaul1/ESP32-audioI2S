//**************************
//        D U M M Y
//**************************


#pragma once
#include <stdint.h>

#define ERR_VORBIS_NONE 0
#define ERR_VORBIS_CHANNELS_OUT_OF_RANGE 1
#define ERR_VORBIS_INVALID_SAMPLERATE 2
#define ERR_VORBIS_EXTRA_CHANNELS_UNSUPPORTED 3
#define ERR_VORBIS_DECODER_ASYNC 4
#define ERR_VORBIS_OGG_SYNC_NOT_FOUND 5
#define ERR_VORBIS_BAD_HEADER 6
#define ERR_VORBIS_NOT_AUDIO 7
#define ERR_VORBIS_BAD_PACKET 8



void VORBISDecoder_FreeBuffers(){;}
int  VORBISDecoder_AllocateBuffers(){return 0;}
int  VORBISFindSyncWord(uint8_t* data, uint16_t len){return 0;}
int  VORBISGetSampRate(){return 0;}
int  VORBISGetBitsPerSample(){return 0;}
int  VORBISGetBitRate(){return 0;}
int  VORBISDecode(uint8_t* data, int* bytesLeft, int16_t* m_outBuff){return 0;}
int  VORBISGetOutputSamps(){return 0;}
char* VORBISgetStreamTitle(){return nullptr;}
int  VORBISGetChannels(){return 0;}