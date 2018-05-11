#ifndef __ADPCM_H_
#define __ADPCM_H_

#define WORD uint16_t
#define DWORD uint32_t

typedef struct tWAVEFORMATEX
{
    uint16_t    wFormatTag;        
    uint16_t    nChannels;         
    uint32_t    nSamplesPerSec;    
    uint32_t    nAvgBytesPerSec;   
    uint16_t    nBlockAlign;       
    uint16_t    wBitsPerSample;    
    uint16_t    cbSize;            
} WAVEFORMATEX;


typedef struct adpcmcoef_tag {
        short   iCoef1;
        short   iCoef2;
} ADPCMCOEFSET;


typedef struct adpcmwaveformat_tag {
        WAVEFORMATEX    wfx;
        uint16_t        wSamplesPerBlock;
        uint16_t        wNumCoef;
        ADPCMCOEFSET    aCoef[1];
} ADPCMWAVEFORMAT;

#endif //__ADPCM_H_