#ifndef _XWB_H_
#define _XWB_H_

#include "adpcm.h"

#ifndef _countof
#define _countof(a) sizeof(a)/sizeof(a[0])
#endif

#define ADPCM_MINIWAVEFORMAT_BLOCKALIGN_CONVERSION_OFFSET 22

#define WAVEBANK_HEADER_SIGNATURE               'DNBW'      
#define WAVEBANK_HEADER_VERSION                 43          

#define WAVEBANK_BANKNAME_LENGTH                64          
#define WAVEBANK_ENTRYNAME_LENGTH               64          

#define WAVEBANK_MAX_DATA_SEGMENT_SIZE          0xFFFFFFFF  
#define WAVEBANK_MAX_COMPACT_DATA_SEGMENT_SIZE  0x001FFFFF  

typedef uint32_t WAVEBANKOFFSET;

#define WAVEBANK_TYPE_BUFFER         0x00000000
#define WAVEBANK_TYPE_STREAMING      0x00000001
#define WAVEBANK_TYPE_MASK           0x00000001

#define WAVEBANK_FLAGS_ENTRYNAMES    0x00010000
#define WAVEBANK_FLAGS_COMPACT       0x00020000
#define WAVEBANK_FLAGS_SYNC_DISABLED 0x00040000
#define WAVEBANK_FLAGS_SEEKTABLES    0x00080000
#define WAVEBANK_FLAGS_MASK          0x000F0000

#define WAVEBANKENTRY_FLAGS_READAHEAD       0x00000001
#define WAVEBANKENTRY_FLAGS_LOOPCACHE       0x00000002
#define WAVEBANKENTRY_FLAGS_REMOVELOOPTAIL  0x00000004
#define WAVEBANKENTRY_FLAGS_IGNORELOOP      0x00000008
#define WAVEBANKENTRY_FLAGS_MASK            0x00000008

#define WAVEBANKMINIFORMAT_TAG_PCM      0x0
#define WAVEBANKMINIFORMAT_TAG_XMA      0x1
#define WAVEBANKMINIFORMAT_TAG_ADPCM    0x2
#define WAVEBANKMINIFORMAT_TAG_WMA      0x3

#define WAVEBANKMINIFORMAT_BITDEPTH_8   0x0
#define WAVEBANKMINIFORMAT_BITDEPTH_16  0x1

#define WAVEBANKENTRY_XMASTREAMS_MAX          3
#define WAVEBANKENTRY_XMACHANNELS_MAX         6

#define WAVEBANK_DVD_SECTOR_SIZE    2048
#define WAVEBANK_DVD_BLOCK_SIZE     (WAVEBANK_DVD_SECTOR_SIZE * 16)

#define WAVEBANK_ALIGNMENT_MIN  4
#define WAVEBANK_ALIGNMENT_DVD  WAVEBANK_DVD_SECTOR_SIZE

typedef enum WAVEBANKSEGIDX {
    WAVEBANK_SEGIDX_BANKDATA = 0,
    WAVEBANK_SEGIDX_ENTRYMETADATA,
    WAVEBANK_SEGIDX_SEEKTABLES,
    WAVEBANK_SEGIDX_ENTRYNAMES,
    WAVEBANK_SEGIDX_ENTRYWAVEDATA,
    WAVEBANK_SEGIDX_COUNT
} WAVEBANKSEGIDX, *LPWAVEBANKSEGIDX;

#pragma pack(1)

typedef struct {
    uint32_t    dwOffset;
    uint32_t    dwLength;
} WAVEBANKREGION;

typedef struct {
    char            dwSignature[4];
    uint32_t        dwVersion;
    uint32_t        dwHeaderVersion;
    WAVEBANKREGION  Segments[WAVEBANK_SEGIDX_COUNT];
} WAVEBANKHEADER;

typedef struct {
    uint32_t    dwStartSample;
    uint32_t    dwTotalSamples;
} WAVEBANKSAMPLEREGION;

union MINIWAVEFORMAT
{
    static const uint32_t TAG_PCM   = 0x0;
    static const uint32_t TAG_XMA   = 0x1;
    static const uint32_t TAG_ADPCM = 0x2;
    static const uint32_t TAG_WMA   = 0x3;

    static const uint32_t BITDEPTH_8 = 0x0;
    static const uint32_t BITDEPTH_16 = 0x1;

    static const size_t ADPCM_BLOCKALIGN_CONVERSION_OFFSET = 22;
    
    struct
    {
        uint32_t       wFormatTag      : 2;
        uint32_t       nChannels       : 3;
        uint32_t       nSamplesPerSec  : 18;
        uint32_t       wBlockAlign     : 8;
        uint32_t       wBitsPerSample  : 1;
    };

    uint32_t           dwValue;

    uint16_t BitsPerSample() const
    {
        if (wFormatTag == TAG_XMA)
            return 16; // XMA_OUTPUT_SAMPLE_BITS == 16
        if (wFormatTag == TAG_WMA)
            return 16;
        if (wFormatTag == TAG_ADPCM)
            return 4; // MSADPCM_BITS_PER_SAMPLE == 4

        // wFormatTag must be TAG_PCM (2 bits can only represent 4 different values)
        return (wBitsPerSample == BITDEPTH_16) ? 16 : 8;
    }

