//**************************
//        D U M M Y
//**************************


#pragma once
#include <stdint.h>



void VORBISDecoder_FreeBuffers(){;}
int  VORBISDecoder_AllocateBuffers(){return 0;}
int  VORBISFindSyncWord(uint8_t* data, uint16_t len){return 0;}
int  VORBISGetSampRate(){return 0;}
int  VORBISGetBitsPerSample(){return 0;}
int  VORBISGetBitRate(){return 0;}
int  VORBISDecode(uint8_t* data, int* bytesLeft, int16_t* m_outBuff){return 0;}
int  VORBISGetOutputSamps(){return 0;}
char* VORBISgetStreamTitle(){return nullptr;}