    uint32_t BlockAlign() const
    {
        switch (wFormatTag)
        {
        case TAG_PCM:
            return wBlockAlign;
            
        case TAG_XMA:
            return (nChannels * 16 / 8); // XMA_OUTPUT_SAMPLE_BITS = 16

        case TAG_ADPCM:
            return (wBlockAlign + ADPCM_BLOCKALIGN_CONVERSION_OFFSET) * nChannels;

        case TAG_WMA:
            {
                static const uint32_t aWMABlockAlign[] =
                {
                    929,
                    1487,
                    1280,
                    2230,
                    8917,
                    8192,
                    4459,
                    5945,
                    2304,
                    1536,
                    1485,
                    1008,
                    2731,
                    4096,
                    6827,
                    5462,
                    1280
                };

                uint32_t dwBlockAlignIndex = wBlockAlign & 0x1F;
                if ( dwBlockAlignIndex < _countof(aWMABlockAlign) )
                    return aWMABlockAlign[dwBlockAlignIndex];
            }
            break;
        }

        return 0;
    }

    uint32_t AvgBytesPerSec() const
    {
        switch (wFormatTag)
        {
        case TAG_PCM:
            return nSamplesPerSec * wBlockAlign;
            
        case TAG_XMA:
            return nSamplesPerSec * BlockAlign();

        case TAG_ADPCM:
            {
                uint32_t blockAlign = BlockAlign();
                uint32_t samplesPerAdpcmBlock = AdpcmSamplesPerBlock();
                return blockAlign * nSamplesPerSec / samplesPerAdpcmBlock;
            }
            break;

        case TAG_WMA:
            {
                static const uint32_t aWMAAvgBytesPerSec[] =
                {
                    12000,
                    24000,
                    4000,
                    6000,
                    8000,
                    20000,
                    2500
                };
                // bitrate = entry * 8

                uint32_t dwBytesPerSecIndex = wBlockAlign >> 5;
                if ( dwBytesPerSecIndex < _countof(aWMAAvgBytesPerSec) )
                    return aWMAAvgBytesPerSec[dwBytesPerSecIndex];
            }
            break;
        }

        return 0;
    }

    uint32_t AdpcmSamplesPerBlock() const
    {
        uint32_t nBlockAlign = (wBlockAlign + ADPCM_BLOCKALIGN_CONVERSION_OFFSET) * nChannels;
        return nBlockAlign * 2 / (uint32_t)nChannels - 12;
    }

    void AdpcmFillCoefficientTable(ADPCMWAVEFORMAT *fmt) const
    {
        // These are fixed since we are always using MS ADPCM
        fmt->wNumCoef = 7; /* MSADPCM_NUM_COEFFICIENTS */

        static ADPCMCOEFSET aCoef[7] = { { 256, 0}, {512, -256}, {0,0}, {192,64}, {240,0}, {460, -208}, {392,-232} };
        memcpy( &fmt->aCoef, aCoef, sizeof(aCoef) );
    }
};


typedef struct {
    union
    {
        struct
        {
            // Entry flags
            uint32_t                   dwFlags  :  4;

            // Duration of the wave, in units of one sample.
            // For instance, a ten second long wave sampled
            // at 48KHz would have a duration of 480,000.
            // This value is not affected by the number of
            // channels, the number of bits per sample, or the
            // compression format of the wave.
            uint32_t                   Duration : 28;
        };
        uint32_t dwFlagsAndDuration;
    };

    MINIWAVEFORMAT  Format;         // Entry format.
    WAVEBANKREGION          PlayRegion;     // Region within the wave data segment that contains this entry.
    WAVEBANKSAMPLEREGION    LoopRegion;     // Region within the wave data (in samples) that should loop.
} WAVEBANKENTRY;

typedef struct
{
    uint32_t       dwOffset            : 21;       // Data offset, in multiplies of the bank alignment
    uint32_t       dwLengthDeviation   : 11;       // Data length deviation, in bytes

} WAVEBANKENTRYCOMPACT;

typedef struct {
    uint32_t    dwFlags;                                // Bank flags
    uint32_t    dwEntryCount;                           // Number of entries in the bank
    char        szBankName[WAVEBANK_BANKNAME_LENGTH];   // Bank friendly name
    uint32_t    dwEntryMetaDataElementSize;             // Size of each entry meta-data element, in bytes
    uint32_t    dwEntryNameElementSize;                 // Size of each entry name element, in bytes
    uint32_t    dwAlignment;                            // Entry alignment, in bytes
    uint32_t    CompactFormat;                          // Format data for compact bank
    uint32_t    BuildTime;                              // Build timestamp
} WAVEBANKDATA;

typedef struct {
    uint32_t    sign;
    uint32_t    filesize;   // minus 8 bytes
    uint32_t    format;

    uint32_t    formatid;
    uint32_t    blocksize;
    uint16_t    audioformat;
    uint16_t    channels;
    uint32_t    rate;
    uint32_t    bytepersec;
    uint16_t    byteperblock;
    uint16_t    bitspersample;

    uint32_t    blockid;
    uint32_t    datasize;
} WAVHEADER_SIMPLE;

typedef struct {
    uint32_t    sign;
    uint32_t    filesize;   // minus 8 bytes
    uint32_t    format;

    uint32_t    formatid;
    uint32_t    blocksize;
    uint16_t    audioformat;
    uint16_t    channels;
    uint32_t    rate;
    uint32_t    bytepersec;
    uint16_t    byteperblock;
    uint16_t    bitspersample;
    uint16_t    extrassz;
    uint16_t    newsz;
    uint16_t    ncoeff;
    uint32_t    coef[7];
    uint32_t    factid;
    uint32_t    factsz;
    uint32_t    factdata;
    
    uint32_t    blockid;
    uint32_t    datasize;
} WAVHEADER_ADPCM;

#pragma pack()

#endif //_XWB_H_